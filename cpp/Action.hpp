#pragma once

#include <Strings.hpp>

#include <QObject>
#include <QJsonObject>
#include <QJSValue>

class BlockchainInterface;

class Action : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString account READ account WRITE setAccount NOTIFY accountChanged)
    QString m_account;
    Q_PROPERTY(QString actionName READ actionName WRITE setActionName NOTIFY actionNameChanged)
    QString m_actionName;
    Q_PROPERTY(QStringList authorizations READ authorizations WRITE setAuthorizations NOTIFY authorizationsChanged)
    QStringList m_authorizations;
    Q_PROPERTY(QJsonObject arguments READ arguments WRITE setArguments NOTIFY argumentsChanged)
    QJsonObject m_arguments;

public:
    explicit Action(QObject* parent = nullptr);

    QString account() const { return m_account; }
    QString actionName() const { return m_actionName; }
    QStringList authorizations() const { return m_authorizations; }
    QJsonObject arguments() const { return m_arguments; }

    static bool validateName(QString action) {
        return Strings::LegalActionArguments.keys().contains(action);
    }
    static bool validateArguments(QString action, QJsonObject arguments) {
        auto keys = arguments.keys();
        QSet<QString> providedArguments(keys.begin(), keys.end());
        return Strings::LegalActionArguments[action] == providedArguments;
    }

    Q_INVOKABLE QString describe(int elideAfter = 25, BlockchainInterface* blockchain = nullptr,
                                 QJSValue cbOnRefereshed = QJSValue::NullValue);

public slots:
    void setAccount(QString account) {
        if (m_account == account)
            return;

        m_account = account;
        emit accountChanged(m_account);
    }
    void setActionName(QString actionName) {
        if (m_actionName == actionName)
            return;

        m_actionName = actionName;
        emit actionNameChanged(m_actionName);
    }
    void setAuthorizations(QStringList authorizations) {
        if (m_authorizations == authorizations)
            return;

        m_authorizations = authorizations;
        emit authorizationsChanged(m_authorizations);
    }
    void setArguments(QJsonObject arguments) {
        if (m_arguments == arguments)
            return;

        m_arguments = arguments;
        emit argumentsChanged(m_arguments);
    }

    void loadJson(QJsonObject json);

signals:
    void accountChanged(QString account);
    void actionNameChanged(QString actionName);
    void authorizationsChanged(QStringList authorizations);
    void argumentsChanged(QJsonObject arguments);
};
