#pragma once

#include <Dnmx.hpp>

#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <QDateTime>

class SignableTransaction : public QObject {
    Q_OBJECT

    ADD_DNMX

    Q_PROPERTY(QJsonObject json READ json NOTIFY jsonChanged)
    QJsonObject m_json;

public:
    // Does not take a parent pointer: lifetime is managed by QML
    explicit SignableTransaction(QJsonObject json);
    virtual ~SignableTransaction() { qInfo(__FUNCTION__); }

    QJsonObject json() const { return m_json; }

    void setExpiration(QDateTime expiration) {
        auto expirationString = expiration.toString(Qt::ISODate);
        // Work around FC bug that doesn't allow Zulu time designator on ISO date
        if (expirationString.back() == 'Z')
            expirationString.chop(1);
        m_json["expiration"] = expirationString;
    }
    void addSignature(QString signature) {
        auto signatures = m_json["signatures"];
        auto sigsArray = signatures.toArray() += signature;
        signatures = sigsArray;
        emit jsonChanged(m_json);
    }

signals:
    void jsonChanged(QJsonObject json);
};
