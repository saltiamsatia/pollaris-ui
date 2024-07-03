#pragma once

#include "KeyManager.hpp"

#include <QObject>
#include <QSslSocket>
#include <QQmlEngine>

class TlsPskSession : public QObject {
    Q_OBJECT
    QML_ELEMENT

    QSslSocket* socketPtr;
    Q_PROPERTY(QSslSocket* socket READ socket CONSTANT)
    KeyManager* keyManagerPtr;
    Q_PROPERTY(KeyManager* keyManager READ keyManager WRITE setKeyManager NOTIFY keyManagerChanged)

    QMetaObject::Connection authConnection;

private slots:
    /// @brief Process data received on the socket, and perhaps emit lineReady()
    void dataReceived();
    /// @brief Process a change in the socket's state
    void socketStateDidChange(QSslSocket::SocketState newState);

public:
    explicit TlsPskSession(QObject *parent = nullptr);

    QSslSocket* socket() const { return socketPtr; }

    KeyManager* keyManager() const;
    void setKeyManager(KeyManager* newKeyManager);

    /**
     * @brief Send a message to the server
     * @param message The message to send
     */
    Q_INVOKABLE void sendMessage(QString message);

    Q_INVOKABLE QString readLine();

public slots:
    /**
     * @brief Connect to a server using the shared secret between hostKey and myKey as the PSK
     * @param host The host to connect to
     * @param hostKey The host's public key, base58 encoded, or "hint" to expect the host's key in the identity hint
     * @param myKey Our public key, base58 encoded. KeyManager must have corresponding private key.
     * @pre KeyManager must be set
     */
    void connectToServer(QString host, QString hostKey, QString myKey);

    /**
     * @brief Disconnect from the server
     */
    void closeConnection();

signals:
    void keyManagerChanged();
    /// @brief Emitted when an ASCII line is available to read
    void lineReady();
    /// @brief Emitted when the TLS handshake completes
    void handshakeCompleted();
    /// @brief Emitted when the session ends, either normally or due to error
    void sessionEnded();
};

