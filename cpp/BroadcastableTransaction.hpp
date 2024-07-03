#pragma once

#include <Enums.hpp>
#include <Dnmx.hpp>

#include <QObject>
#include <QJsonObject>

class BlockchainInterface;

class BroadcastableTransaction : public QObject {
    Q_OBJECT

    ADD_DNMX

    BlockchainInterface* blockchain = nullptr;

    Q_PROPERTY(QByteArray id READ id CONSTANT)
    QByteArray m_id;
    Q_PROPERTY(QJsonObject json READ json CONSTANT)
    QJsonObject m_json;
    Q_PROPERTY(TransactionStatus status READ status NOTIFY statusChanged)
    TransactionStatus m_status = TransactionStatus::Pending;
    Q_PROPERTY(unsigned long blockNumber READ blockNumber NOTIFY blockNumberChanged)
    unsigned long m_blockNumber = 0;

public:
    // Does not take a parent pointer: lifetime is managed by QML
    explicit BroadcastableTransaction(BlockchainInterface* blockchain, QByteArray id, QJsonObject json);
    virtual ~BroadcastableTransaction() { qInfo(__FUNCTION__); }

    QJsonObject json() const { return m_json; }
    QByteArray id() const { return m_id; }
    TransactionStatus status() const { return m_status; }
    uint64_t blockNumber() const { return m_blockNumber; }

    // Called by BlockchainInterface when the reply to the broadcast API call is received
    void broadcastFinished(QByteArray response);

signals:
    void statusChanged(TransactionStatus status);
    void blockNumberChanged(uint64_t blockNumber);

    /**
     * @brief Notification that the broadcast was confirmed on the blockchain.
     *      Although confirmed, the transaction could be reversed depending on potential
     *      forking of the blockchain.  The transaction will finally be finalized
     *      upon notification of its irreversible status.
     * @see broadcastIrreversible()
     */
    void broadcastConfirmed(const qulonglong& blockNumber, const QString& transactionId);

    /**
     * @brief Notification that the broadcast was confirmed and irreversible on the blockchain
     */
    void broadcastIrreversible();

    /**
     * @brief Notification that the broadcast failed
     * @param failureReason Description of the failure reason
     */
    void broadcastFailed(const QString& failureReason);

private slots:
    void headBlockChanged();
};
