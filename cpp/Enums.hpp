#pragma once

#include <QObject>

//! \brief A class to hold the various Pollaris enumeration types
class Pollaris {
    Q_GADGET

public:
    /*!
     * \enum Pollaris::LoadState
     * \brief Specifies the status of loading a particular row of a table
     *
     * \value Loaded Signifies that the row is loaded and believed to match the backend's record
     * \value Loading Signifies that the row is not yet loeaded
     * \value Stale Signifies that the row is loaded, but the cached value is believed to be outdated
     * \value DraftEdit Signifies that the row has draft modifications which are not yet saved to the database
     * \value DraftAdd Signifies that the row is added in a draft edit
     * \value DraftDelete Signifies that the row is deleted in a draft edit
     * \value PendingEdit Signifies that the row has pending modifications which are submitted but not yet confirmed
     * \value PendingAdd Signifies that the row is added in a pending edit
     * \value PendingDelete Signifies that the row is deleted in a pending edit
     */
    enum class LoadState {
        Loaded,
        Loading,
        Stale,
        DraftEdit,
        DraftAdd,
        DraftDelete,
        PendingEdit,
        PendingAdd,
        PendingDelete
    };
    Q_ENUM(LoadState)

    /*!
     * \enum Pollaris::TransactionStatus
     * \brief Specifies the status of a transaction
     *
     * \value Draft A transaction which is still under construction and is not yet ready to sign
     * \value Authorizing A transaction which the user has indicated should run and is waiting for signatures
     * \value Pending A transaction which has been signed and is ready for broadcast, or has been broadcast
     * \value Confirmed A transaction which is included in a block, but has not yet reached irreversibility
     * \value Irreversible A transaction which is included in an irreversible block
     * \value Failed A transaction which has failed, whether due to an error or expiration prior to confirmation
     * \value Unknown A transaction whose state is unknown
     */
    enum class TransactionStatus {
        Draft,
        Authorizing,
        Pending,
        Confirmed,
        Irreversible,
        Failed,
        Unknown
    };
    Q_ENUM(TransactionStatus)
};

using LoadState = Pollaris::LoadState;
Q_DECLARE_METATYPE(LoadState)
using TransactionStatus = Pollaris::TransactionStatus;
Q_DECLARE_METATYPE(TransactionStatus)

template<typename Enum, std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
inline uint qHash(Enum e) { return qHash(static_cast<int>(e)); }
