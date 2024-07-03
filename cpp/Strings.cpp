#include <Strings.hpp>

#include <QVariantMap>

const QString Strings::AssistantName = QStringLiteral("Así");

const QString Strings::GetTableRows = QStringLiteral("/v1/chain/get_table_rows");
const QString Strings::GetInfo = QStringLiteral("/v1/chain/get_info");
const QString Strings::GetBlock = QStringLiteral("/v1/chain/get_block");

const QString Strings::UnknownTable = QStringLiteral("Unknown Table");
const QString Strings::PollGroups = QStringLiteral("poll.groups");
const QString Strings::GroupAccts = QStringLiteral("group.accts");
const QString Strings::Journal = QStringLiteral("journal");

const QString Strings::Global = QStringLiteral("global");
const QString Strings::Rows = QStringLiteral("rows");
const QString Strings::More = QStringLiteral("more");
const QString Strings::NextKey = QStringLiteral("next_key");
const QString Strings::ChainId = QStringLiteral("chain_id");
const QString Strings::HeadBlockId = QStringLiteral("head_block_id");
const QString Strings::HeadBlockNum = QStringLiteral("head_block_num");
const QString Strings::HeadBlockTime = QStringLiteral("head_block_time");
const QString Strings::Id = QStringLiteral("id");
const QString Strings::Name = QStringLiteral("name");
const QString Strings::Tags = QStringLiteral("tags");
const QString Strings::Timestamp = QStringLiteral("timestamp");
const QString Strings::Table = QStringLiteral("table");
const QString Strings::Scope = QStringLiteral("scope");
const QString Strings::Key = QStringLiteral("key");
const QString Strings::Modification = QStringLiteral("modification");
const QString Strings::Contract_name = QStringLiteral("fmv");
const QString Strings::Active = QStringLiteral("active");
const QString Strings::AuthorizationTemplate = QStringLiteral("%1@%2");
const QString Strings::TransactionId = QStringLiteral("transaction_id");
const QString Strings::Error = QStringLiteral("error");
const QString Strings::What = QStringLiteral("what");
const QString Strings::Processed = QStringLiteral("processed");
const QString Strings::BlockNum = QStringLiteral("block_num");
const QString Strings::LastIrreversibleBlockNum = QStringLiteral("last_irreversible_block_num");
const QString Strings::Transactions = QStringLiteral("transactions");
const QString Strings::Trx = QStringLiteral("trx");
const QString Strings::DraftId = QStringLiteral("DRAFT_ID");
const QString Strings::Deleted = QStringLiteral("DELETED");
const QString Strings::Account = QStringLiteral("account");

const QString Strings::VoterAdd = QStringLiteral("voter.add");
const QString Strings::VoterRemove = QStringLiteral("voter.remove");
const QString Strings::GroupCopy = QStringLiteral("group.copy");
const QString Strings::GroupRename = QStringLiteral("group.rename");
const QString Strings::CntstNew = QStringLiteral("cntst.new");
const QString Strings::CntstModify = QStringLiteral("cntst.modify");
const QString Strings::CntstTally = QStringLiteral("cntst.tally");
const QString Strings::CntstDelete = QStringLiteral("cntst.delete");
const QString Strings::DcsnSet = QStringLiteral("dcsn.set");

const QString Strings::GroupName = QStringLiteral("groupName");
const QString Strings::Voter = QStringLiteral("voter");
const QString Strings::Weight = QStringLiteral("weight");
const QString Strings::NewName = QStringLiteral("newName");
const QString Strings::GroupId = QStringLiteral("groupId");
const QString Strings::Description = QStringLiteral("description");
const QString Strings::Contestants = QStringLiteral("contestants");
const QString Strings::Begin = QStringLiteral("begin");
const QString Strings::End = QStringLiteral("end");
const QString Strings::ContestId = QStringLiteral("contestId");
const QString Strings::NewDescription = QStringLiteral("newDescription");
const QString Strings::NewTags = QStringLiteral("newTags");
const QString Strings::DeleteContestants = QStringLiteral("deleteContestants");
const QString Strings::AddContestants = QStringLiteral("addContestants");
const QString Strings::NewBegin = QStringLiteral("newBegin");
const QString Strings::NewEnd = QStringLiteral("newEnd");
const QString Strings::VoterName = QStringLiteral("voterName");
const QString Strings::Opinions = QStringLiteral("opinions");
const QString Strings::BlockNumOrId = QStringLiteral("block_num_or_id");

const QSet<QString> Strings::VoterAddArgs{GroupName, Voter, Weight, Tags};
const QSet<QString> Strings::VoterRemoveArgs{GroupName, Voter};
const QSet<QString> Strings::GroupCopyArgs{GroupName, NewName};
const QSet<QString> Strings::GroupRenameArgs{GroupName, NewName};
const QSet<QString> Strings::CntstNewArgs{GroupId, Name, Description, Contestants, Begin, End, Tags};
const QSet<QString> Strings::CntstModifyArgs{GroupId, ContestId, NewName, NewDescription, NewTags,
                                             DeleteContestants, AddContestants, NewBegin, NewEnd};
const QSet<QString> Strings::CntstTallyArgs{GroupId, ContestId};
const QSet<QString> Strings::CntstDeleteArgs{GroupId, ContestId};
const QSet<QString> Strings::DcsnSetArgs{GroupId, ContestId, VoterName, Opinions, Tags};

const QMap<QString, QSet<QString>> Strings::LegalActionArguments{
    {VoterAdd, VoterAddArgs},
    {VoterRemove, VoterRemoveArgs},
    {GroupCopy, GroupCopyArgs},
    {GroupRename, GroupRenameArgs},
    {CntstNew, CntstNewArgs},
    {CntstModify, CntstModifyArgs},
    {CntstTally, CntstTallyArgs},
    {CntstDelete, CntstDeleteArgs},
    {DcsnSet, DcsnSetArgs}
};

const QVariantMap Strings::MapOfAll = {
    {QStringLiteral("AssistantName"), AssistantName},
    {QStringLiteral("GetTableRows"), GetTableRows},
    {QStringLiteral("GetInfo"), GetInfo},
    {QStringLiteral("GetBlock"), GetBlock},
    {QStringLiteral("UnknownTable"), UnknownTable},
    {QStringLiteral("PollGroups"), PollGroups},
    {QStringLiteral("GroupAccts"), GroupAccts},
    {QStringLiteral("Journal"), Journal},
    {QStringLiteral("Global"), Global},
    {QStringLiteral("Rows"), Rows},
    {QStringLiteral("More"), More},
    {QStringLiteral("NextKey"), NextKey},
    {QStringLiteral("ChainId"), ChainId},
    {QStringLiteral("HeadBlockId"), HeadBlockId},
    {QStringLiteral("HeadBlockNum"), HeadBlockNum},
    {QStringLiteral("HeadBlockTime"), HeadBlockTime},
    {QStringLiteral("Id"), Id},
    {QStringLiteral("Name"), Name},
    {QStringLiteral("Tags"), Tags},
    {QStringLiteral("Timestamp"), Timestamp},
    {QStringLiteral("Table"), Table},
    {QStringLiteral("Scope"), Scope},
    {QStringLiteral("Key"), Key},
    {QStringLiteral("Modification"), Modification},
    {QStringLiteral("Contract_name"), Contract_name},
    {QStringLiteral("Active"), Active},
    {QStringLiteral("AuthorizationTemplate"), AuthorizationTemplate},
    {QStringLiteral("TransactionId"), TransactionId},
    {QStringLiteral("Error"), Error},
    {QStringLiteral("Processed"), Processed},
    {QStringLiteral("BlockNum"), BlockNum},
    {QStringLiteral("LastIrreversibleBlockNum"), LastIrreversibleBlockNum},
    {QStringLiteral("Transactions"), Transactions},
    {QStringLiteral("Trx"), Trx},
    {QStringLiteral("DraftId"), DraftId},
    {QStringLiteral("Deleted"), Deleted},
    {QStringLiteral("Account"), Account},
    {QStringLiteral("VoterAdd"), VoterAdd},
    {QStringLiteral("VoterRemove"), VoterRemove},
    {QStringLiteral("GroupCopy"), GroupCopy},
    {QStringLiteral("GroupRename"), GroupRename},
    {QStringLiteral("CntstNew"), CntstNew},
    {QStringLiteral("CntstModify"), CntstModify},
    {QStringLiteral("CntstTally"), CntstTally},
    {QStringLiteral("CntstDelete"), CntstDelete},
    {QStringLiteral("DcsnSet"), DcsnSet},
    {QStringLiteral("GroupName"), GroupName},
    {QStringLiteral("Voter"), Voter},
    {QStringLiteral("Weight"), Weight},
    {QStringLiteral("NewName"), NewName},
    {QStringLiteral("GroupId"), GroupId},
    {QStringLiteral("Description"), Description},
    {QStringLiteral("Contestants"), Contestants},
    {QStringLiteral("Begin"), Begin},
    {QStringLiteral("End"), End},
    {QStringLiteral("ContestId"), ContestId},
    {QStringLiteral("NewDescription"), NewDescription},
    {QStringLiteral("NewTags"), NewTags},
    {QStringLiteral("DeleteContestants"), DeleteContestants},
    {QStringLiteral("AddContestants"), AddContestants},
    {QStringLiteral("NewBegin"), NewBegin},
    {QStringLiteral("NewEnd"), NewEnd},
    {QStringLiteral("VoterName"), VoterName},
    {QStringLiteral("Opinions"), Opinions},
    {QStringLiteral("BlockNumOrId"), BlockNumOrId},
    {QStringLiteral("VoterAddArgs"), QVariant::fromValue(VoterAddArgs)},
    {QStringLiteral("VoterRemoveArgs"), QVariant::fromValue(VoterRemoveArgs)},
    {QStringLiteral("GroupCopyArgs"), QVariant::fromValue(GroupCopyArgs)},
    {QStringLiteral("GroupRenameArgs"), QVariant::fromValue(GroupRenameArgs)},
    {QStringLiteral("CntstNewArgs"), QVariant::fromValue(CntstNewArgs)},
    {QStringLiteral("CntstModifyArgs"), QVariant::fromValue(CntstModifyArgs)},
    {QStringLiteral("CntstTallyArgs"), QVariant::fromValue(CntstTallyArgs)},
    {QStringLiteral("CntstDeleteArgs"), QVariant::fromValue(CntstDeleteArgs)},
    {QStringLiteral("DcsnSetArgs"), QVariant::fromValue(DcsnSetArgs)},
    {QStringLiteral("LegalActionArguments"), QVariant::fromValue(LegalActionArguments)},
};
