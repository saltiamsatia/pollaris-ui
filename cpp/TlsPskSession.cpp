#include "TlsPskSession.hpp"

#include <QUrl>
#include <QDebug>

void TlsPskSession::dataReceived() {
    if (socketPtr->canReadLine())
        emit lineReady();
}

void TlsPskSession::socketStateDidChange(QAbstractSocket::SocketState newState) {
    qDebug() << newState;
    if (newState == QAbstractSocket::UnconnectedState)
        emit sessionEnded();
}

TlsPskSession::TlsPskSession(QObject *parent)
    : QObject{parent},
      socketPtr(new QSslSocket(this)) {
    connect(socketPtr, &QSslSocket::readyRead, this, &TlsPskSession::dataReceived);
    connect(socketPtr, &QSslSocket::stateChanged, this, &TlsPskSession::socketStateDidChange);
    connect(socketPtr, &QSslSocket::errorOccurred, this, [](QAbstractSocket::SocketError e) { qDebug() << e; });
    connect(socketPtr, &QSslSocket::sslErrors, this, [](QList<QSslError> errors) { qDebug() << errors; });
    connect(socketPtr, &QSslSocket::encrypted, this, [this] {
        qDebug() << "Handshake completed";
        emit handshakeCompleted();
    });
}

void TlsPskSession::connectToServer(QString host, QString hostKey, QString myKey) {
    if (keyManager() == nullptr) {
        qWarning() << "Cannot connect to TLS server: KeyManager is not set";
        return;
    }
    if (!keyManager()->hasPrivateKey(myKey)) {
        qWarning() << "Cannot connect to TLS server: KeyManager does not have requested key";
        return;
    }

    QUrl url = QUrl::fromUserInput(host);
    qInfo() << "Connecting to host" << url.host() << ":" << url.port() << "[" << hostKey << "] with my key " << myKey;
    socketPtr->setPeerVerifyMode(QSslSocket::VerifyNone);
    socketPtr->connectToHostEncrypted(url.host(), url.port());
    QObject::disconnect(authConnection);
    authConnection = connect(socketPtr, &QSslSocket::preSharedKeyAuthenticationRequired, this,
                             [this, myKey, hostKey](QSslPreSharedKeyAuthenticator* authenticator) mutable {
                                 qDebug() << "Configuring authenticator";
                                 if (hostKey == "hint")
                                     hostKey = QString::fromUtf8(authenticator->identityHint());

                                 authenticator->setIdentity(myKey.toUtf8());
                                 authenticator->setPreSharedKey(keyManager()->getSharedSecret(hostKey, myKey));
    });
}

void TlsPskSession::closeConnection() {
    socketPtr->close();
}

KeyManager* TlsPskSession::keyManager() const {
    return keyManagerPtr;
}

void TlsPskSession::setKeyManager(KeyManager* newKeyManager) {
    if (keyManagerPtr == newKeyManager)
        return;
    keyManagerPtr = newKeyManager;
    emit keyManagerChanged();
}

void TlsPskSession::sendMessage(QString message) {
    socketPtr->write(message.toUtf8());
}

QString TlsPskSession::readLine() {
    if (!socketPtr->canReadLine()) {
        qWarning() << "Unable to read line from TLS socket: a line is not yet available";
        return {};
    }

    return QString::fromUtf8(socketPtr->readLine());
}
