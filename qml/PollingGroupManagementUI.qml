import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.14

import Pollaris.Utilities 1.0

Window {
    id: pollingGroupUI
    width: 610
    height: 790

    property real uiSpacing: 20
    property TableInterface groupsTable: context? context.blockchain.getPollingGroupTable() : null
    property TableModel groupsModel: groupsTable? groupsTable.allRows() : null

    function openGroup(group) {
        var component = Qt.createComponent("GroupMembersManagementUI.qml")
        var membersWindow = component.createObject(pollingGroupUI, {"group": group})
        membersWindow.show();
    }

    Item {
        id: rootItem
        anchors.fill: parent
        focus: true

        Keys.onEscapePressed: pollingGroupUI.close()

        ListView {
            id: pollingGroupsListView
            anchors.fill: parent
            anchors.margins: uiSpacing/2
            spacing: uiSpacing

            headerPositioning: ListView.OverlayHeader
            header: ListHeader {
                width: parent.width

                RowLayout {
                    width: parent.width
                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Polling Groups")
                        font.pointSize: 20
                    }
                }
            }

            model: groupsModel
            delegate: Rectangle {
                id: groupRect
                color: groupMouseArea.containsMouse? Qt.lighter("lightgrey", 1.04) : "lightgrey"
                width: parent.width
                height: groupLayout.height + uiSpacing
                radius: height/8

                property var loadState: model.loadState
                property var groupId: model.id
                property string groupName: model.name
                property var groupSize: model.groupSize
                property bool locallyEdited: [Pollaris.DraftAdd, Pollaris.DraftEdit, Pollaris.DraftDelete,
                                              Pollaris.PendingAdd, Pollaris.PendingEdit, Pollaris.PendingDelete]
                                                .indexOf(groupRect.loadState) >= 0

                MouseArea {
                    id: groupMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: openGroup(Object.assign({}, model))
                }

                StatusRibbon {
                    status: groupRect.loadState
                }
                RowLayout {
                    id: groupLayout
                    width: parent.width - uiSpacing*2
                    anchors.centerIn: parent
                    spacing: uiSpacing

                    Text {
                        text: groupId < groupsTable.baseDraftId? "#" + groupId : qsTr("TBD")
                        font.pointSize: 25
                    }
                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.minimumWidth: 80
                        Text {
                            text: qsTr("Name: %1").arg(model.name)
                            elide: Text.ElideRight
                            font.pointSize: 11
                            Layout.fillWidth: true
                        }
                        Text {
                            text: {
                                var status = groupRect.groupSize.status
                                var size = groupRect.groupSize.value
                                if (status === Pollaris.Loading)
                                    return qsTr("Loading members...")
                                return (size !== 1? qsTr("%1 members") : qsTr("%1 Member")).arg(size)
                            }
                            elide: Text.ElideRight
                            font.pointSize: 11
                            Layout.fillWidth: true
                        }
                    }
                    Item { Layout.fillWidth: true }
                    FieldButton {
                        id: copyButton
                        buttonText: "Copy"
                        placeholderText: "New name"

                        Keys.onEscapePressed: open = false
                        onButtonClicked: open = !open
                        onOpened: renameButton.close()
                        onClosed: fieldText = ""
                        onFieldAccepted: {
                            var collision = groupsTable.findRowIf(function(row) { return row.name === fieldText })
                            if (fieldText !== "" && !collision) {
                                var args = {}
                                args[strings.GroupName] = groupName
                                args[strings.NewName] = fieldText
                                context.transactionManager.addAction(strings.GroupCopy, args)
                                open = false
                            }
                        }
                    }
                    FieldButton {
                        id: renameButton
                        buttonText: "Rename"
                        placeholderText: "New name"

                        Keys.onEscapePressed: open = false
                        onButtonClicked: open = !open
                        onOpened: copyButton.close()
                        onClosed: fieldText = ""
                        onFieldAccepted: {
                            var collision = groupsTable.findRowIf(function(row) { return row.name === fieldText })
                            if (fieldText !== "" && !collision) {
                                var args = {}
                                args[strings.GroupName] = groupName
                                args[strings.NewName] = fieldText
                                context.transactionManager.addAction(strings.GroupRename, args)
                                open = false
                            }
                        }
                    }
                }
            }
        }
    }
}
