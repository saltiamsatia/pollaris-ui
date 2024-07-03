#pragma once

#include <QObject>

#include <BlockchainInterface.hpp>
#include <MutableTransaction.hpp>
#include <SignableTransaction.hpp>
#include <BroadcastableTransaction.hpp>

class KeyManager : public QObject {
    Q_OBJECT

    BlockchainInterface* m_blockchain = nullptr;

public:
    explicit KeyManager(QObject *parent = nullptr);

    BlockchainInterface* blockchain() const { return m_blockchain; }

    /**
     * @brief Prepare the transaction for signing
     * @param transaction   Transaction
     * @return Signable transaction
     * @throws fc::timeout_exception if the preparation exceeds 1 second
     */
    Q_INVOKABLE SignableTransaction* prepareForSigning(MutableTransaction* transaction);
    
    /**
     * @brief Sign the transaction with the contract authority's key
     * @param transaction   Transaction
     */
    Q_INVOKABLE void signTransaction(SignableTransaction* transaction);

    /**
     * @brief Sign the transaction with a given private key
     * @param transaction   Transaction
     * @param privateKey    Private key
     */
    Q_INVOKABLE void signTransaction(SignableTransaction* transaction, QString privateKey);

    /**
     * @brief Prepare the transaction for broadcasting
     * @param transaction   Transaction
     * @return Broadcastable transaction
     * @throws fc::timeout_exception if the preparation exceeds 1 second
     */
    Q_INVOKABLE BroadcastableTransaction* prepareForBroadcast(SignableTransaction* transaction);

    /**
     * @brief Create a new keypair, stored in the wallet, and return the public key
     * @return The public key, base58 encoded, of the new keypair
     */
    Q_INVOKABLE QString createNewKey();

    /**
     * @brief Check if the wallet has the private key corresponding to the provided public key
     * @param publicKey The public key to check
     * @return True if the wallet has the corresponding private key; false otherwise
     */
    Q_INVOKABLE bool hasPrivateKey(QString publicKey);

    /**
     * @brief Get the shared secret between two keys
     * @param foreignKey Public key to get shared secret for
     * @param myKey Public key corresponding to private key in wallet to get shared secret for
     * @return The shared secret
     * @pre hasPrivateKey(myKey) must return true
     */
    Q_INVOKABLE QByteArray getSharedSecret(QString foreignKey, QString myKey);

    /**
     * @brief Check if the provided string is a public key
     * @param maybeKey A string which might be a base58 encoded public key
     * @return Whether the value appears to be a valid public key
     */
    Q_INVOKABLE bool isPublicKey(QString maybeKey);

public slots:
    void setBlockchain(BlockchainInterface* blockchain) {
        if (m_blockchain != blockchain)
            emit blockchainChanged(m_blockchain = blockchain);
    }

signals:
    void blockchainChanged(BlockchainInterface* blockchain);
};

// Dirty crossover function to decode an Action from JSON with FC
class Action;
void decodeAction(QByteArray json, Action* action);
