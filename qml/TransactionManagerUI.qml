import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14

import Qt.labs.settings 1.1

import Pollaris.Utilities 1.0
import "JsUtils.js" as Utils

Window {
    id: transactionManagerUI
    width: 610
    height: 790

    property real uiSpacing: 20
    property MutableTransaction activeTransaction: activeItem()? activeItem().trx : null
    property alias activeTransactionIndex: transactionList.currentIndex
    property bool disableSave: false
    property bool locked: tableEditController.changesPending

    Connections {
        target: context
        onBlockchainChanged: {
            if (context.blockchain) {
                settings.load()
                enabled = false
            }
        }
    }

    function createTransaction(setActive = true) {
        if (locked)
            return null
        var trx = context.blockchain.createTransaction()
        console.info("TransactionManager: creating transaction")
        if (trx) {
            transactionsModel.insert(0, {"trx": trx})
            transactionsModel.setProperty(0, "status", Pollaris.Draft)
            if (setActive)
                activeTransactionIndex = 0
            settings.save()
            return transactionsModel.get(0)
        } else {
            console.error("Unable to create a transaction!")
            return null
        }
    }
    function activeItem(createIfNone = false, setActive = true) {
        // The check if index is greater than count is unlikely to ever come up true, but it causes bindings to
        // re-evaluate if the model changes. Without it, deleting the active transaction doesn't refresh bindings
        if (activeTransactionIndex === -1 || activeTransactionIndex > transactionsModel.count) {
            if (createIfNone && !locked)
                return createTransaction(setActive)
            return null
        }
        return transactionsModel.get(activeTransactionIndex)
    }
    function addAction(name, args) {
        if (locked) {
            console.warn("TransactionManager: Cannot add actions while a transaction is pending")
            return false
        }

        var trx = activeItem(true)

        if (trx.status !== Pollaris.Draft)
            trx = createTransaction(true)

        trx.trx.addAction(name, args)

        if (!visible)
            show()
        requestActivate()
        settings.save()
        return true
    }

    KeyManager {
        id: keyManager
        Component.onCompleted: {
            if (context) {
                setBlockchain(context.blockchain)
            }
        }
    }
    Settings {
        id: settings
        category: "TransactionManager"

        function save() {
            if (disableSave) return
            var trxs = []
            for (var i = 0; i < transactionsModel.count; ++i)
                if (transactionsModel.get(i).status === Pollaris.Draft)
                    trxs.push(transactionsModel.get(i).trx.actions)
            settings.setValue("trxs", JSON.stringify(trxs))
            console.info("Saved transactions: " + JSON.stringify(trxs))
        }
        function load() {
            disableSave = true
            var trxs = JSON.parse(settings.value("trxs", "[]"))
            console.info("Loading transactions: " + JSON.stringify(trxs))
            while (trxs.length !== 0) {
                var trx = transactionManagerUI.createTransaction(false)
                var actions = trxs.pop()
                while (actions.length !== 0) {
                    var action = actions.shift()
                    trx.trx.addAction(action.actionName, action.arguments)
                }
            }
            console.info("Loaded " + transactionsModel.count + " transactions")
            disableSave = false
        }
    }
    ListModel {
        id: transactionsModel
    }
    TableEditController {
        id: tableEditController
        actions: transactionManagerUI.activeTransaction? transactionManagerUI.activeTransaction.actions : []
    }

    Item {
        id: rootItem
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: transactionManagerUI.close()

        ListView {
            id: transactionList
            anchors.fill:  parent
            anchors.margins: uiSpacing/2
            spacing: uiSpacing

            headerPositioning: ListView.OverlayHeader
            header: ListHeader {
                width: parent.width

                Text {
                    width: parent.width
                    font.pointSize: 20
                    text: qsTr("Transactions")
                    horizontalAlignment: Qt.AlignHCenter
                }
                RoundButton {
                    anchors.right: parent.right
                    text: "Create"
                    visible: !locked
                    onClicked: createTransaction()
                }
            }

            model: transactionsModel
            delegate: Transaction {
                width: parent.width
                trx: model.trx
                status: model.status
                open: isActiveTransaction

                property bool isActiveTransaction: index === transactionManagerUI.activeTransactionIndex

                onTransactionClicked: if (!locked) transactionManagerUI.activeTransactionIndex = index
                onDeleteActionClicked: {
                    trx.deleteAction(index)
                    settings.save()
                }
                onDeleteClicked: {
                    var modelBak = Object.assign({}, model)
                    transactionsModel.remove(index)
                    settings.save()

                    // Wait 1 second for GUI to settle, then destroy the transactions
                    Utils.setTimeout(1000, function() {
                        modelBak.trx.destroy()
                        if (modelBak.authTrx) modelBak.authTrx.destroy()
                        if (modelBak.pendingTrx) modelBak.pendingTrx.destroy()
                    })
                }
                onRunClicked: {
                    console.info("TransactionManager: Running transaction")
                    var component = Qt.createComponent("TransactionConfirmationUI.qml")
                    model.authTrx = keyManager.prepareForSigning(model.trx)
                    model.status = Pollaris.Authorizing
                    var confirmationWindow = component.createObject(transactionManagerUI,
                                                                    {"transaction": model.authTrx})
                    confirmationWindow.accepted.connect(function() {
                        keyManager.signTransaction(model.authTrx)
                        model.pendingTrx = keyManager.prepareForBroadcast(model.authTrx)
                        model.pendingTrx.statusChanged.connect(updateStatus)
                        updateStatus()
                        tableEditController.transactionSubmitted()
                        blockchain.submitTransaction(model.pendingTrx)
                        settings.save()
                        confirmationWindow.destroy()
                    })
                    confirmationWindow.canceled.connect(function() {
                        model.status = Pollaris.Draft
                        model.authTrx.destroy()
                        model.authTrx = null
                        confirmationWindow.destroy()
                    })
                    confirmationWindow.show();
                    settings.save()
                }
                onStatusChanged: {
                    if (status === Pollaris.Failed) {
                        // TODO: Ideally we notify the user and advise them on how to proceed, but for now...
                        console.info("TransactionManager: Transaction failed! Resetting edits")
                        tableEditController.resetEdits()
                    }
                }

                function updateStatus() { if (!!model.pendingTrx) model.status = Number(model.pendingTrx.status) }
            }
        }
    }
}
