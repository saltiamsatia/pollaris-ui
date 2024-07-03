import QtQuick 2.14

import Pollaris.Utilities 1.0
import "JsUtils.js" as Utils

Item {
    id: tableEditController

    enum EditType {
        AddRow,
        EditRow,
        DeleteRow
    }
    
    property BlockchainInterface blockchain: (context && context.blockchain)? context.blockchain : null
    property var actions: []

    //! \qmlproperty edits
    //! List of lists, parallel to actions, where each sublist looks like:
    //! [{"type": TableEditController.AddRow, "table": ptr, "fields": {"field1": value1, "field2": value2...},
    //!   "settled": false},
    //!  {"type": TableEditController.EditRow, "table": ptr, "id": "RowId",
    //!   "fields": {"field1": value1, "field2": value2...}, "settled": false},
    //!  {"type": TableEditController.DeleteRow, "table": ptr, "id": "RowId", "settled": false}]
    //! Notes:
    //!  - Once settled, edits may receive an additional "unexpectedValues" object if the value from the backend was
    //! not as predicted, i.e. {..., "unexpectedValues": {"field1": "Unexpected value"}}
    //! - Edits of type AddRow and EditRow may have their fields modified to match the final value if subsequent
    //! edits are made to the added row. When such an edit is made, an object of the form {"values": {copy of
    //! "fields"}, "until": {"action": Number, "edit": Number}} will be appended to an array property
    //! "originalFields", storing the values of the fields prior to the modification, and the indices into the edits
    //! array of the modifying edit
    //! - Edits of type AddRow and EditRow may become settled before the transaction runs if the row being edited is
    //! deleted later in the transaction. In this case, the auto-settled edits will gain an object property called
    //! "settledBy" of the form {"action": Number, "edit": Number} storing the indices into the edits array of the
    //! DeleteRow affecting their row.
    property var edits: []
    onEditsChanged: console.info(`Edits:\n${describeEdits()}`)
    property bool changesPending: data.editedTables.some(function(table){ return table.hasPendingEdits; })
    onChangesPendingChanged: console.info("TEC: Changes pending: " + changesPending)
    property bool editsSettled: {
        return edits.every(function(editList) { return editList.every(function(edit) { return edit.settled; }); })
    }
    onEditsSettledChanged: console.info("TEC: Edits settled: " + editsSettled)

    QtObject {
        id: data

        property var editedTables: []
        onEditedTablesChanged: {
            var tables = "TEC: Edited tables:\n"
            editedTables.forEach(function(table) { tables += ` -> ${tableToString(table)}\n`; })
            console.info(tables)
        }
        property var invalidatedTimerReset

        function tableToString(table) { return table.tableName + "[" + table.tableScope + "]" }
        function editInvalidated(table, id) {
            if (!changesPending) {
                console.info(`A draft edit was invalidated in table ${tableToString(table)} with ID ${id}. ` +
                            "Resetting edits")

                // TODO: The proper thing to do here is notify the user and ask them before taking action...
                // but so we can test the underlying tech first:
                // Delay before resetting to let the tables settle completely
                if (invalidatedTimerReset)
                    // If timer is already running, just restart it
                    invalidatedTimerReset()
                invalidatedTimerReset = Utils.setTimeout(200, actionsChanged)
            }
        }
        function tableSettled(table) {
            // Find the table in the editedTables array
            let tableIndex = editedTables.indexOf(table)
            if (tableIndex === -1) return
            // Remove the table from the array
            editedTables.splice(tableIndex, 1)
            // Grab our connections off the table and disconnect them
            let connections = table.dnmx.TECConnections
            if (connections) {
                table.draftEditInvalidated.disconnect(connections.editInvalidated)
                table.pendingEditSettled.disconnect(connections.editSettled)
                table.hasPendingEditsChanged.disconnect(connections.pendingEditsChanged)
            }
            delete table.dnmx.TECConnections
        }
        function tableEdited(table) {
            if (editedTables.indexOf(table) !== -1)
                return
            // Define signal handlers
            let invalidatedHandler = function(id) { editInvalidated(table, id); }
            let settledHandler = function(from, to) { settleEdit(table, from, to); }
            let pendingChangedHandler = function(hasPendingEdits) {
                if (!hasPendingEdits) {
                    console.info(`Table ${tableToString(table)} no longer pending. Removing from edited tables`)
                    tableSettled(table)
                }
            }
            let connections = {"editInvalidated": invalidatedHandler, "editSettled": settledHandler,
                               "pendingEditsChanged": pendingChangedHandler}

            // Connect to table's signals
            table.draftEditInvalidated.connect(connections.editInvalidated)
            table.pendingEditSettled.connect(connections.editSettled)
            table.hasPendingEditsChanged.connect(connections.pendingEditsChanged)
            // Attach our connections to table's DNMX property and add table to editedTables
            table.dnmx.TECConnections = connections
            editedTables.push(table)
            editedTablesChanged()
        }
        function beginAction() {
            edits.push([])
        }
        function addRow(table, fields) {
            var edit = {"type": TableEditController.AddRow, "table": table, "fields": fields, "settled": false}
            table.draftAddRow(edit.fields)
            tableEdited(table)
            edits[edits.length-1].push(edit)
        }
        function editRow(table, id, fields) {
            var edit = {"type": TableEditController.EditRow, "table": table,
                        "id": id, "fields": fields, "settled": false}
            var position = {"action": edits.length-1, "edit": edits[edits.length-1].length}

            var row = table.getRow(id)
            if (!!row && row.loadState === Pollaris.DraftAdd) {
                // We're editing a row we added in this same transaction. Apply fields to the earlier edits so we can
                // still match them against the row from the database at the end
                edits.forEach(function(editList) { editList.forEach(function(edit) {
                    if ((edit.type === TableEditController.AddRow && Utils.matchFields(edit.fields, row)) ||
                            (edit.type === TableEditController.EditRow && edit.id === id)) {
                        // Mark that we've edited the originally added value
                        var oldFieldsRecord = {"values": Utils.deepCopy(edit.fields), "until": position}
                        if (!!edit.originalFields) edit.originalFields.push(oldFieldsRecord)
                        else edit.originalFields = [oldFieldsRecord]
                        Object.assign(edit.fields, fields)
                        edit.editedBy = position
                    }
                }); })
            }

            table.draftEditRow(edit.id, edit.fields)
            tableEdited(table)
            edits[edits.length-1].push(edit)
        }
        function deleteRow(table, id) {
            var edit = {"type": TableEditController.DeleteRow, "table": table, "id": id, "settled": false}
            var position = {"action": edits.length-1, "edit": edits[edits.length-1].length}

            var row = table.getRow(id)
            if (!!row && row.loadState === Pollaris.DraftAdd) {
                // We're deleting a row we added in this same transaction. Mark both the add and delete edits settled
                edit.settled = true
                edits.forEach(function(editList) { editList.forEach(function(edit) {
                    if ((edit.type === TableEditController.AddRow && Utils.matchFields(edit.fields, row)) ||
                            (edit.type === TableEditController.EditRow && edit.id === id)) {
                        edit.settled = true
                        edit.settledBy = position
                    }
                }); })
            }

            table.draftDeleteRow(edit.id)
            tableEdited(table)
            edits[edits.length-1].push(edit)
        }

        function processVoterAdd(groupName, voter, weight) {
            // Find polling group, create if necessary
            let groupTable = blockchain.getPollingGroupTable()
            let groupRow = groupTable.findRowIf(function(row) { return row.name === groupName; })
            if (!groupRow) {
                // Group doesn't exist; create it
                let fields = {}
                fields[strings.Name] = groupName
                addRow(groupTable, fields)
                groupRow = groupTable.findRowIf(function(row) { return row.name === groupName; })
                console.info(`TEC: [voter.add] Draft added polling group ${JSON.stringify(groupRow)}`)
                if (!groupRow) {
                    // Creation failed -- unable to continue
                    console.error("TEC: [voter.add] Unable to create draft polling group")
                    return false
                }
            }

            // Does voter exist already?
            let accountsTable = blockchain.getGroupMembersTable(groupRow.id)
            let accountRow = accountsTable.findRowIf(function(row) { return row.account === voter; })
            if (!!accountRow) {
                // Voter exists; are we changing his weight?
                if (accountRow[strings.Weight] === weight)
                    // No, so this will fail
                    return false
                // Update the voter's weight in place
                let fields = {}
                fields[strings.Weight] = weight
                editRow(accountsTable, voter, fields)
                console.info(`TEC: [voter.add] Draft edited voter ${voter} in group ${groupName}`)
            } else {
                // Voter does not exist; add him
                let fields = {}
                fields[strings.Account] = voter
                fields[strings.Weight] = weight
                addRow(accountsTable, fields)
                console.info(`TEC: [voter.add] Draft added voter ${voter} to group ${groupName}`)
            }

            return true
        }
        function processVoterRemove(groupName, voter) {
            // Find the polling group
            let groupTable = blockchain.getPollingGroupTable()
            let groupRow = groupTable.findRowIf(function(row) { return row.name === groupName; })
            if (!groupRow) {
                // Can't find the polling group; unable to continue
                console.error(`TEC: [voter.remove] Could not find polling group ${groupName} to remove voter from`)
                return false
            }

            // Find the account
            let accountsTable = blockchain.getGroupMembersTable(groupRow[strings.Id])
            let accountRow = accountsTable.findRowIf(function(row) { return row[strings.Account] === voter; })
            if (!accountRow) {
                // Can't find the account; unable to continue
                console.error(`TEC: [voter.remove] Could not find voter ${voter} to remove from group ${groupName}`)
                return false
            }

            // Delete the account
            deleteRow(accountsTable, voter)
            return true
        }
        function processGroupRename(groupName, newName) {
            if (groupName === newName)
                // Names are the same; cannot continue
                return false

            // Find the polling group
            let groupTable = blockchain.getPollingGroupTable()
            let groupRow = groupTable.findRowIf(function(row) { return row[strings.Name] === groupName; })
            if (!groupRow) {
                // Can't find the polling group; unable to continue
                console.error(`TEC: [group.rename] Could not find polling group ${groupName} to rename`)
                return false
            }

            // Check the new name isn't already taken
            let newGroupRow = groupTable.findRowIf(function(row) { return row[strings.Name] === newName; })
            if (!!newGroupRow) {
                // New name is taken; cannot conntinue
                console.error(`TEC: [group.rename] New name ${newName} is already taken`)
                return false
            }

            // Update the group's name
            let fields = {}
            fields[strings.Name] = newName
            editRow(groupTable, groupRow[strings.Id], fields)
            return true
        }
        function processGroupCopy(groupName, newName) {
            if (groupName === newName)
                // Names are the same; cannot continue
                return false

            // Find the polling group
            let groupTable = blockchain.getPollingGroupTable()
            let groupRow = groupTable.findRowIf(function(row) { return row[strings.Name] === groupName; })
            if (!groupRow) {
                // Can't find the polling group; unable to continue
                console.error(`TEC: [group.copy] Could not find polling group ${groupName} to copy`)
                return false
            }

            // Check the new group's name isn't already taken
            let newGroupRow = groupTable.findRowIf(function(row) { return row[strings.Name] === newName; })
            if (!!newGroupRow) {
                // New name is taken; cannot conntinue
                console.error(`TEC: [group.copy] New name ${newName} is already taken`)
                return false
            }

            // Create the new group
            let fields = {}
            fields[strings.Name] = newName
            addRow(groupTable, fields)
            newGroupRow = groupTable.findRowIf(function(row) { return row[strings.Name] === newName; })
            if (!newGroupRow) {
                // Can't find new group; unable to continue
                console.error(`TEC: [group.copy] Unable to create new polling group ${newName}`)
                return false
            }

            // Add all locally known members from first group to second group
            let sourceAccountTable = blockchain.getGroupMembersTable(groupRow[strings.Id])
            let targetAccountTable = blockchain.getGroupMembersTable(newGroupRow[strings.Id])
            let sourceAccounts = sourceAccountTable.localRows()
            if (!!sourceAccounts) {
                sourceAccounts.forEach(function(a) { delete a.loadState; addRow(targetAccountTable, a); })
            }
            return true
        }

        function applyActions(actions) {
            actions.forEach(function(action) {
                beginAction()

                switch(action.actionName) {
                case strings.VoterAdd:
                    processVoterAdd(action.arguments[strings.GroupName], action.arguments[strings.Voter],
                                    action.arguments[strings.Weight])
                    break;
                case strings.VoterRemove:
                    processVoterRemove(action.arguments[strings.GroupName], action.arguments[strings.Voter])
                    break;
                case strings.GroupRename:
                    processGroupRename(action.arguments[strings.GroupName], action.arguments[strings.NewName])
                    break;
                case strings.GroupCopy:
                    processGroupCopy(action.arguments[strings.GroupName], action.arguments[strings.NewName])
                    break;
                }
            })
            editsChanged()
        }

        function idFieldName(tableName) {
            if (tableName === strings.PollGroups) return strings.Id
            if (tableName === strings.GroupAccts) return strings.Account
        }

        function settleEdit(table, from, to) {
            console.info("TEC: Edit settled in table; searching for corresponding pending edit for " +
                        JSON.stringify(from) + " => " + JSON.stringify(to))
            var found = false

            // Search through edits looking for a match: for each pending action...
            for (var actionIdx = 0; actionIdx !== edits.length; actionIdx++) {
                var editList = edits[actionIdx]
                // For each edit in that action...
                for (var editIdx = 0; editIdx !== editList.length; editIdx++) {
                    var edit = editList[editIdx]
                    // If the edit is settled already, it's not a match
                    if (edit.settled) continue;
                    // If the edit is for a different table, it's not a match
                    if (edit.table !== table) continue;

                    // Check if the from/to matches what we expect for this edit based on its type
                    let unexpectedValues = {}
                    switch(edit.type) {
                    case TableEditController.AddRow:
                        // Edit fields should match from exactly
                        if (!Object.keys(edit.fields).every(function(key) {
                            return edit.fields[key] === from[key]
                                    || JSON.stringify(edit.fields[key]) === JSON.stringify(from[key])
                        }))
                            continue;
                        // It's a match! Check for unexpected values
                        Utils.matchFields(from, to, unexpectedValues)
                        break;
                    case TableEditController.EditRow:
                        // The edited ID should match from's ID
                        if (edit.id !== from[idFieldName(edit.table.tableName)]) continue;
                        // It's a match! Check for unexpected values
                        Utils.matchFields(from, to, unexpectedValues)
                        break;
                    case TableEditController.DeleteRow:
                        // to should be simply {"deleted": true}
                        if (!to.DELETED) continue;
                        // The edited ID should match from's ID
                        if (edit.id !== from[idFieldName(edit.table.tableName)]) continue;
                        // It's a match! There's nothing to check, really
                        break;
                    }

                    // Mark the match
                    console.info(`TEC: Matched edits[${actionIdx}][${editIdx}] with a settled edit`)
                    found = true
                    edit.settled = true
                    if (Object.keys(unexpectedValues).length > 0) {
                        console.warn("Unexpected values in edit: " + JSON.stringify(Object.keys(unexpectedValues)))
                        edit.unexpectedValues = Object.assign({}, unexpectedValues)
                    }
                }
            }

            editsChanged()
            if (!found)
                console.warn("TEC: Could not find matching edit for change " + JSON.stringify(from) +
                             " => " + JSON.stringify(to))
        }
    }
    
    onActionsChanged: {
        console.info("Making local edits for actions: " + JSON.stringify(actions))
        resetEdits()
        data.applyActions(actions)
    }

    function resetEdits() {
        edits = []
        while (data.editedTables.length > 0) {
            var table = data.editedTables[0]
            table.resetEdits();
            data.tableSettled(table);
        }
        data.editedTablesChanged()
    }
    function transactionSubmitted() {
        data.editedTables.forEach(function(table) { table.markEditsPending(); })
    }
    function describeEdits() {
        var result = ""
        var actionNum = 1
        var describeFields = function(edit) {
            var description = JSON.stringify(edit.fields)
            var parenthetical = ""
            // If field originalFields exists, the fields field is edited to match the final state. The values at the
            // time of this action are the first element in the originalFields array.
            if (edit.hasOwnProperty("originalFields")) {
                description = JSON.stringify(edit.originalFields[0].values)
                parenthetical = `Later edited in Action ${edit.originalFields[0].until.action+1}, edit `
                        + (edit.originalFields[0].until.edit+1)
            }
            // If field settledBy exists, this edit was pre-settled by the action at the index specified in settledBy
            // At the time of writing, this only happens when the row was created and deleted in the same transaction
            if (edit.hasOwnProperty("settledBy")) {
                if (parenthetical) parenthetical += ", and settled "
                else parenthetical = "Settled "
                parenthetical += `in Action ${edit.settledBy.action+1}, edit ${edit.settledBy.edit+1}`
            }
            return (parenthetical? `${description} (${parenthetical})` : description)
        }

        edits.forEach(function(editList) {
            result += ` => Action ${actionNum++}:\n`
            editList.forEach(function(edit) {
                result += " ==> "
                if (edit.settled)
                    result += "[X]"
                else
                    result += "[ ]"
                result +=  ` <${data.tableToString(edit.table)}> `
                switch(edit.type) {
                case TableEditController.AddRow:
                    result += "Add " + describeFields(edit)
                    break;
                case TableEditController.EditRow:
                    result += `Edit ID: ${edit.id} -> ${describeFields(edit)}`
                    break;
                case TableEditController.DeleteRow:
                    result += `Delete ID: ${edit.id}`
                    break;
                }
                result += "\n"
            })
        })

        return result
    }
}
