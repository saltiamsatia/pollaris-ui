#pragma once

#include <QObject>
#include <QString>
#include <QSet>
#include <QMap>

// Namespace struct containing constant static QStrings for various tables, fields, actions, JSON values, etc.
// The MapOfAll contains all of the other strings in a QVariantMap, for export to QML. Remember when adding new
// strings to add them to MapOfAll!
struct Strings {
    // Pre-define commonly used strings to avoid typos and make program faster

    const static QString AssistantName;

    // API calls
    const static QString GetTableRows;
    const static QString GetInfo;
    const static QString GetBlock;

    // Table names
    const static QString UnknownTable;
    const static QString PollGroups;
    const static QString GroupAccts;
    const static QString Journal;

    // JSON keys, scope names, account names, authorization levels, etc etc
    const static QString Global;
    const static QString Rows;
    const static QString More;
    const static QString NextKey;
    const static QString ChainId;
    const static QString HeadBlockId;
    const static QString HeadBlockNum;
    const static QString HeadBlockTime;
    const static QString Id;
    const static QString Name;
    const static QString Tags;
    const static QString Timestamp;
    const static QString Table;
    const static QString Scope;
    const static QString Key;
    const static QString Modification;
    const static QString Contract_name;
    const static QString Active;
    const static QString AuthorizationTemplate;
    const static QString TransactionId;
    const static QString Error;
    const static QString What;
    const static QString Processed;
    const static QString BlockNum;
    const static QString LastIrreversibleBlockNum;
    const static QString Transactions;
    const static QString Trx;
    const static QString DraftId;
    const static QString Deleted;
    const static QString Account;

    // Action names
    const static QString VoterAdd;
    const static QString VoterRemove;
    const static QString GroupCopy;
    const static QString GroupRename;
    const static QString CntstNew;
    const static QString CntstModify;
    const static QString CntstTally;
    const static QString CntstDelete;
    const static QString DcsnSet;

    // Argument names
    const static QString GroupName;
    const static QString Voter;
    const static QString Weight;
    const static QString NewName;
    const static QString GroupId;
    const static QString Description;
    const static QString Contestants;
    const static QString Begin;
    const static QString End;
    const static QString ContestId;
    const static QString NewDescription;
    const static QString NewTags;
    const static QString DeleteContestants;
    const static QString AddContestants;
    const static QString NewBegin;
    const static QString NewEnd;
    const static QString VoterName;
    const static QString Opinions;
    const static QString BlockNumOrId;

    // Argument sets for each action
    const static QSet<QString> VoterAddArgs;
    const static QSet<QString> VoterRemoveArgs;
    const static QSet<QString> GroupCopyArgs;
    const static QSet<QString> GroupRenameArgs;
    const static QSet<QString> CntstNewArgs;
    const static QSet<QString> CntstModifyArgs;
    const static QSet<QString> CntstTallyArgs;
    const static QSet<QString> CntstDeleteArgs;
    const static QSet<QString> DcsnSetArgs;
    const static QMap<QString, QSet<QString>> LegalActionArguments;

    // Map containing all of the above values by name
    const static QVariantMap MapOfAll;
};

// Helpers to generate the JSON argument strings for get_table_rows calls
inline QByteArray getTableJson(QString table, QString scope, QString lowerBound, int limit, bool reverse = false) {
    auto format = QStringLiteral(R"({"code": "fmv", "table": "%1", "scope": "%2", "json": true,
                                     "lower_bound": %3, "limit": %4, "reverse": %5})");
    QString rev = reverse? QStringLiteral("true") : QStringLiteral("false");
    return format.arg(table, scope, lowerBound, QString::number(limit), rev).toLocal8Bit();
}
inline QByteArray getTableJson(QString table, QString scope, QString lowerBound) {
    auto format = QStringLiteral(R"({"code": "fmv", "table": "%1", "scope": "%2",
                                     "limit": 100, "json": true, "lower_bound": %3})");
    return format.arg(table, scope, lowerBound).toLocal8Bit();
}
inline QByteArray getTableJson(QString table, QString scope) {
    auto format = QStringLiteral(R"({"code": "fmv", "table": "%1", "scope": "%2", "limit": 100, "json": true})");
    return format.arg(table, scope).toLocal8Bit();
}
