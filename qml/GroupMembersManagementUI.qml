import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14

import Pollaris.Utilities 1.0

Window {
    id: groupMemberUI
    width: 610
    height: 790

    property var group
    property TableInterface groupMembersTable: context.blockchain.getGroupMembersTable(group.id)
    property TableModel groupMembersModel: groupMembersTable.allRows()
    property real uiSpacing: 20

    Item {
        id: rootItem
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: groupMemberUI.close()

        ListView {
            id: groupMembersListView
            anchors.fill: parent
            anchors.margins: uiSpacing
            spacing: uiSpacing

            headerPositioning: ListView.OverlayHeader
            header: ListHeader {
                id: headerItem
                width: parent.width

                ColumnLayout {
                    width: parent.width
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%1: Group Members").arg(group.name)
                        horizontalAlignment: Qt.AlignHCenter
                        elide: Text.ElideRight
                        font.pointSize: 20
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: newVoterNameField
                            placeholderText: qsTr("Name")
                        }
                        SpinBox {
                            id: newVoterWeightField
                        }
                        Item { Layout.fillWidth: true }
                        RoundButton {
                            id: addButton
                            Layout.preferredWidth: 100

                            property bool newVoter: !groupMembersTable.findRowIf(function(row) {
                                return row.account === newVoterNameField.text })

                            Behavior on text {
                                SequentialAnimation {
                                    PropertyAnimation {
                                        target: addButton; property: "opacity"; from: 1; to: 0; duration: 100 }
                                    PropertyAction { target: addButton; property: "text" }
                                    PropertyAnimation {
                                        target: addButton; property: "opacity"; from: 0; to: 1; duration: 100 }
                                }
                            }

                            text: newVoter? qsTr("Add") : qsTr("Update")
                            onClicked: {
                                let args = {}
                                args[strings.GroupName] = group.name
                                args[strings.Voter] = newVoterNameField.text
                                args[strings.Weight] = newVoterWeightField.value
                                args[strings.Tags] = []
                                context.transactionManager.addAction(strings.VoterAdd, args)
                            }
                        }
                    }
                }
            }

            model: groupMembersModel
            delegate: Rectangle {
                color: "lightgrey"
                width: parent.width
                height: memberLayout.height + uiSpacing
                radius: height/8
                enabled: !deleted

                property string name: model.account
                property int weight: model.weight
                property int loadState: model.loadState
                property bool deleted: loadState === Pollaris.PendingDelete || loadState === Pollaris.DraftDelete

                BigX {
                    id: bigX
                    anchors.fill: parent
                    opacity: .3
                    radius: parent.radius
                    visible: parent.deleted
                    z: 3
                }

                StatusRibbon { status: model.loadState }
                RowLayout {
                    id: memberLayout
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        margins: uiSpacing / 2
                    }

                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.minimumWidth: 80

                        Text {
                            text: "Name: " + name
                            elide: Text.ElideRight
                            font.pointSize: 11
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "Weight: " + weight
                            elide: Text.ElideRight
                            font.pointSize: 11
                            Layout.fillWidth: true
                        }
                    }
                    Item { Layout.fillWidth: true }
                    RoundButton {
                        text: "Remove"
                        onClicked: {
                            let args = {}
                            args[strings.GroupName] = group.name
                            args[strings.Voter] = name
                            context.transactionManager.addAction(strings.VoterRemove, args)
                        }
                    }
                }
            }
        }
    }
}
