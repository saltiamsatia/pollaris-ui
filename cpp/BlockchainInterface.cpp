#include <BlockchainInterface.hpp>
#include <Tables.hpp>

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimeZone>
#include <QTimer>
#include <QQmlEngine>

// Private data class
class BlockchainInterface_Private {
public:
    QUrl nodeUrl;
    QByteArray chainId;
    QByteArray headBlockId;
    unsigned long headBlockNumber = 0;
    unsigned long irreversibleBlockNumber = 0;
    QDateTime headBlockTime;
    uint64_t serverLatency = 0;
    QNetworkAccessManager* network;
    BlockchainInterface::SyncStatus syncStatus = BlockchainInterface::SyncStatus::Idle;
    uint32_t syncInterval = 2500;
    uint32_t syncStaleSeconds = 10;

    QTimer* syncTimer = nullptr;
    JournalEntry lastJournalEntry;

    PollingGroupsTable* pollingGroupTable = nullptr;
    QMap<uint64_t, GroupMembersTable*> groupAccountsTables;
};

// Constructor & destructor
BlockchainInterface::BlockchainInterface(QObject *parent) : QObject(parent), data(new BlockchainInterface_Private()) {
    data->network = new QNetworkAccessManager(this);
    connect(this, &BlockchainInterface::nodeUrlChanged, &BlockchainInterface::connectNow);
}

BlockchainInterface::~BlockchainInterface() {
    disconnect();
    delete data;
}

// Blockchain access methods
AbstractTableInterface* BlockchainInterface::getPollingGroupTable() {
    if (data->pollingGroupTable != nullptr)
        return data->pollingGroupTable;

    auto scope = eosio::string_to_uint64_t(Strings::Global);
    auto table = data->pollingGroupTable = new PollingGroupsTable(this, makeApiCaller(), scope);
    connect(table, &QObject::destroyed, this, [this] { data->pollingGroupTable = nullptr; });
    connect(this, &BlockchainInterface::refreshAllTables, table, &AbstractTableInterface::fullRefresh);
    connect(this, &BlockchainInterface::newJournalEntries, table, &AbstractTableInterface::processJournal);
    return table;
}

AbstractTableInterface* BlockchainInterface::getGroupMembersTable(quint64 groupId) {
    if (data->groupAccountsTables.contains(groupId))
        return data->groupAccountsTables[groupId];

    auto table = data->groupAccountsTables[groupId] = new GroupMembersTable(this, makeApiCaller(), groupId);
    connect(table, &QObject::destroyed, this, [this, groupId] { data->groupAccountsTables.remove(groupId); });
    connect(this, &BlockchainInterface::refreshAllTables, table, &AbstractTableInterface::fullRefresh);
    connect(this, &BlockchainInterface::newJournalEntries, table, &AbstractTableInterface::processJournal);
    return table;
}

void BlockchainInterface::rescopeGroupMembersTable(quint64 oldGroup, quint64 newGroup) {
    if (data->groupAccountsTables.contains(oldGroup)) {
        auto table = data->groupAccountsTables.take(oldGroup);
        table->updateScope(QString::number(newGroup));

        if (data->groupAccountsTables.contains(newGroup)) {
            qWarning() << "BlockchainInterface: Rescoping GroupMembers table" << oldGroup << "to" << newGroup
                       << "but a table with the new scope already exists. The old table will be orphaned.";
            // Orphan the old table
            table->setParent(nullptr);
            QQmlEngine::setObjectOwnership(table, QQmlEngine::JavaScriptOwnership);
            return;
        }

        qInfo() << "BlockchainInterface: Rescoping GroupMembers table from" << oldGroup << "to" << newGroup;
        data->groupAccountsTables.insert(newGroup, table);
    }
}

QNetworkReply* BlockchainInterface::getBlock(unsigned long number) {
    auto reply = makeCall(Strings::GetBlock,
                          QStringLiteral("{\"%1\": %2}").arg(Strings::BlockNumOrId,
                                                             QString::number(number)).toLocal8Bit());
    connectNetworkReply(reply);
    return reply;
}

// Getters
BlockchainInterface::SyncStatus BlockchainInterface::syncStatus() const { return data->syncStatus; }
QString BlockchainInterface::nodeUrl() const { return data->nodeUrl.toString(); }
QByteArray BlockchainInterface::headBlockId() const { return data->headBlockId; }
unsigned long BlockchainInterface::headBlockNumber() const { return data->headBlockNumber; }
unsigned long BlockchainInterface::irreversibleBlockNumber() const { return data->irreversibleBlockNumber; }
QDateTime BlockchainInterface::headBlockTime() const { return data->headBlockTime; }
uint32_t BlockchainInterface::syncInterval() const { return data->syncInterval; }
uint32_t BlockchainInterface::syncStaleSeconds() const { return data->syncStaleSeconds; }
QByteArray BlockchainInterface::chainId() const { return data->chainId; }
quint64 BlockchainInterface::serverLatency() const { return data->serverLatency; }

// Setters
void BlockchainInterface::setNodeUrl(QString nodeUrl) {
    auto url = QUrl::fromUserInput(nodeUrl);
    if (data->nodeUrl == url)
        return;
    data->nodeUrl = url;
    emit nodeUrlChanged(data->nodeUrl.toString());
}
void BlockchainInterface::setSyncInterval(uint32_t syncRate) {
    if (data->syncInterval == syncRate)
        return;
    emit syncIntervalChanged(data->syncInterval = syncRate);
}
void BlockchainInterface::setSyncStaleSeconds(uint32_t syncStaleSeconds) {
    if (data->syncStaleSeconds == syncStaleSeconds)
        return;
    emit syncStaleSecondsChanged(data->syncStaleSeconds = syncStaleSeconds);
}


// Business logic
void BlockchainInterface::disconnect() {
    // Update status
    if (data->syncStatus != SyncStatus::Idle) {
        qInfo() << "BlockchainInterface: Disconnecting";
        emit syncStatusChanged(data->syncStatus = SyncStatus::Idle);
    }

    resetTimer();
}

void BlockchainInterface::connectNow() {
    // Sanity check URL
    if (data->nodeUrl.isEmpty())
        return;
    if (!data->nodeUrl.isValid()) {
        qWarning() << "BlockchainInterface: Asked to connect, but invalid URL set:" << data->nodeUrl;
        return;
    }

    // Abort any pending requests
    disconnect();
    // Update status
    qInfo() << "BlockchainInterface: Connecting to" << data->nodeUrl;
    emit syncStatusChanged(data->syncStatus = SyncStatus::WaitingForConnection);

    // Send request for chain info
    beginSync();
    // Schedule next sync
    resetTimer(data->syncInterval, [this] { beginSync(); });
}

void BlockchainInterface::submitTransaction(BroadcastableTransaction* transaction) {
    if (transaction == nullptr) {
        qCritical() << "Asked to broadcast transaction, but transaction is nullptr!";
        return;
    }

    auto reply = makeCall("/v1/chain/push_transaction", QJsonDocument(transaction->json()).toJson());
    connect(reply, &QNetworkReply::finished,
            [transaction, reply] { transaction->broadcastFinished(reply->readAll()); });
}

void BlockchainInterface::beginSync() {
    auto* reply = makeCall(Strings::GetInfo);
    connect(reply, &QNetworkReply::finished, [this, reply] { processInfoReply(reply); });
    connectNetworkReply(reply);

    // Have we already connected and processed journal entries?
    if (data->lastJournalEntry.isValid())
        // Yes, so get all entries after the last we've seen
        reply = makeCall(Strings::GetTableRows, getTableJson(Strings::Journal, Strings::Global,
                                                             QString::number(data->lastJournalEntry.id+1)));
    else
        // No, so just get the last journal entry
        reply = makeCall(Strings::GetTableRows,
                         getTableJson(Strings::Journal, Strings::Global,
                                      QString::number(data->lastJournalEntry.id), 1, true));
    connect(reply, &QNetworkReply::finished, [this, reply] { processJournalReply(reply); });
}

void BlockchainInterface::processInfoReply(QNetworkReply* reply) {
    // Check that reply is successful
    if (reply->error() != QNetworkReply::NoError)
        return;

    // Sanity check response
    auto jsonDoc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject response;
    // Check JSON is an object and assign it to response while checking the object contains a "head_block_id" field
    if (!jsonDoc.isObject() || !(response = jsonDoc.object()).contains(Strings::HeadBlockId)) {
        qWarning() << "BlockchainInterface: Error: get_info response not sensible:" << jsonDoc;
        emit nodeResponseNonsense();
        updateSyncStatus(SyncStatus::RecoveringConnection);
        return;
    }

    // Update properties
    auto chainId = response[Strings::ChainId].toString().toLocal8Bit();
    if (chainId != data->chainId)
        emit chainIdChanged(data->chainId = chainId);
    data->headBlockId = response[Strings::HeadBlockId].toString().toLocal8Bit();
    data->headBlockNumber = response[Strings::HeadBlockNum].toVariant().toULongLong();
    data->irreversibleBlockNumber = response[Strings::LastIrreversibleBlockNum].toVariant().toULongLong();
    data->headBlockTime = QDateTime::fromString(response[Strings::HeadBlockTime].toString(), Qt::DateFormat::ISODate);
    data->headBlockTime.setTimeZone(QTimeZone::utc());
    emit headBlockChanged();
}

void BlockchainInterface::processJournalReply(QNetworkReply* reply) {
    // Check that reply is successful
    if (reply->error() != QNetworkReply::NoError)
        return;

    // Sanity check response and parse response to a JSON array
    auto jsonDoc = QJsonDocument::fromJson(reply->readAll());
    auto rows = parseRows(jsonDoc);
    if (!rows.has_value()) {
        qWarning() << "BlockchainInterface: Error: response to request for journal not sensible:" << jsonDoc;
        emit nodeResponseNonsense();
        updateSyncStatus(SyncStatus::RecoveringConnection);
        return;
    }

    // Parse JSON array to a list of JournalEntries
    if (rows.value().isEmpty())
        // No news
        return;
    auto entries = JournalEntry::fromJsonArray(rows.value());

    // If we've been syncing the journal, notify of these new entries
    if (data->lastJournalEntry.isValid() && entries.first().id == data->lastJournalEntry.id + 1)
        emit newJournalEntries(entries);
    else
        // Either we haven't been syncing or there was a break in the journal,
        // so if any tables are already out, fully refresh them now
        emit refreshAllTables();

    // Update the latest journal entry
    data->lastJournalEntry = entries.last();
    qInfo() << "BlockchainInterface: Synchronized journal through entry" << data->lastJournalEntry.id;
}

QNetworkReply* BlockchainInterface::makeCall(QString apiPath, QByteArray json) {
    // Create request and set headers
    QNetworkRequest request(data->nodeUrl.toString() + apiPath);
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("Pollaris Alpha"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    request.setHeader(QNetworkRequest::ContentLengthHeader, json.length());

    // POST the request
    auto* reply = data->network->post(request, json);
    reply->setProperty("request-content", json);
    reply->setProperty("time-sent", QDateTime::currentMSecsSinceEpoch());

    // Schedule RTT recording immediately so it's the first slot to run
    QObject::connect(reply, &QNetworkReply::finished, [this, reply] {
        auto now = QDateTime::currentMSecsSinceEpoch();
        auto timeSent = reply->property("time-sent");
        if (!timeSent.isNull()) {
            auto rtt = now - timeSent.toULongLong();
            reply->setProperty("rtt", rtt);
            if (rtt != data->serverLatency)
                emit serverLatencyChanged(data->serverLatency = rtt);
        }
    });
    return reply;
}

ApiCallback BlockchainInterface::makeApiCaller() {
    return [this](QString apiPath, QByteArray json) {
        auto reply = makeCall(apiPath, json);
        connectNetworkReply(reply);
        return reply;
    };
}

void BlockchainInterface::connectNetworkReply(QNetworkReply* reply) {
    // If reply succeeds, go to synchronized status
    connect(reply, &QNetworkReply::finished, [this, reply] {

        if (reply->error() != QNetworkReply::NoError)
            return;

        // If we weren't previously connected, log the state change
        if (data->syncStatus < SyncStatus::Connected) {
            auto rtt = reply->property("rtt");
            qInfo() << "BlockchainInterface: Connected to node with RTT" << rtt << "ms";
        }
        // Check if head block is stale and go to appropriate synchronized state
        auto now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
        bool stale = (now - data->headBlockTime.toSecsSinceEpoch()) > 10;
        if (stale)
            updateSyncStatus(SyncStatus::SynchronizedStale);
        else
            updateSyncStatus(SyncStatus::Synchronized);
    });
    // If reply errors, log it and go to recovery status
    connect(reply, &QNetworkReply::errorOccurred, [this, reply](QNetworkReply::NetworkError error) {
        qWarning() << "BlockchainInterface: Network error:" << error;
        qWarning() << "The request content was:" << reply->property("request-content");
        if (reply->bytesAvailable())
            qWarning() << "The error content is:" << reply->readAll();

        if (error == QNetworkReply::ProtocolUnknownError)
            emit nodeError(-1);
        else if (error == QNetworkReply::ConnectionRefusedError)
            emit nodeError(-2);
        else
            emit nodeError(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());

        if (data->syncStatus > SyncStatus::Connected)
            updateSyncStatus(SyncStatus::RecoveringConnection);
        else
            updateSyncStatus(SyncStatus::WaitingForConnection);
    });
}

void BlockchainInterface::updateSyncStatus(BlockchainInterface::SyncStatus status) {
    if (status != data->syncStatus)
        emit syncStatusChanged(data->syncStatus = status);
}

template<typename Callable>
void BlockchainInterface::resetTimer(uint32_t interval, Callable&& task) {
    resetTimer();
    data->syncTimer = new QTimer(this);
    data->syncTimer->callOnTimeout(std::forward<Callable>(task));
    data->syncTimer->setInterval(interval);
    data->syncTimer->start();
}

void BlockchainInterface::resetTimer() {
    if (data->syncTimer != nullptr) {
        data->syncTimer->stop();
        data->syncTimer->deleteLater();
        data->syncTimer = nullptr;
    }
}

JournalEntry::JournalEntry(QJsonObject json)
    : id(json[Strings::Id].toVariant().toULongLong()),
      timestamp(QDateTime::fromString(json[Strings::Timestamp].toString(), Qt::DateFormat::ISODate)),
      table(json[Strings::Table].toString()),
      scope(json[Strings::Scope].toVariant().toULongLong()),
      key(json[Strings::Key].toVariant().toULongLong()),
      type(EntryType(json[Strings::Modification].toInt())) {
    timestamp.setTimeZone(QTimeZone::utc());
}

QList<JournalEntry> JournalEntry::fromJsonArray(QJsonArray array) {
    QList<JournalEntry> result;
    result.reserve(array.size());
    std::transform(array.begin(), array.end(), std::back_inserter(result),
                   [](QJsonValueRef value) { return value.toObject(); });
    return result;
}
