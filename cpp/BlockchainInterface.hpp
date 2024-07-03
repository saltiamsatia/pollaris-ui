#pragma once

#include <AbstractTableInterface.hpp>
#include <MutableTransaction.hpp>
#include <BroadcastableTransaction.hpp>

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QMetaEnum>

class BlockchainInterface_Private;

class BlockchainInterface : public QObject {
    Q_OBJECT
    ADD_DNMX

    BlockchainInterface_Private* data;

    // Configuration properties (writeable)
    Q_PROPERTY(QString nodeUrl READ nodeUrl WRITE setNodeUrl NOTIFY nodeUrlChanged)
    Q_PROPERTY(quint32 syncInterval READ syncInterval WRITE setSyncInterval NOTIFY syncIntervalChanged)
    Q_PROPERTY(quint32 syncStaleSeconds READ syncStaleSeconds WRITE setSyncStaleSeconds
               NOTIFY syncStaleSecondsChanged)

    // Status properties (read-only)
    Q_PROPERTY(SyncStatus syncStatus READ syncStatus NOTIFY syncStatusChanged)
    Q_PROPERTY(QString syncStatusString READ syncStatusString NOTIFY syncStatusChanged)
    Q_PROPERTY(QByteArray chainId READ chainId NOTIFY chainIdChanged)
    Q_PROPERTY(QByteArray headBlockId READ headBlockId NOTIFY headBlockChanged)
    Q_PROPERTY(unsigned long headBlockNumber READ headBlockNumber NOTIFY headBlockChanged)
    Q_PROPERTY(unsigned long irreversibleBlockNumber READ irreversibleBlockNumber NOTIFY headBlockChanged)
    Q_PROPERTY(QDateTime headBlockTime READ headBlockTime NOTIFY headBlockChanged)
    Q_PROPERTY(quint64 serverLatency READ serverLatency NOTIFY serverLatencyChanged)

public:
    /*!
     * \enum BlockchainInterface::SyncStatus
     * \brief Specifies the synchronization status of the BlockchainInterface to the EOSIO API node
     *
     * \value Idle Not connected -- indicates URL is not set or valid
     * \value WaitingForConnection A request is out, but hasn't been responded to yet
     * \value RecoveringConnection Connection was up, but began failing -- same as Synchronized, but it's not working
     * \value Connected This state is never actually set, but all values greater than this mean connected
     * \value Synchronized Requests are getting fulfilled, head block is fresh, and journal is tracking
     * \value SynchronizedStale Requests are getting fulfilled, but head block is too old
     */
    enum class SyncStatus {
        Idle = 0, // Not connected -- indicates URL is not set or valid
        WaitingForConnection, // A request is out, but hasn't been responded to yet
        RecoveringConnection, // Connection was up, but began failing -- same as Synchronized, but it's not working
        Connected = 7, // This state is never actually set, but all values greater than this mean connected
        Synchronized, // Requests are getting fulfilled, head block is fresh, and journal is tracking
        SynchronizedStale // Requests are getting fulfilled, but head block is too old
    };
    Q_ENUM(SyncStatus)

public:
    explicit BlockchainInterface(QObject *parent = nullptr);
    virtual ~BlockchainInterface();

    Q_INVOKABLE MutableTransaction* createTransaction() { return new MutableTransaction(this); }
    Q_INVOKABLE AbstractTableInterface* getPollingGroupTable();
    Q_INVOKABLE AbstractTableInterface* getGroupMembersTable(quint64 groupId);

    //! Called to update the scope of a GroupMembers table when it becomes a real table instead of a speculative one
    void rescopeGroupMembersTable(quint64 oldGroup, quint64 newGroup);

    Q_INVOKABLE QNetworkReply* getBlock(unsigned long number);

    SyncStatus syncStatus() const;
    QString syncStatusString() const { return QMetaEnum::fromType<SyncStatus>().valueToKey(int(syncStatus())); }
    QString nodeUrl() const;
    QByteArray headBlockId() const;
    unsigned long headBlockNumber() const;
    unsigned long irreversibleBlockNumber() const;
    QDateTime headBlockTime() const;
    uint32_t syncInterval() const;
    uint32_t syncStaleSeconds() const;
    QByteArray chainId() const;
    quint64 serverLatency() const;

public slots:
    void setNodeUrl(QString nodeUrl);
    void setSyncInterval(uint32_t syncRate);
    void setSyncStaleSeconds(uint32_t syncStaleSeconds);

    void disconnect();
    void connectNow();

    void submitTransaction(BroadcastableTransaction* transaction);

signals:
    // Property change signals
    void syncStatusChanged(SyncStatus syncStatus);
    void nodeUrlChanged(QString nodeUrl);
    void chainIdChanged(QByteArray chainId);
    void headBlockChanged();
    void syncIntervalChanged(uint32_t syncInterval);
    void syncStaleSecondsChanged(uint32_t syncStaleSeconds);
    void serverLatencyChanged(quint64 serverLatency);

    // Signal that node returned an error; errorCode will be an HTTP status, or -1 for protocol unknown, -2 for
    // connection refused, 0 for some other non-HTTP error
    void nodeError(int errorCode);
    // Signal that the node returned a response that seemed invalid
    void nodeResponseNonsense();

    // Signals to tables to refresh their data
    void newJournalEntries(QList<JournalEntry> entries);
    void refreshAllTables();

private:
    void beginSync();
    void processInfoReply(QNetworkReply* reply);
    void processJournalReply(QNetworkReply* reply);
    QNetworkReply* makeCall(QString apiPath, QByteArray json = QByteArrayLiteral("{}"));
    ApiCallback makeApiCaller();
    void connectNetworkReply(QNetworkReply* reply);
    void updateSyncStatus(SyncStatus status);

    template<typename Callable>
    void resetTimer(uint32_t interval, Callable&& task);
    void resetTimer();
};
