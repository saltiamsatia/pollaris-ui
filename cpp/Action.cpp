#include <Action.hpp>
#include <Strings.hpp>
#include <BlockchainInterface.hpp>
#include <KeyManager.hpp>

#include <QJsonDocument>

Action::Action(QObject* parent) : QObject(parent) {}

/*!
 * \brief Describe the action in human-readable text
 * \param elideAfter The max size of interpolated values before they are elided to fit
 * \param blockchain If provided, look up IDs to show name strings instead
 * \param cbOnRefereshed If callable, it is invoked passing updated description once name strings are loaded
 * \return A human-readable, rich formatted string describing the action
 */
QString Action::describe(int elideAfter, BlockchainInterface* blockchain, QJSValue cbOnRefereshed) {
    if (!validateArguments(m_actionName, m_arguments))
        return tr("Invalid action");

    auto elide = [elideAfter](QString in) {
        if (in.length() > elideAfter) {
            auto cutSize = elideAfter / 2;
            return in.left(cutSize) + "..." + in.right(cutSize);
        }
        return in;
    };

    if (m_actionName == Strings::VoterAdd)
        return tr("Add voter <b>%1</b> to polling group <b>%2</b>")
                .arg(elide(m_arguments[Strings::Voter].toString()),
                     elide(m_arguments[Strings::GroupName].toString()));
    if (m_actionName == Strings::VoterRemove)
        return tr("Remove voter <b>%1</b> from polling group <b>%2</b>")
                .arg(elide(m_arguments[Strings::Voter].toString()),
                     elide(m_arguments[Strings::GroupName].toString()));
    if (m_actionName == Strings::GroupCopy)
        return tr("Copy polling group <b>%1</b> naming the new group <b>%2</b>")
                .arg(elide(m_arguments[Strings::GroupName].toString()),
                     elide(m_arguments[Strings::NewName].toString()));
    if (m_actionName == Strings::GroupRename)
        return tr("Rename polling group <b>%1</b> to <b>%2</b>")
                .arg(elide(m_arguments[Strings::GroupName].toString()),
                     elide(m_arguments[Strings::NewName].toString()));
    if (m_actionName == Strings::CntstNew)
        return tr("Create new contest for polling group <b>ID %1</b> with name <b>%2</b>")
                .arg(m_arguments[Strings::GroupId].toString(), m_arguments[Strings::Name].toString());
    // TODO: Lookup names corresponding to IDs and callback with updated value
    if (m_actionName == Strings::CntstModify)
        return tr("Modify contest <b>ID %1</b> in polling group <b>ID %2</b>")
                .arg(m_arguments[Strings::ContestId].toString(), m_arguments[Strings::GroupId].toString());
    if (m_actionName == Strings::CntstTally)
        return tr("Tally contest <b>ID %1</b> in polling group <b>ID %2</b>")
                .arg(m_arguments[Strings::ContestId].toString(), m_arguments[Strings::GroupId].toString());
    if (m_actionName == Strings::CntstDelete)
        return tr("Delete contest <b>ID %1</b> from polling group <b>ID %2</b>")
                .arg(m_arguments[Strings::ContestId].toString(), m_arguments[Strings::GroupId].toString());
    if (m_actionName == Strings::DcsnSet)
        return tr("Set decision on contest <b>ID %1</b> in polling group <b>ID %2</b> for voter <b>ID %3</b>")
                .arg(m_arguments[Strings::ContestId].toString(),
                     m_arguments[Strings::GroupId].toString(),
                     m_arguments[Strings::VoterName].toString());
    return tr("Unknown action");
}

void Action::loadJson(QJsonObject json) {
    decodeAction(QJsonDocument(json).toJson(), this);
}
