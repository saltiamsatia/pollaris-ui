import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14

import Pollaris.Utilities 1.0

Rectangle {
    color: open? "gray" : "lightgray"
    height: transactionLayout.height + uiSpacing
    radius: uiSpacing

    property MutableTransaction trx
    property int status
    property bool open: false
    property bool runButtonVisible: status === Pollaris.Draft
    property bool deleteButtonEnabled: status !== Pollaris.Authorizing && status !== Pollaris.Pending
    property bool canDeleteActions: runButtonVisible

    signal transactionClicked()
    signal deleteActionClicked(int index)
    signal deleteClicked()
    signal runClicked()

    function statusString() {
        switch(status) {
        case Pollaris.Draft:
            return "Draft"
        case Pollaris.Authorizing:
            return "Authorizing"
        case Pollaris.Pending:
            return "Pending"
        case Pollaris.Confirmed:
            return "Confirmed"
        case Pollaris.Irreversible:
            return "Finalized"
        case Pollaris.Failed:
            return "Failed"
        default:
            return "Unknown"
        }
    }
    
    MouseArea { anchors.fill: parent; onClicked: transactionClicked() }
    ColumnLayout {
        id: transactionLayout
        width: parent.width-uiSpacing/2
        anchors.centerIn: parent

        RowLayout {
            Layout.fillWidth: true

            RoundButton {
                text: "X"
                enabled: deleteButtonEnabled
                onClicked: deleteClicked()
            }
            Text {
                text: qsTr("Status: %1").arg(statusString())
            }
            Item { Layout.fillWidth: true }
            Text {
                text: (trx.actions.length === 1? qsTr("%1 action") : qsTr("%1 actions")).arg(trx.actions.length)
            }
            Item { Layout.fillWidth: true }
            RoundButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Run")
                enabled: trx.actions.length > 0
                visible: runButtonVisible
                onClicked: runClicked()
            }
        }

        GridLayout {
            id: actionLayout
            visible: open
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            columnSpacing: uiSpacing
            rowSpacing: uiSpacing
            columns: transactionLayout.width / 170

            Repeater {
                Layout.fillWidth: true
                model: trx.actions
                delegate: Rectangle {
                    radius: height / 8
                    Layout.preferredWidth: 175
                    Layout.preferredHeight: 120

                    Action {
                        id: action
                        actionName: modelData.actionName
                        arguments: modelData.arguments
                    }

                    RoundButton {
                        anchors {
                            top: parent.top
                            left: parent.left
                            margins: uiSpacing/4
                        }
                        text: "X"
                        visible: canDeleteActions
                        onClicked: deleteActionClicked(index)
                    }
                    Text {
                        anchors.fill: parent
                        anchors.margins: uiSpacing / 4
                        horizontalAlignment: Qt.AlignHCenter
                        verticalAlignment: Qt.AlignVCenter
                        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                        text: {
                            // Include these in the binding so they trigger a refresh
                            var makeItUpdate = action.actionName + action.arguments
                            return action.describe(25, context? context.blockchain : null,
                                                   function (txt) { text = txt })
                        }
                    }
                }
            }
        }
    }
}
