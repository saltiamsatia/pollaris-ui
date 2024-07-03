#pragma once

#include <Infrastructure/reflectors.hpp>

#include <Strings.hpp>
#include <Enums.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>

class BlockchainInterface;

/*!
 * \fn std::optional<QJsonArray> parseRows(QJsonDocument getTableRowsResponse, QJsonValue* nextKey = nullptr)
 * \brief Sanity check a reponse from the get_table_rows API call and return the rows
 * \param getTableRowsResponse The full JSON response from the API server
 * \param nextKey Optional parameter. If provided, and if the response indicates that more rows exist for the query
 * than were returned in this response, value will be populated with the key to use to fetch the next row after the
 * ones already returned. If there are no further rows, value will be set to null.
 * \return An array of the rows from the server, or null if the response failed parsing
 */
inline std::optional<QJsonArray> parseRows(QJsonDocument getTableRowsResponse, QJsonValue* nextKey = nullptr) {
    if (!getTableRowsResponse.isObject())
        return {};

    QJsonObject response = getTableRowsResponse.object();
    auto rows = response[Strings::Rows];
    if (!rows.isArray())
        return {};

    if (nextKey != nullptr)
        *nextKey = (response[Strings::More].toBool()? response[Strings::NextKey] : QJsonValue());
    return rows.toArray();
}

//! \brief Template to convert a reflected struct to/from Qt types
template<class Struct>
struct Convert {
    using Reflector = infra::reflector<Struct>;
    using Members = typename Reflector::members;
    static_assert(typename Reflector::is_defined(), "JSON converters cannot be used on unreflected types");

    static Struct fromJsonObject(QJsonObject object) {
        using namespace infra;
        Struct result;
        typelist::runtime::for_each(Members(), [&result, &object] (auto MemberWrapper) {
            using Member = typename decltype(MemberWrapper)::type;
            Member::get(result) = object[QString(Member::get_name())].toVariant().value<typename Member::type>();
        });
        return result;
    }
    static QJsonObject toJsonObject(const Struct& record) {
        using namespace infra;
        QJsonObject result;
        typelist::runtime::for_each(Members(), [&result, &record] (auto MemberWrapper) {
            using Member = typename decltype(MemberWrapper)::type;
            result[QString(Member::get_name())] = QJsonValue::fromVariant(QVariant::fromValue(Member::get(record)));
        });
        return result;
    }

    static QList<Struct> fromJsonArray(QJsonArray array) {
        QList<Struct> result;
        std::transform(array.begin(), array.end(), std::back_inserter(result), [](QJsonValueRef v) {
            return fromJsonObject(v.toObject());
        });
        return result;
    }

    static QVariantMap toVariantMap(const Struct& record) {
        QVariantMap result;
        infra::typelist::runtime::for_each(Members(), [&record, &result](auto Descriptor) {
            using descriptor = typename decltype(Descriptor)::type;
            result[descriptor::get_name()] = QVariant::fromValue(descriptor::get(record));
        });
        return result;
    }
    static Struct fromVariantMap(QVariantMap map, const Struct& defaultValues = Struct(),
                                 QStringList* unusedKeys = nullptr) {
        Struct result = defaultValues;
        infra::typelist::runtime::for_each(Members(), [&map, &result](auto Descriptor) {
            using descriptor = typename decltype (Descriptor)::type;
            auto updateItr = map.find(descriptor::get_name());
            if (updateItr != map.end()) {
                descriptor::get(result) = updateItr->template value<typename descriptor::type>();
                map.erase(updateItr);
            }
        });
        if (unusedKeys != nullptr) {
            unusedKeys->clear();
            if (!map.isEmpty())
                unusedKeys->append(map.keys());
        }
        return result;
    }
};

template<class Row>
using RowId = decltype(std::declval<Row>().getId());

//! A template for a less-than comparator that compares rows and row IDs by ID value
template<class Row>
struct CompareId {
    using is_transparent = void;
    bool operator()(const Row& a, const Row& b) const { return a.getId() < b.getId(); }
    bool operator()(const Row& a, RowId<Row> b) const { return a.getId() < b; }
    bool operator()(RowId<Row> a, const Row& b) const { return a < b.getId(); }
    bool operator()(RowId<Row> a, RowId<Row> b) const { return a < b; }
};

// Forward declare AbstractTable
template<class Row> class AbstractTable;

//! This template can be specialized to inject table-specific logic at various points during general table processing
template<class Row, typename=void>
struct TableRowOperations {
    using Table = AbstractTable<Row>;
    using RowId = decltype(std::declval<Row>().getId());

    //! Called immediately after adding new rows to table from database, which did not have placeholders
    static void RowsAdded(QList<Row>, Table*) {}
    //! Called immediately after inserting Loading row placeholder into table, before value comes from database
    static void RowLoading(const Row&, Table*) {}
    //! Called immediately after adding row to table from database, overwriting a placeholder
    static void RowLoaded(const Row&, Table*) {}
    //! Called immediately after marking row in table as stale
    static void RowStale(const Row&, Table*) {}
    //! Called immediately after updating row in table; first row is old value, second is new
    static void RowUpdated(const Row&, const Row&, Table*) {}
    //! Called immediately after deleting row from table
    static void RowDeleted(const Row&, Table*) {}

    //! Called immediately after inserting DraftAdd row into table
    static void RowDraftAdded(const Row&, Table*) {}
    //! Called immediately after updating row with draft edits; first row is old value, second is new
    static void RowDraftEdited(const Row&, const Row&, Table*) {}
    //! Called immediately after marking row as DraftDelete
    static void RowDraftDeleted(const Row&, Table*) {}

    //! Called immediately after resetting draft or pending changes
    static void LocalChangesReset(Table*) {}
    //! Called immediately after changing draft changes to pending
    static void DraftChangesPending(Table*) {}

    //! Called immediately after adding pending new row to table from database; receives draft row ID and new row
    static void PendingAddSettled(RowId, const Row&, Table*) {}
    //! Called immediately after updating pending edited row from database; first row is old value, second is new
    static void PendingEditSettled(const Row&, const Row&, Table*) {}
    //! Called immediately after deleting pending delete row from table
    static void PendingDeleteSettled(const Row&, Table*) {}
    //! Called immediately after updating draft row to table from database; first row is draft value, second is new
    static void DraftRowInvalidated(const Row&, const std::optional<Row>&, Table*) {}
};

//! This template manages the backups of rows and their load states.
template<class Row, typename=void>
class BackupManager {
    QList<Row> rows;
    QList<LoadState> states;

    template<typename C, typename... Args>
    using ReturnType = decltype(std::declval<C>()(std::declval<Args>()...));

public:
    bool isEmpty() const { return rows.isEmpty(); }
    QList<Row> getRows() const { return rows; }
    void clear() { rows.clear(); states.clear(); }

    std::optional<std::tuple<Row, LoadState>> get(RowId<Row> id) const {
        auto backupItr = std::lower_bound(rows.begin(), rows.end(), id, CompareId<Row>());
        if (backupItr != rows.end() && backupItr->getId() == id)
            return std::make_tuple(*backupItr, states[backupItr-rows.begin()]);
        return {};
    }
    bool remove(RowId<Row> id, Row* rowValue = nullptr, LoadState* rowState = nullptr) {
        auto backupItr = std::lower_bound(rows.begin(), rows.end(), id, CompareId<Row>());
        if (backupItr != rows.end() && backupItr->getId() == id) {
            auto stateItr = states.begin() + (backupItr - rows.begin());
            if (rowState != nullptr)
                *rowState = *stateItr;
            if (rowValue != nullptr)
                *rowValue = *backupItr;
            states.erase(stateItr);
            rows.erase(backupItr);

            return true;
        }
        return false;
    }
    void save(const Row& row, LoadState rowState) {
        auto editItr = std::lower_bound(rows.begin(), rows.end(), row.getId(), CompareId<Row>());
        if (editItr == rows.end() || editItr->getId() != row.getId()) {
            states.insert(editItr-rows.begin(), rowState);
            rows.insert(editItr, row);
        }
    }

    template<typename Callable, typename Ret = ReturnType<Callable, Row>>
    auto forEach(Callable&& c) const { return std::for_each(rows.begin(), rows.end(), std::forward<Callable>(c)); }
    template<typename Callable, typename Ret = ReturnType<Callable, Row, LoadState, std::function<void()>>>
    auto forEach(Callable&& c) {
        int i = 0;
        auto deleter = [this, &i] { rows.removeAt(i); states.removeAt(i); --i; };
        for (; i < rows.length(); ++i)
            c(rows[i], states[i], deleter);
        return c;
    }
};
