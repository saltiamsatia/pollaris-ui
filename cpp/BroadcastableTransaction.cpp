#include <BroadcastableTransaction.hpp>
#include <BlockchainInterface.hpp>
#include <Strings.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QDebug>

BroadcastableTransaction::BroadcastableTransaction(BlockchainInterface* blockchain, QByteArray id, QJsonObject json)
    : QObject(nullptr), blockchain(blockchain), m_id(id), m_json(json) {
    if (blockchain == nullptr)
        qCritical() << "BroadcastableTransaction created with null blockchain pointer.";
    auto keys = json.keys();
    if (QSet<QString>(keys.begin(), keys.end()) != QSet<QString>{
            QStringLiteral("signatures"),
            QStringLiteral("compression"),
            QStringLiteral("packed_context_free_data"),
            QStringLiteral("packed_trx")})
        qCritical() << "Creating BroadcastableTransaction with invalid JSON:" << json;
}

void BroadcastableTransaction::broadcastFinished(QByteArray response) {
    auto responseObject = QJsonDocument::fromJson(response).object();

    if (responseObject.contains(Strings::Error)) {
        qWarning() << "Transaction failed:" << responseObject;

        emit statusChanged(m_status = TransactionStatus::Failed);

        QString errorMessage;
        if (responseObject.contains(Strings::Error)
                && responseObject[Strings::Error].toObject().contains(Strings::What)) {
            errorMessage = responseObject[Strings::Error].toObject()[Strings::What].toString();
        }
        emit broadcastFailed(errorMessage);

        return;
    }

    if (responseObject.contains(Strings::TransactionId) && responseObject[Strings::TransactionId].toString() != m_id)
        qWarning() << "Broadcast transaction reply has unexpected ID"
                   << responseObject[Strings::TransactionId] << "vs" << m_id;

    if (responseObject.contains(Strings::Processed) &&
            responseObject[Strings::Processed].toObject().contains(Strings::BlockNum)) {
        auto trxBlock = responseObject[Strings::Processed].toObject()[Strings::BlockNum].toVariant().toULongLong();
        qInfo() << "Transaction" << m_id << "confirmed in block" << trxBlock;
        emit statusChanged(m_status = TransactionStatus::Confirmed);
        emit blockNumberChanged(m_blockNumber = trxBlock);
        emit broadcastConfirmed(trxBlock, m_id);
    }

    if (blockchain != nullptr)
        connect(blockchain, &BlockchainInterface::headBlockChanged,
                this, &BroadcastableTransaction::headBlockChanged);
}

void BroadcastableTransaction::headBlockChanged() {
    if (blockchain == nullptr) {
        if (sender() != nullptr && dynamic_cast<BlockchainInterface*>(sender()) != nullptr)
            blockchain = dynamic_cast<BlockchainInterface*>(sender());
        else
            return;
    }

    if (m_status != TransactionStatus::Confirmed) {
        qInfo() << "Disconnecting BroadcastableTransaction from block changes; status is" << m_status;
        disconnect(blockchain, &BlockchainInterface::headBlockChanged,
                   this, &BroadcastableTransaction::headBlockChanged);
        return;
    }

    if (blockchain->headBlockNumber() >= m_blockNumber) {
        // This transaction's block is now said to be irreversible... Fetch it to check that the transaction is in it
        auto reply = blockchain->getBlock(m_blockNumber);
        connect(reply, &QNetworkReply::finished, [this, reply] {
            if (reply->error() != QNetworkReply::NoError)
                return;

            auto response = QJsonDocument::fromJson(reply->readAll()).object();
            if (response.contains(Strings::Transactions) && response[Strings::Transactions].isArray()) {
                auto transactions = response[Strings::Transactions].toArray();
                for (const auto trx : transactions)
                    if (trx.isObject() && trx.toObject().contains(Strings::Trx) &&
                            trx.toObject()[Strings::Trx].isObject()) {
                        auto trxObject = trx.toObject()[Strings::Trx].toObject();
                        if (trxObject.contains(Strings::Id) && trxObject[Strings::Id].isString()) {
                            auto id = trxObject[Strings::Id].toString();
                            if (id == m_id) {
                                emit statusChanged(m_status = TransactionStatus::Irreversible);
                                emit broadcastIrreversible();
                                return;
                            }
                            qWarning() << "BroadcastableTransaction: Transaction not found in irreversible block";
                            emit statusChanged(m_status = TransactionStatus::Unknown);
                            return;
                        }
                    }
            }

            qWarning() << "BroadcastableTransaction: get_block API call response not understood:" << response;
            emit statusChanged(m_status = TransactionStatus::Unknown);
        });
    }
}
