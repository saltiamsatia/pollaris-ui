import QtQuick 2.14

Rectangle {
    id: bigX
    color: "black"
    layer.enabled: true
    Rectangle { anchors.centerIn: parent; width: parent.width / 4; height: 10; rotation: 20 }
    Rectangle { anchors.centerIn: parent; width: parent.width / 4; height: 10; rotation: -20 }
}
