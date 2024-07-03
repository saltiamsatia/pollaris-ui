#pragma once

#include <AbstractTable.hpp>
#include <BlockchainInterface.hpp>

#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>

class BlockchainInterface;

//! \brief A row in the backend's group.accts table
struct GroupMember {
    QString account;
    uint32_t weight;
    QStringList tags;

    QString getId() const { return account; }
    void setId(QString id) { account = id; }

    bool operator==(const GroupMember& b) const {
        return std::tie(account, weight, tags) == std::tie(b.account, b.weight, b.tags);
    }
    bool operator!=(const GroupMember& b) const { return !(*this == b); }
};
REFLECT_STRUCT(GroupMember, (account)(weight)(tags))

//! \brief A row from the backend's poll.groups table
struct PollingGroup {
    uint64_t id;
    QString name;
    QStringList tags;

    uint64_t getId() const { return id; }
    void setId(uint64_t id) { this->id = id; }

    bool operator==(const PollingGroup& b) const {
        return std::tie(id, name, tags) == std::tie(b.id, b.name, b.tags);
    }
    bool operator!=(const PollingGroup& b) const { return !(*this == b); }
};
REFLECT_STRUCT(PollingGroup, (id)(name)(tags))

//! Add QDebug support for reflected types
template<class Reflected, typename = std::enable_if_t<infra::reflector<Reflected>::is_defined::value>>
QDebug operator<<(QDebug qd, const Reflected& val) {
    using Reflector = infra::reflector<Reflected>;
    qd.nospace() << Reflector::type_name << "{";
    infra::typelist::runtime::for_each(typename Reflector::members(), [&qd, &val](auto Descriptor) {
        using descriptor = typename decltype(Descriptor)::type;
        qd << descriptor::get_name() << ": " << descriptor::get(val);
        if constexpr (!std::is_same_v<descriptor, infra::typelist::last<typename Reflector::members>>)
            qd << ", ";
    });
    qd << "}";
    return qd.space();
}

// Instantiate GroupMembers table
template<> struct TableName<GroupMember> {
    static const bool defined = true;
    constexpr static const QString* name = &Strings::GroupAccts;
};
using GroupMembersTable = AbstractTable<GroupMember>;

/*!
 * \brief A virtual field in \ref PollingGroupsTable which retrieves the number of members in the group
 */
class PollingGroupSizeField : public VirtualFieldInterface<PollingGroupSizeField, QVariant, PollingGroup> {
    using Base = VirtualFieldInterface<PollingGroupSizeField, QVariant, PollingGroup>;

    int64_t value = -1;

public:
    using Type = QVariant;
    using RowType = PollingGroup;
    constexpr static const char* role = "groupSize";

    PollingGroupSizeField(BlockchainInterface* blockchain) : Base(blockchain) {}

    template<typename ChangedSignal>
    void rowChanged(const PollingGroup& group, LoadState groupState, ChangedSignal&& signal) {
        // We only need to connect the signals once. If we've already connected, then do nothing.
        if (children.size() > 0) return;
        // If row is not yet loaded, the ID might not be valid yet. Wait until we're called again when it's loaded
        if (groupState == LoadState::Loading) return;

        // Get the group members table for this polling group ID
        auto table = dynamic_cast<GroupMembersTable*>(blockchain->getGroupMembersTable(group.id));
        if (table == nullptr) {
            qCritical("Failed to cast AbstractTableInterface to GroupMembersTable");
            return;
        }
        // Get a model for all rows in the table
        auto model = table->allRows();

        // When the value changes, recalculate it and cache it before emitting the signal
        auto onChanged = [this, signal=std::move(signal), model] {
            value = model->rowCount(QModelIndex());
            signal();
        };
        // If rows are inserted or removed from the model, that changes the polling group size
        QObject::connect(model, &QAbstractListModel::rowsInserted, onChanged);
        QObject::connect(model, &QAbstractListModel::rowsRemoved, onChanged);
        // Save the model. When this field is destroyed, model will be too.
        addChild(model);

        // Update the cached value now
        onChanged();
    }

    QPair<Type, LoadState> get(const PollingGroup&, LoadState groupState) {
        if (groupState == LoadState::Loading || children.isEmpty())
            return qMakePair(QVariant(), LoadState::Loading);

        return qMakePair(QVariant::fromValue(value), groupState);
    }
};

template<>
struct TableRowOperations<PollingGroup, void> : public TableRowOperations<PollingGroup, PollingGroup> {
    static void PendingAddSettled(uint64_t draftId, const PollingGroup& row, AbstractTableInterface* table) {
        // When a pending added polling group settles, the draft ID resolves to a real ID.
        // If a table existed for the group members, we need to update its scope to the new ID.
        if (draftId >= table->baseDraftId())
            table->getBlockchain()->rescopeGroupMembersTable(draftId, row.id);
    }
};

// Instantiate PollingGroups table
template<> struct TableName<PollingGroup> {
    static const bool defined = true;
    constexpr static const QString* name = &Strings::PollGroups;
};
template<>
struct VirtualFields<PollingGroup> { using type = infra::typelist::list<PollingGroupSizeField>; };
using PollingGroupsTable = AbstractTable<PollingGroup>;
