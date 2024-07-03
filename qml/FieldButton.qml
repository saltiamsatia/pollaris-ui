import QtQuick 2.14
import QtQuick.Controls 2.14

Item {
    id: fieldButton
    property alias buttonText: theButton.text
    property alias open: fieldSlider.open
    property alias fieldText: theField.text
    property alias placeholderText: theField.placeholderText
    property real openWidth: theButton.height*5

    implicitWidth: open? openWidth : theButton.width
    implicitHeight: theButton.height

    signal buttonClicked
    signal fieldAccepted
    signal opened
    signal closed

    function open() { open = true }
    function close() { open = false }

    Behavior on implicitWidth {
        SequentialAnimation {
            PropertyAnimation {}
            ScriptAction { script: {if (open) fieldButton.opened(); else fieldButton.closed() } }
        }
    }

    Item {
        id: fieldSlider
        visible: !!width
        x: theButton.width / 2
        width: open? openWidth-x : 0
        height: parent.height
        clip: true
        
        property bool open: false
        
        Behavior on width { PropertyAnimation {} }
        
        TextField {
            id: theField
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Qt.AlignRight
            onVisibleChanged: focus = visible
            onAccepted: fieldButton.fieldAccepted()
        }
    }
    RoundButton {
        id: theButton
        onClicked: fieldButton.buttonClicked()
    }
}
