#pragma once

#include <Strings.hpp>
#include <Dnmx.hpp>

#include <QObject>
#include <QDateTime>

class BlockchainInterface;

class MutableTransaction : public QObject {
    Q_OBJECT

    BlockchainInterface* blockchain;

    ADD_DNMX

    Q_PROPERTY(QDateTime expiration READ expiration WRITE setExpiration NOTIFY expirationChanged)
    QDateTime m_expiration;
    Q_PROPERTY(QByteArray refBlockId READ refBlockId WRITE setRefBlockId NOTIFY refBlockIdChanged)
    QByteArray m_refBlockId = 0;
    Q_PROPERTY(uint64_t maxNetWords READ maxNetWords WRITE setMaxNetWords NOTIFY maxNetWordsChanged)
    uint64_t m_maxNetWords = 0;
    Q_PROPERTY(uint8_t maxCpuMs READ maxCpuMs WRITE setMaxCpuMs NOTIFY maxCpuMsChanged)
    uint8_t m_maxCpuMs = 0;
    Q_PROPERTY(uint64_t delaySeconds READ delaySeconds WRITE setDelaySeconds NOTIFY delaySecondsChanged)
    uint64_t m_delaySeconds = 0;
    Q_PROPERTY(QList<QObject*> actions READ actions WRITE setActions NOTIFY actionsChanged)
    QList<QObject*> m_actions;

public:
    explicit MutableTransaction(BlockchainInterface* blockchain);
    virtual ~MutableTransaction() { qInfo(__FUNCTION__); }

    QList<QObject*> actions() const { return m_actions; }
    QDateTime expiration() const { return m_expiration; }
    QByteArray refBlockId() const { return m_refBlockId; }
    uint64_t maxNetWords() const { return m_maxNetWords; }
    uint8_t maxCpuMs() const { return m_maxCpuMs; }
    uint64_t delaySeconds() const { return m_delaySeconds; }

public slots:
    void setActions(QList<QObject*> actions);
    void setExpiration(QDateTime expiration) {
        if (m_expiration != expiration)
            emit expirationChanged(m_expiration = expiration);
    }
    void setRefBlockId(QByteArray refBlockId) {
        if (m_refBlockId != refBlockId)
            emit refBlockIdChanged(m_refBlockId = refBlockId);
    }
    void setMaxNetWords(uint64_t maxNetWords) {
        if (m_maxNetWords != maxNetWords)
            emit maxNetWordsChanged(m_maxNetWords = maxNetWords);
    }
    void setMaxCpuMs(uint8_t maxCpuMs) {
        if (m_maxCpuMs != maxCpuMs)
            emit maxCpuMsChanged(m_maxCpuMs = maxCpuMs);
    }
    void setDelaySeconds(uint64_t delaySeconds) {
        if (m_delaySeconds != delaySeconds)
            emit delaySecondsChanged(m_delaySeconds = delaySeconds);
    }
    void addAction(QString actionName, QJsonObject arguments);
    void deleteAction(int index);

signals:
    void actionsChanged(QList<QObject*> actions);
    void expirationChanged(QDateTime expiration);
    void refBlockIdChanged(QByteArray refBlockId);
    void maxNetWordsChanged(uint64_t maxNetWords);
    void maxCpuMsChanged(uint8_t maxCpuMs);
    void delaySecondsChanged(uint64_t delaySeconds);
};
