import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14

import Pollaris.Utilities 1.0

Window {
    id: transactionConfirmationWindow
    width: 400
    height: 550

    property SignableTransaction transaction

    signal canceled
    signal accepted

    onCanceled: close()
    onAccepted: close()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Would you like to sign and broadcast this transaction?"
        }
        ListView {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.fillHeight: true

            model: !!transaction? transaction.json.actions : []
            delegate: Text {
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                Action {
                    id: action
                }

                text: {
                    action.loadJson(modelData)
                    return action.describe(25, (context && context.blockchain)? context.blockchain : null,
                                           function(txt) { text = txt })
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true

            Button { text: "Cancel"; onClicked: transactionConfirmationWindow.canceled() }
            Item { Layout.fillWidth: true }
            Button { text: "Broadcast"; onClicked: transactionConfirmationWindow.accepted() }
        }
    }
}
