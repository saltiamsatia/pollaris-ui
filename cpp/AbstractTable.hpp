#pragma once

#include <AbstractTableInterface.hpp>
#include <TableSupport.hpp>
#include <EosioName.hpp>

#include <QDebug>
#include <QJSEngine>

/*!
 * \brief A CRTP-style template defining the interface of a virtual field
 *
 * Virtual fields are fields used for GUI concerns, but not stored in the backend database. A virtual field is
 * implemented using a class which manages the calculation of the field value, connects to signals to detect changes
 * affecting the virtual field, and automatically updates it as those changes affect its value.
 *
 * This is a compile-time interface using the CRTP design pattern. It is applied to an implementation like so:
 * class MyVirtualField : public VirtualFieldInterface<MyVirtualField> { ... };
 *
 * Implementors must provide implementations of rowChanged() and get(), and additionally must define the types Type,
 * RowType, and the constexpr C-string role.
 */
template<class ConcreteField, class Type=typename ConcreteField::Type, class RowType=typename ConcreteField::RowType>
class VirtualFieldInterface {
protected:
    // Objects which are 'owned' by this virtual field, and should be deleted when it is deleted. These are useful as
    // context objects to connections, or for storing any table models etc the virtual field uses to calculate itself.
    QMap<QObject*, QMetaObject::Connection> children;

    void addChild(QObject* child) {
        children[child] = child->connect(child, &QObject::destroyed, [this, child] { children.remove(child); });
    }

    // A pointer to the BlockhainInterface
    BlockchainInterface* blockchain;

public:
    // Implementors must define a name for the role the virtual field defines
    constexpr static const char* role = ConcreteField::role;

    // Virtual Fields store a pointer to the blockchain interface to look up other tables
    VirtualFieldInterface(BlockchainInterface* blockchain) : blockchain(blockchain) {}

    ~VirtualFieldInterface() {
        while (!children.empty()) {
            auto child = children.firstKey();
            QObject::disconnect(children.first());
            children.remove(child);
            delete child;
        }
    }

    // This method is called to indicate that the row this virtual field augments has changed. It may be used to
    // update connections to signals that data the virtual field depends on has changed, to notify that the field may
    // have changed as a result.
    // Row is the row of the table this virtual field belongs to
    // RowState is the current load status of the row
    // ChangedSignal is a callable with no arguments or return value. When called, it emits the field's changed signal
    template<typename ChangedSignal>
    void rowChanged(const RowType& row, LoadState rowState, ChangedSignal&& signal) {
        static_cast<ConcreteField*>(this)->rowChanged(row, rowState, std::forward<ChangedSignal>(signal));
    }

    // Get the current value and load status of the virtual field, in the context of the provided row
    QPair<Type, LoadState> get(const RowType& row, LoadState rowState) {
        return static_cast<ConcreteField*>(this)->get(row, rowState);
    }
};

//! This template defines the virtual fields for a given row type. By default, there are no virtual fields, but they
//! can be added for a given row type by specializing this template.
template<class Row>
struct VirtualFields { using type = infra::typelist::list<>; };
template<class Row>
using VirtualFields_t = typename VirtualFields<Row>::type;

//! This template must be specialized for a given table type, to define the table's name in the backend database
template<class Row>
struct TableName {
    // Specializations must set this to true
    static const bool defined = false;
    // Specializations must set this to point to a QString of the table name
    constexpr static const QString* name = &Strings::UnknownTable;
};

/*!
 * \brief A template for a table in the database
 *
 * \tparam Row Struct containing the literal rows of the backend database
 *
 * The Pollaris GUI keeps caches of the data in the backend database tables in order to support the GUI operations.
 * This class template describes a type which manages one of the tables (an instance of the class is responsible for
 * a single scope of a single table).
 *
 * The data is accessed by a two-layered system, where the table itself (AbstractTable) contains the data, but to
 * access the data in a QML-friendly fashion with update signals etc, a second layer is used which is a subclass of
 * QAbstractListModel. This second layer is called the table 'model.' A GUI can request a model containing the rows
 * of the table it wishes to display, and that model will be kept up to date with change notification signals.
 * Multiple models can be taken from the table displaying different ranges of rows of the table. These may overlap.
 *
 * Virtual fields can only be viewed through a model; the table itself does not calculate nor store them.
 */
template<class Row>
class AbstractTable : public AbstractTableInterface {
    constexpr static const QString* TableName = ::TableName<Row>::name;
    static_assert(::TableName<Row>::defined, "TableName must be specialized for all table types");

    using VirtualFields = ::VirtualFields_t<Row>;
    template<class T> using IsVirtualField = std::is_base_of<VirtualFieldInterface<T>, T>;
    static_assert (infra::typelist::filter<VirtualFields, IsVirtualField>() == VirtualFields(),
                   "All elements of VirtualFields list must be implementors of VirtualFieldInterface");

    using RowOps = TableRowOperations<Row>;

    ApiCallback callApi;

    using RowFields = typename infra::reflector<Row>::members;
    using RowId = ::RowId<Row>;
    using VirtualFieldTuple = infra::typelist::apply<VirtualFields, std::tuple>;

    // The actual list of rows
    QList<Row> rowList;
    // Parallel array to rowList, the LoadState of the corresponding row
    QVector<LoadState> rowStates;

    // The list of rows in DraftAdd or PendingAdd states, awaiting placement in rowList
    QList<QVariantMap> locallyAddedRows;

    // Original copies of rows that have been edited; also serves as a list of edited rows
    BackupManager<Row> backups;
    bool deleteBackupRow(RowId id, Row* rowValue = nullptr, LoadState* stateValue = nullptr) {
        bool r = backups.remove(id, rowValue, stateValue);
        if (backups.isEmpty()) emit hasPendingEditsChanged(pendingEdits = false);
        return r;
    }

    // Map of row IDs being loaded to the time we started loading them; used to avoid duplicating load requests
    std::map<RowId, qint64> loadingRows;
    bool rowIsLoading(RowId id, bool markAsLoading = false);

    class Model : public QAbstractListModel {
        AbstractTable* table = nullptr;
        BlockchainInterface* blockchain = nullptr;

        // List of the row IDs this model shows
        QList<RowId> modelIds;
        // List of virtual field tuples, parallel to modelIds above
        mutable QList<VirtualFieldTuple> modelVirtualFields;

        VirtualFieldTuple constructVirtualFieldTuple() {
            if constexpr (infra::typelist::length<VirtualFields>() > 0)
                return infra::typelist::runtime::make_tuple(VirtualFields(), blockchain);
            else
                return VirtualFieldTuple();
        }

        //! The role number of the load state
        static constexpr int LOAD_STATE_ROLE = 0;
        //! The first physical role number
        static constexpr int PHYSICAL_ROLE_BASE = 1;
        //! The number of physical roles
        static constexpr int PHYSICAL_ROLE_COUNT = infra::typelist::length<typename infra::reflector<Row>::members>();
        //! The first virtual role number
        static constexpr int VIRTUAL_ROLE_BASE = PHYSICAL_ROLE_BASE + PHYSICAL_ROLE_COUNT;
        //! The number of virtual roles
        static constexpr int VIRTUAL_ROLE_COUNT = infra::typelist::length<VirtualFields>();

        void updateVirtualRoles(const Row& row, LoadState rowState, VirtualFieldTuple& virtualFields);

    public:
        Model(AbstractTable* table, BlockchainInterface* blockchain);

        int rowCount(const QModelIndex&) const override { return modelIds.size(); }
        QVariant data(const QModelIndex& index, int role) const override;
        QHash<int, QByteArray> roleNames() const override;

        void updateRows(QList<Row> rows);
        void markRowStale(RowId id);
        void deleteRow(RowId id);
    };

    QSet<Model*> models;

    QString tableAndScope = QLatin1String("%1[%2]").arg(TableName->toLatin1(), scope.toLatin1());

public:
    AbstractTable(BlockchainInterface* blockchain, ApiCallback callApi, uint64_t scope)
        : AbstractTableInterface(blockchain, QString::number(scope)), callApi(callApi) {}
    AbstractTable(BlockchainInterface* blockchain, ApiCallback callApi, QString scope)
        : AbstractTableInterface(blockchain, scope), callApi(callApi) {}

    QString tableName() const override { return *TableName; }
    bool hasPendingEdits() const override { return pendingEdits; }

    void updateScope(QString newScope);

    QAbstractListModel* allRows() override;
    QJSValue findRowIf(QJSValue predicate) const override;
    QVariantMap getRow(QVariant id) const override;
    QVariantList localRows() const override;

    const Row* getRow(RowId id, LoadState* rowState = nullptr) const;
    template<typename Callback>
    void refreshRow(RowId id, Callback callback);
    void refreshRow(RowId id);
    void fullRefresh() override;
    void processJournal(QList<JournalEntry> entries) override;

    void draftEditRow(QVariant rowId, QVariantMap changeMap) override;
    void draftAddRow(QVariantMap fieldMap) override;
    void draftDeleteRow(QVariant rowId) override;
    void markEditsPending() override;
    void resetEdits() override;

private:
    void processRowsResponse(QNetworkReply* reply, size_t loadCount);

    void deleteRow(RowId id);
    void markStale(RowId id);
    void getNew(RowId id);

    void checkPendingInsertion(const Row& newRow);
    void checkOverwrittenEdit(Row oldRow, LoadState oldState, const Row& newRow);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// AbstractTable Implementation ///////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<class Row> AbstractTable<Row>::Model::Model(AbstractTable* table, BlockchainInterface* blockchain)
    : QAbstractListModel(table), table(table), blockchain(blockchain) {
    if (table == nullptr)
        qCritical("AbstratTableModel created with nullptr to AbstractTable!");
    if (blockchain == nullptr)
        qCritical("AbstractTableModel created with nullptr to blockchain!");

    // For now, just take all rows as relevant
    // TODO: Make models do subset ranges, like the docs say they do
    for (int i = 0; i < table->rowList.length(); ++i) {
        const Row& r = table->rowList[i];
        modelIds.append(r.getId());
        modelVirtualFields.push_back(constructVirtualFieldTuple());
        updateVirtualRoles(r, table->rowStates[i], modelVirtualFields.back());

        // If the row is stale, refresh it.
        if (table->rowStates[i] == LoadState::Stale)
            table->refreshRow(r.getId());
    };
}

template<class Row> void AbstractTable<Row>::Model::updateVirtualRoles(const Row& row, LoadState rowState,
                                                                       VirtualFieldTuple& virtualFields) {
    using FieldNumbers = infra::typelist::make_sequence<infra::typelist::length<VirtualFields>()>;
    infra::typelist::runtime::for_each(FieldNumbers(), [this, &row, rowState, &virtualFields] (auto FieldNumber) {
        constexpr auto fieldNumber = decltype(FieldNumber)::type::value;
        constexpr auto role = VIRTUAL_ROLE_BASE + fieldNumber;
        auto pos = std::lower_bound(modelIds.begin(), modelIds.end(), row.getId(), CompareId<Row>());
        auto index = createIndex(pos-modelIds.begin(), 0);
        auto emitDataChanged = [this, index=std::move(index), role] { emit dataChanged(index, index, {role}); };
        std::get<fieldNumber>(virtualFields).rowChanged(row, rowState, emitDataChanged);
    });
}

template<class Row> QVariant AbstractTable<Row>::Model::data(const QModelIndex& index, int role) const {
    // Check row index
    if (index.row() >= modelIds.length()) {
        qWarning() << "Asked to retrieve row" << index.row() << "from" << table->tableAndScope << "table of"
                   << modelIds.length() << "rows";
        return QVariant();
    }
    RowId rowId = modelIds[index.row()];

    // Get row
    LoadState state;
    const Row* row = table->getRow(rowId, &state);
    if (row == nullptr) {
        // If row is somehow not loaded, schedule it to load, and return null (or loading for loadstate)
        table->refreshRow(rowId);
        if (role == LOAD_STATE_ROLE)
            return QVariant::fromValue(LoadState::Loading);
        return QVariant();
    }
    // Check if role is load state role
    if (role == LOAD_STATE_ROLE)
        return QVariant::fromValue(state);
    // Check role number
    QVariant result;
    if (role < (PHYSICAL_ROLE_BASE + PHYSICAL_ROLE_COUNT)) {
        // Get field from row
        infra::typelist::runtime::dispatch(RowFields(), role - PHYSICAL_ROLE_BASE, [row, &result](auto Reflector) {
            result = QVariant::fromValue(decltype(Reflector)::type::get(*row));
        });
    } else if (role < (VIRTUAL_ROLE_BASE + VIRTUAL_ROLE_COUNT)) {
        // Get virtual field
        if constexpr (VIRTUAL_ROLE_COUNT > 0) {
            using Indexes = infra::typelist::make_sequence<infra::typelist::length<VirtualFields>()>;
            VirtualFieldTuple& virtualFields = modelVirtualFields[index.row()];
            infra::typelist::runtime::dispatch(Indexes(), role-VIRTUAL_ROLE_BASE,
                                               [&virtualFields, this, row, state, &result](auto Index) {
                auto r = std::get<decltype(Index)::type::value>(virtualFields).get(*row, state);
                result = QVariantMap{{QLatin1String("value"), QVariant::fromValue(r.first)},
                                     {QLatin1String("status"), QVariant::fromValue(r.second)}};
            });
        }
    } else {
        qWarning() << "Asked to retrieve role" << role << "from" << table->tableAndScope
                   << "table, but that role is not defined!";
    }

    return result;
}

template<class Row> QHash<int, QByteArray> AbstractTable<Row>::Model::roleNames() const {
    QHash<int, QByteArray> result;
    result[LOAD_STATE_ROLE] = LOAD_STATE_ROLE_NAME;
    infra::typelist::runtime::for_each(RowFields(), [&result](auto Reflector) {
        result[result.size()] = decltype(Reflector)::type::get_name();
    });
    infra::typelist::runtime::for_each(VirtualFields(), [&result](auto Descriptor) {
        result[result.size()] = decltype(Descriptor)::type::role;
    });
    return result;
}

template<class Row> void AbstractTable<Row>::Model::updateRows(QList<Row> rows) {
    if (rows.isEmpty())
        return;

    auto pos = std::lower_bound(modelIds.begin(), modelIds.end(), rows.first().getId(), CompareId<Row>());
    if (pos == modelIds.end()) {
        auto firstRowNumber = modelIds.size();
        beginInsertRows(QModelIndex(), firstRowNumber, firstRowNumber + rows.length()-1);
        for (const Row& r : rows) {
            modelIds.append(r.getId());
            modelVirtualFields.append(constructVirtualFieldTuple());
            updateVirtualRoles(r, LoadState::Loaded, modelVirtualFields.last());
        }
        endInsertRows();
    } else {
        while (!rows.empty()) {
            auto rowNumber = pos - modelIds.begin();
            // New row or update?
            if (pos != modelIds.end() && *pos == rows.first().getId()) {
                // Update.
                updateVirtualRoles(rows.takeFirst(), LoadState::Loaded, modelVirtualFields[rowNumber]);
                emit dataChanged(createIndex(rowNumber, 0), createIndex(rowNumber, 0));
            } else {
                // New row.
                beginInsertRows(QModelIndex(), rowNumber, rowNumber);
                pos = modelIds.insert(pos, rows.first().getId());
                modelVirtualFields.insert(rowNumber, constructVirtualFieldTuple());
                updateVirtualRoles(rows.takeFirst(), LoadState::Loaded, modelVirtualFields[rowNumber]);
                endInsertRows();
            }
            if (!rows.isEmpty())
                pos = std::lower_bound(pos, modelIds.end(), rows.first().getId(), CompareId<Row>());
        }
    }
}

template<class Row> void AbstractTable<Row>::Model::markRowStale(RowId id) {
    auto pos = std::lower_bound(modelIds.begin(), modelIds.end(), id, CompareId<Row>());
    if (pos != modelIds.end() && *pos == id) {
        auto row = pos - modelIds.begin();
        emit dataChanged(createIndex(row, 0), createIndex(row, 0), {LOAD_STATE_ROLE});
    }

    // For now, models load all rows automatically, so just reload it. In the future, we might not bother all the time
    table->refreshRow(id);
}

template<class Row> void AbstractTable<Row>::Model::deleteRow(RowId id) {
    auto pos = std::lower_bound(modelIds.begin(), modelIds.end(), id, CompareId<Row>());
    if (pos != modelIds.end() && *pos == id) {
        auto row = pos - modelIds.begin();
        beginRemoveRows(QModelIndex(), row, row);
        modelIds.erase(pos);
        modelVirtualFields.removeAt(row);
        endRemoveRows();
    }
}

template<class Row> bool AbstractTable<Row>::rowIsLoading(RowId id, bool markAsLoading) {
    // The duration a loading request may be waiting before it may be dropped and reloaded
    constexpr static qint64 MAX_LOADING_SECONDS_BEFORE_DROPPED = 7;
    auto itr = loadingRows.lower_bound(id);
    if (itr != loadingRows.end() && itr->first == id) {
        if ((QDateTime::currentSecsSinceEpoch() - itr->second) < MAX_LOADING_SECONDS_BEFORE_DROPPED)
            return true;
        qInfo() << "A request to load row ID" << id << "was made, but never answered.";
    }

    if (markAsLoading)
        loadingRows.insert(itr, std::make_pair(id, QDateTime::currentSecsSinceEpoch()));
    return false;
}

template<class Row>
void AbstractTable<Row>::updateScope(QString newScope) {
    if (newScope != scope) {
        tableAndScope = tableAndScope.left(tableAndScope.indexOf('[')) + "[" + newScope + "]";
        emit scopeChanged(scope = newScope);
        fullRefresh();
    }
}

template<class Row> QAbstractListModel* AbstractTable<Row>::allRows() {
    if (models.empty())
        fullRefresh();
    Model* model = new Model(this, blockchain);
    models.insert(model);
    connect(model, &QObject::destroyed, this, [this, model] { models.remove(model); });
    return model;
}

template<class Row> QJSValue AbstractTable<Row>::findRowIf(QJSValue predicate) const {
    auto* engine = qjsEngine(this);
    if (rowList.isEmpty() || !predicate.isCallable() || engine == nullptr)
        return QJSValue(QJSValue::UndefinedValue);

    for (int i = 0; i < rowList.length(); ++i) {
        const Row& row = rowList[i];
        QJSValue jsRow = engine->toScriptValue(Convert<Row>::toJsonObject(row));
        if (predicate.call({jsRow}).toBool()) {
            jsRow.setProperty(LOAD_STATE_ROLE_NAME, (int)rowStates[i]);
            return jsRow;
        }
    }
    return QJSValue(QJSValue::NullValue);
}

template<class Row> QVariantMap AbstractTable<Row>::getRow(QVariant id) const {
    LoadState state;
    const auto* row = getRow(id.value<RowId>(), &state);
    if (row == nullptr)
        return {};
    auto variantRow = Convert<Row>::toVariantMap(*row);
    variantRow[LOAD_STATE_ROLE_NAME] = QVariant::fromValue((int)state);
    return variantRow;
}

template<class Row> QVariantList AbstractTable<Row>::localRows() const {
    QVariantList result;
    result.reserve(rowList.length());
    for (int i = 0; i < rowList.length(); ++i) {
        auto row = Convert<Row>::toVariantMap(rowList[i]);
        row[LOAD_STATE_ROLE_NAME] = QVariant::fromValue(rowStates[i]);
        result.append(std::move(row));
    }
    return result;
}

template<class Row> const Row* AbstractTable<Row>::getRow(RowId id, LoadState* rowState) const {
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), id, CompareId<Row>());
    if (pos != rowList.end() && pos->getId() == id) {
        if (rowState != nullptr)
            *rowState = rowStates[pos - rowList.begin()];
        return &*pos;
    }
    return nullptr;
}

template<class Row> template<class Callback> void AbstractTable<Row>::refreshRow(RowId id, Callback callback) {
    if (rowIsLoading(id, /* mark it as loading now */ true))
        // Already loading; don't load it again
        return;

    QString lowerBound;
    if constexpr (std::is_same_v<RowId, QString>)
        lowerBound = '"' + id + '"';
    else
        lowerBound = QString::number(id);

    auto* reply = callApi(Strings::GetTableRows, getTableJson(*TableName, scope, lowerBound, 1));
    connect(reply, &QNetworkReply::finished, [this, reply, id, cb=std::move(callback)] {
        loadingRows.remove(id);
        processRowsResponse(reply, 1);
        cb(getRow(id));
    });
}

template<class Row> void AbstractTable<Row>::refreshRow(RowId id) {
    if (rowIsLoading(id, /* mark it as loading now */ true))
        // Already loading; don't load it again
        return;

    QString lowerBound;
    if constexpr (std::is_same_v<RowId, QString>)
        lowerBound = '"' + id + '"';
    else
        lowerBound = QString::number(id);

    auto* reply = callApi(Strings::GetTableRows, getTableJson(*TableName, scope, lowerBound, 1));
    connect(reply, &QNetworkReply::finished, [this, id, reply] {
        loadingRows.erase(id);
        processRowsResponse(reply, 1);
    });
}

template<class Row> void AbstractTable<Row>::fullRefresh() {
    auto* reply = callApi(Strings::GetTableRows, getTableJson(*TableName, scope));
    connect(reply, &QNetworkReply::finished, [this, reply] { processRowsResponse(reply, 0); });
}

template<class Row> void AbstractTable<Row>::processJournal(QList<JournalEntry> entries) {
    std::for_each(entries.begin(), entries.end(), [this](const JournalEntry& entry) {
        if (entry.table == *TableName && QString::number(entry.scope) == scope) {
            auto key = [&entry] {
                if constexpr (std::is_same_v<RowId, QString>)
                    return eosio::name_to_string(entry.key);
                else
                    return entry.key;
            };
            if (entry.type == JournalEntry::DeleteRow) {
                qInfo() << tableAndScope << "deleting row ID" << entry.key << "as per journal";
                deleteRow(key());
            } else if (entry.type == JournalEntry::ModifyRow) {
                qInfo() << tableAndScope << "marking stale row ID" << entry.key << "as per journal";
                markStale(key());
            } else if (entry.type == JournalEntry::AddRow) {
                qInfo() << tableAndScope << "marking new row ID" << entry.key << "as per journal";
                getNew(key());
            }
        }
    });
}

template<class Row> void AbstractTable<Row>::draftEditRow(QVariant rowId, QVariantMap changeMap) {
    // No new edits allowed when edits are pending
    if (pendingEdits)
        return;

    // Find the row to edit
    auto id = rowId.value<RowId>();
    auto rowItr = std::lower_bound(rowList.begin(), rowList.end(), id, CompareId<Row>());
    if (rowItr == rowList.end() || rowItr->getId() != id) {
        qCritical() << tableAndScope << "Asked to make draft edits to row ID" << id << "but row not found!";
        return;
    }
    auto rowState = rowStates[rowItr - rowList.begin()];
    if (rowState == LoadState::DraftDelete) {
        qWarning() << tableAndScope << "Asked to make draft edit to row ID" << id << "but that row is draft deleted";
        return;
    }
    auto oldRow = std::move(*rowItr);

    // Check that the row ID is unchanged
    QStringList unusedKeys;
    auto scratchRow = Convert<Row>::fromVariantMap(changeMap, oldRow, &unusedKeys);
    if (!unusedKeys.isEmpty())
        qWarning() << tableAndScope << "Asked to make draft edits to row, but edits contained unknown fields:"
                   << unusedKeys;
    if (scratchRow.getId() != oldRow.getId()) {
        qWarning() << tableAndScope << "Asked to make draft edits to row ID" << id << "which change the row's ID";
        return;
    }

    if (rowState != LoadState::DraftAdd && rowState != LoadState::DraftEdit) {
        auto& state = rowStates[rowItr-rowList.begin()];
        // Save a backup of the unedited row, if one isn't already saved, unless the row was already a draft
        backups.save(oldRow, state);
        // Update the row state to draft, but again, not if the row was a draft already
        state = LoadState::DraftEdit;
    } else if (rowState == LoadState::DraftAdd) {
        // When editing a draft added row, update any values in the locally added rows to the edited values.
        // This is to ensure that we still match the updated row when we get it from the backend.
        auto itr = std::find_if(locallyAddedRows.begin(), locallyAddedRows.end(), [rowId](QVariantMap row) {
                return row[Strings::DraftId] == rowId;
        });
        if (itr != locallyAddedRows.end()) {
            auto keys = changeMap.keys();
            while (!keys.empty()) {
                auto pos = itr->find(keys.first());
                if (pos != itr->end())
                    *pos = changeMap[keys.takeFirst()];
            }
        }
    }

    // Apply the edits to the table
    *rowItr = std::move(scratchRow);
    RowOps::RowDraftEdited(oldRow, *rowItr, this);

    // Notify the models
    QList<Row> updates(rowItr, rowItr+1);
    std::for_each(models.begin(), models.end(), [&updates](Model* model) { model->updateRows(updates); });
}

template<class Row> void AbstractTable<Row>::draftAddRow(QVariantMap fieldMap) {
    // No new edits allowed when edits are pending
    if (pendingEdits)
        return;

    QStringList unusedKeys;
    Row newRow = Convert<Row>::fromVariantMap(fieldMap, Row(), &unusedKeys);
    if (!unusedKeys.isEmpty())
        qWarning() << tableAndScope << "Asked to draft add row, but row contained unknown fields:" << unusedKeys;
    // Numeric IDs for draft rows are assigned by the table
    if constexpr (std::is_integral_v<RowId>) {
        if (newRow.getId() != RowId()) {
            qWarning() << tableAndScope << "Asked to draft add row" << fieldMap << "which specifies a numerid ID."
                       << "Draft added rows cannot specify their own numeric IDs";
            return;
        }
        newRow.setId((rowList.isEmpty() ||
                      rowList.last().getId() < BASE_DRAFT_ID)? BASE_DRAFT_ID : (rowList.last().getId() + 1));
    } else {
        // String IDs must be checked against collisions
        if (getRow(newRow.getId()) != nullptr) {
            qWarning() << tableAndScope << "Asked to draft add row" << newRow << "but ID collides with other row";
            return;
        }
    }
    // Store the draft ID in the fieldMap to aid in lookup later
    fieldMap[Strings::DraftId] = QVariant::fromValue(newRow.getId());

    // Add the row to the table and the new rows list
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), newRow.getId(), CompareId<Row>());
    pos = rowList.insert(pos, newRow);
    rowStates.insert(pos - rowList.begin(), LoadState::DraftAdd);
    locallyAddedRows.append(fieldMap);
    RowOps::RowDraftAdded(newRow, this);

    // Add the row to the backup rows with the DraftAdd state to signify that there was no backed up row
    backups.save(newRow, LoadState::DraftAdd);

    // Notify the models
    QList<Row> added{std::move(newRow)};
    std::for_each(models.begin(), models.end(), [&added](Model* model) { model->updateRows(added); });
}

template<class Row> void AbstractTable<Row>::draftDeleteRow(QVariant rowId) {
    // No new edits allowed when edits are pending
    if (pendingEdits)
        return;
    auto id = rowId.value<RowId>();
    qInfo() << tableAndScope << "Draft deleting row ID" << id;

    // Find the row
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), id, CompareId<Row>());
    if (pos == rowList.end() || pos->getId() != id) {
        qWarning() << tableAndScope << "Asked to draft delete row ID" << id << "but that ID wasn't found";
        return;
    }

    // If row is draft added, just delete it; it's not really there!
    auto& state = rowStates[pos-rowList.begin()];
    if (state == LoadState::DraftAdd) {
        auto itr = std::remove_if(locallyAddedRows.begin(), locallyAddedRows.end(), [rowId](QVariantMap row) {
            return row[Strings::DraftId] == rowId;
        });
        locallyAddedRows.erase(itr, locallyAddedRows.end());
        deleteRow(id);
        deleteBackupRow(id);
        return;
    }

    // Mark row as draft delete and add it to the backup list
    backups.save(*pos, state);
    state = LoadState::DraftDelete;
    RowOps::RowDraftDeleted(*pos, this);

    // Notify the models
    QList<Row> removed{*pos};
    std::for_each(models.begin(), models.end(), [&removed](Model* model) { model->updateRows(removed); });
}

template<class Row> void AbstractTable<Row>::markEditsPending() {
    // Look at the backups to know which rows have been edited, and set their state to Pending
    QList<Row> updates;
    backups.forEach([this, &updates](const Row& bak) {
        auto pos = std::lower_bound(rowList.begin(), rowList.end(), bak.getId(), CompareId<Row>());
        if (pos != rowList.end() && pos->getId() == bak.getId()) {
            LoadState& state = rowStates[pos - rowList.begin()];

            if (state == LoadState::DraftEdit)
                state = LoadState::PendingEdit;
            else if (state == LoadState::DraftAdd)
                state = LoadState::PendingAdd;
            else if (state == LoadState::DraftDelete)
                state = LoadState::PendingDelete;
            else
                qWarning() << tableAndScope << "Asked to mark edits pending, but row ID" << bak.getId()
                           << "does not have a draft status";

            updates.append(*pos);
        } else {
            qWarning() << tableAndScope << "Asked to mark edits pending, but backup row" << bak.getId()
                       << "does not correspond to a row in the table!";
        }
    });
    RowOps::DraftChangesPending(this);

    // Notify the models
    std::for_each(models.begin(), models.end(), [&updates](Model* model) { model->updateRows(updates); });
    // Edits are now pending rather than draft, so set hasPendingEdits to true
    if (!backups.isEmpty())
        emit hasPendingEditsChanged(pendingEdits = true);
}

template<class Row> void AbstractTable<Row>::resetEdits() {
    qInfo() << tableAndScope << "Resetting edits";
    // Restore the backups
    backups.forEach([this](const Row& bak, LoadState bakState, auto remove) {
        auto pos = std::lower_bound(rowList.begin(), rowList.end(), bak.getId(), CompareId<Row>());
        if (pos != rowList.end() && pos->getId() == bak.getId()) {
            LoadState& rowState = rowStates[pos - rowList.begin()];
            // If the row was a local add, just delete it again
            if (rowState == LoadState::DraftAdd || rowState == LoadState::PendingAdd) {
                qInfo() << tableAndScope << "Removing locally added row" << pos->getId();
                deleteRow(pos->getId());
                remove();
            } else {
                qInfo() << tableAndScope << "Reverting locally edited or deleted row" << pos->getId();
                // If the row was a local edit, restore its pre-edit value
                if (rowState == LoadState::DraftEdit || rowState == LoadState::PendingEdit)
                    *pos = bak;
                // Whether it was a local edit or local delete, restore its old state
                rowState = bakState;
            }
        } else {
            qWarning() << tableAndScope << "Asked to revert edits, but backup row" << bak.getId()
                       << "does not correspond to a row in the table!";
            remove();
        }
    });
    RowOps::LocalChangesReset(this);

    // Notify the models
    std::for_each(models.begin(), models.end(), [this](Model* model) { model->updateRows(backups.getRows()); });

    // Delete the backups
    backups.clear();

    // Clear the locally added rows
    locallyAddedRows.clear();
}

template<class Row> void AbstractTable<Row>::processRowsResponse(QNetworkReply* reply, size_t loadCount) {
    if (reply->error() != QNetworkReply::NoError)
        return;

    // Sanity check response
    QJsonValue nextKey;
    auto jsonDoc = QJsonDocument::fromJson(reply->readAll());
    auto rows = parseRows(jsonDoc, &nextKey);
    if (!rows.has_value()) {
        qWarning() << "Error in" << tableAndScope << "table: response to request for rows not sensible:" << jsonDoc;
        return;
    }

    // Check if there's more to load and load it
    if (!nextKey.isNull() && (loadCount == 0 || rows.value().size() < loadCount)) {
        auto json = getTableJson(*TableName, scope, nextKey.toString());
        auto* reply = callApi(Strings::GetTableRows, json);
        size_t remaining = loadCount == 0? 0 : (loadCount - rows.value().size());
        connect(reply, &QNetworkReply::finished, [this, reply, remaining] { processRowsResponse(reply, remaining); });
    }

    if (rows.value().isEmpty())
        return;

    // Find the position in the table where we'll begin placing rows
    const QList<Row> newRows = Convert<Row>::fromJsonArray(rows.value());
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), newRows.first(), CompareId<Row>());
    auto newPos = newRows.begin();

    // Just adding new rows to the end, or updating throughout?
    if (pos == rowList.end()) {
        // Adding at the end
        qInfo() << tableAndScope << "Inserting" << newRows.size() << "rows at end of table";
        auto firstRow = rowList.length();
        // Insert the rows
        rowList.append(newRows);
        // Insert the loadState roles
        rowStates.insert(rowStates.end(), newRows.length(), LoadState::Loaded);
        RowOps::RowsAdded(newRows, this);
    } else {
        // Updating throughout
        while (newPos != newRows.end()) {
            auto rowNumber = pos - rowList.begin();
            //Overwrite or insert?
            if (pos != rowList.end() && pos->getId() == newPos->getId()) {
                // Overwrite.
                qInfo() << tableAndScope << "Updating row ID" << newPos->getId();
                auto oldRow = std::move(*pos);
                auto oldState = rowStates[rowNumber];
                *pos = std::move(*newPos);
                rowStates[rowNumber] = LoadState::Loaded;
                // Check the row against the edit tracking to see if it matches local insertion or edit
                if (oldState == LoadState::PendingAdd || oldState == LoadState::Loading) {
                    RowOps::RowLoaded(*pos, this);
                    checkPendingInsertion(*pos);
                } else {
                    RowOps::RowUpdated(oldRow, *pos, this);
                    checkOverwrittenEdit(std::move(oldRow), oldState, *pos);
                }
            } else {
                // Insert.
                qInfo() << tableAndScope << "Inserting row ID" << newPos->getId();
                pos = rowList.insert(pos, *newPos);
                rowStates.insert(rowNumber, LoadState::Loaded);
                // Check the row against the edit tracking to see if it matches a local insertion
                checkPendingInsertion(*newPos);
            }
            if (++newPos != newRows.end())
                pos = std::lower_bound(pos, rowList.end(), *newPos, CompareId<Row>());
        }
    }

    // Notify the models
    std::for_each(models.begin(), models.end(), [newRows](Model* model) { model->updateRows(newRows); });
}

template<class Row> void AbstractTable<Row>::deleteRow(RowId id) {
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), id, CompareId<Row>());
    if (pos != rowList.end() && pos->getId() == id) {
        LoadState state = rowStates[pos - rowList.begin()];
        rowStates.removeAt(pos - rowList.begin());
        auto row = std::move(*pos);
        rowList.erase(pos);
        RowOps::RowDeleted(row, this);
        if (state == LoadState::PendingDelete) {
            RowOps::PendingDeleteSettled(row, this);
            emit pendingEditSettled(Convert<Row>::toVariantMap(row), {{Strings::Deleted, QVariant(true)}});
            if (!deleteBackupRow(id))
                qWarning() << tableAndScope << "Deleting row with pending delete state, but couldn't find the backup";
        } else if (state == LoadState::DraftAdd || state == LoadState::DraftEdit || state == LoadState::DraftDelete) {
            RowOps::DraftRowInvalidated(row, {}, this);
            emit draftEditInvalidated(QVariant::fromValue(id));
            deleteBackupRow(id);
        }
    }
    std::for_each(models.begin(), models.end(), [id](Model* model) { model->deleteRow(id); });
}

template<class Row> void AbstractTable<Row>::markStale(RowId id) {
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), id, CompareId<Row>());
    if (pos != rowList.end() && pos->getId() == id) {
        // If row is pending an edit (or delete), don't mark it stale, but do refresh it
        auto& state = rowStates[pos - rowList.begin()];
        if (state == LoadState::PendingEdit || state == LoadState::PendingDelete) {
            refreshRow(id);
            return;
        }
        // If row is in a draft state, reset it and notify that it got munged
        if (state == LoadState::DraftAdd || state == LoadState::DraftEdit || state == LoadState::DraftDelete) {
            emit draftEditInvalidated(QVariant::fromValue(id));
            if (!deleteBackupRow(id, &*pos, &state))
                qWarning() << tableAndScope << "Draft row invalidated, but couldn't find the backup";
        }

        state = LoadState::Stale;
        RowOps::RowStale(*pos, this);
    }
    std::for_each(models.begin(), models.end(), [id](Model* model) { model->markRowStale(id); });
}

template<class Row> void AbstractTable<Row>::getNew(RowId id) {
    // Regardless of all else, load the row
    refreshRow(id);

    // If row already exists, don't create a placeholder, but log it if it's weird
    auto pos = std::lower_bound(rowList.begin(), rowList.end(), id, CompareId<Row>());
    if (pos != rowList.end() && pos->getId() == id) {
        auto state = rowStates[pos-rowList.begin()];
        // If the row exists as a pending added row, it's normal and not worth logging
        if (state != LoadState::PendingAdd)
            qWarning() << tableAndScope << "asked to load new row ID" << id
                       << "but that row already exists with state" << rowStates[pos-rowList.begin()];
        return;
    }

    // Create a placeholder with Loading state to mark new row's position
    auto index = pos - rowList.begin();
    {
        Row row;
        row.setId(id);
        rowList.insert(pos, std::move(row));
    }
    rowStates.insert(index, LoadState::Loading);
    RowOps::RowLoading(*pos, this);
}

template<class Row> void AbstractTable<Row>::checkPendingInsertion(const Row& newRow) {
    // We only check if new rows match a pending row add if there are pending row adds.
    if (!pendingEdits)
        return;
    if (locallyAddedRows.isEmpty())
        return;

    // Predicate to check if all fields specified in the QVariantMap match our new row
    auto matchesRow = [&newRow](const QVariantMap newFields) {
        return infra::typelist::runtime::all_of(RowFields(), [&newRow, &newFields] (auto Descriptor) {
            using descriptor = typename decltype(Descriptor)::type;
            QString fieldName(descriptor::get_name());
            auto itr = newFields.find(fieldName);
            if (itr != newFields.end())
                return itr->value<typename descriptor::type>() == descriptor::get(newRow);
            return true;
        });
    };

    qInfo() << tableAndScope << "Checking incoming row" << newRow << "against added rows:" << locallyAddedRows;
    auto match = std::find_if(locallyAddedRows.begin(), locallyAddedRows.end(), matchesRow);
    if (match == locallyAddedRows.end())
        // New row does not match a pending add
        return;

    // We have a match! Remove it from the added rows list and delete any draft row that might be left
    qInfo() << tableAndScope << "New row" << newRow << "from backend matches a pending insertion" << *match;
    QVariantMap matchFields = *match;
    match = locallyAddedRows.erase(match);
    auto draftID = matchFields[Strings::DraftId].value<RowId>();
    if (draftID != newRow.getId())
        deleteRow(draftID);
    RowOps::PendingAddSettled(draftID, newRow, this);

    // Signal the match
    matchFields.remove(Strings::DraftId);
    emit pendingEditSettled(matchFields, Convert<Row>::toVariantMap(newRow));

    // Remove the backup
    if (!deleteBackupRow(draftID))
        qWarning() << tableAndScope << "Matched inserted row to pending insertion, but couldn't find the backup";

    // Hopefully there was only one match; warn if there was another
    if (std::find_if(locallyAddedRows.begin(), locallyAddedRows.end(), matchesRow) != locallyAddedRows.end())
        qWarning() << tableAndScope << "Multiple pending insertions matched an inserted row";
}

template<class Row> void AbstractTable<Row>::checkOverwrittenEdit(Row oldRow, LoadState oldState, const Row& newRow) {
    if (!QSet<LoadState>{LoadState::Loading, LoadState::DraftAdd, LoadState::DraftEdit, LoadState::DraftDelete,
                         LoadState::PendingAdd, LoadState::PendingEdit, LoadState::PendingDelete}.contains(oldState))
        // Not an edit
        return;
    qInfo() << tableAndScope << "Processing possible row edit from" << oldRow << oldState << "to" << newRow;

    auto id = oldRow.getId();
    if (oldState == LoadState::DraftAdd) {
        // This will probably never happen: an added row overwrites a draft addition
        qInfo() << tableAndScope << "Updated row from backend collides in ID with a draft added row";
        RowOps::DraftRowInvalidated(oldRow, newRow, this);
        emit draftEditInvalidated(QVariant::fromValue(id));

        // Remove row from locally added rows list
        auto addItr = std::find_if(locallyAddedRows.begin(), locallyAddedRows.end(), [id](QVariantMap row) {
                return row[Strings::DraftId].value<RowId>() == id;
        });
        if (addItr != locallyAddedRows.end())
            locallyAddedRows.erase(addItr);
        else
            qWarning() << tableAndScope << "Unable to find locally added row record for munged draft add row";
    } else if (oldState == LoadState::DraftEdit) {
        qInfo() << tableAndScope << "Update from backend overwrote draft edit on row ID" << id;
        RowOps::DraftRowInvalidated(oldRow, newRow, this);
        // Notify of the overwritten Draft edit
        emit draftEditInvalidated(QVariant::fromValue(id));
    } else if (oldState == LoadState::PendingEdit) {
        qInfo() << tableAndScope << "Update from backend overwrote pending edit on row ID" << id;
        RowOps::PendingEditSettled(oldRow, newRow, this);

        // If the pending edit was mistaken, log this fact
        if (oldRow != newRow)
            qWarning() << "Pending row " << oldRow << "did not match update from backend" << newRow;

        // Signal that the pending edit is resolved
        emit pendingEditSettled(Convert<Row>::toVariantMap(oldRow), Convert<Row>::toVariantMap(newRow));
    } else if (oldState == LoadState::DraftDelete || oldState == LoadState::PendingDelete) {
        qInfo() << tableAndScope << "Updated row from backend changes a row we had a local delete for";
        RowOps::DraftRowInvalidated(oldRow, newRow, this);
        emit draftEditInvalidated(QVariant::fromValue(id));
    }

    // In most cases, we want to delete the backup, but not if the row was Loading, and PendingAdd did it already
    if (!deleteBackupRow(id))
        qWarning() << tableAndScope << "Unable to find backup when processing overwritten local edit";
}
