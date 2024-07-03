#include <MutableTransaction.hpp>
#include <BlockchainInterface.hpp>
#include <Action.hpp>

#include <QQmlEngine>

MutableTransaction::MutableTransaction(BlockchainInterface* blockchain) : QObject(nullptr), blockchain(blockchain) {
    // Transaction lifetime is managed by QML.
    QQmlEngine::setObjectOwnership(this, QQmlEngine::JavaScriptOwnership);
    if (blockchain == nullptr)
        qCritical("MutableTransaction created with a nullptr for BlockchainInterface! This won't end well.");
}

void MutableTransaction::setActions(QList<QObject*> actions) {
    auto end = std::remove_if(actions.begin(), actions.end(), [](QObject* o) { return o == nullptr; });
    if (end == actions.begin() && !actions.isEmpty()) {
        qWarning() << "Ignoring request to set actions to list of nullptrs";
        return;
    }
    if (end != actions.end())
        actions.erase(end, actions.end());

    if (m_actions != actions) {
        // Reparent the actions to this
        std::for_each(actions.begin(), actions.end(), [this](QObject* a) { a->setParent(this); });
        emit actionsChanged(m_actions = actions);
    }
}

void MutableTransaction::addAction(QString actionName, QJsonObject arguments) {
    // Check action name is valid
    if (!Action::validateName(actionName)) {
        qDebug() << "Asked to add action to transaction, but action name is unkown:" << actionName;
        return;
    }
    // Check argument names are valid
    if (!Action::validateArguments(actionName, arguments)) {
        qDebug() << "Asked to add" << actionName << "action to transaction, but provided arguments"
                 << arguments.keys() << "do not match action's arguments"
                 << Strings::LegalActionArguments[actionName];
        return;
    }

    auto* action = new Action(this);
    action->setAccount(Strings::Contract_name);
    action->setActionName(actionName);
    action->setArguments(arguments);
    // If action is DCSN_SET, authorization is voter@active; otherwise, it's fmv@active
    if (actionName == Strings::DcsnSet)
        action->setAuthorizations({Strings::AuthorizationTemplate.arg(arguments[Strings::VoterName].toString(),
                                                                      Strings::Active)});
    else
        action->setAuthorizations({Strings::AuthorizationTemplate.arg(Strings::Contract_name, Strings::Active)});
    m_actions.append(action);
    emit actionsChanged(m_actions);
}

void MutableTransaction::deleteAction(int index) {
    if (index >= 0 && index < m_actions.length()) {
        m_actions.takeAt(index)->deleteLater();
        emit actionsChanged(m_actions);
    }
}
