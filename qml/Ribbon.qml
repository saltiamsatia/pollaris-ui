import QtQuick 2.14

Item {
    anchors.top: parent.top
    anchors.right: parent.right
    width: outerTopLeg
    height: outerRightLeg
    clip: true
    z: 5

    property alias text: label.text
    property alias font: label.font
    property alias color: ribbon.color
    property alias textColor: label.color
    property alias ribbonHeight: ribbon.height
    property alias ribbonOpacity: ribbon.opacity
    
    property real labelPadding: label.width / 3
    property real innerHypotenuse: label.width + labelPadding
    property real innerRightLeg: innerHypotenuse / 2
    property real innerTopLeg: Math.sqrt(Math.pow(innerHypotenuse, 2) - Math.pow(innerRightLeg, 2))
    property real outerRightLeg: innerRightLeg + 2*Math.sqrt(Math.pow(ribbon.height, 2)/3)
    property real outerHypotenuse: outerRightLeg * 2
    property real outerTopLeg: Math.sqrt(Math.pow(outerHypotenuse, 2) - Math.pow(outerRightLeg, 2))
    
    Rectangle {
        id: ribbon
        height: label.height
        width: outerHypotenuse * 1.6
        x: parent.width - width/2 - label.width/2 - (parent.innerHypotenuse - label.width)/2
        y: parent.innerRightLeg
        transform: Rotation {
            angle: 30
            origin.x: ribbon.width/2 + label.width/2 + labelPadding/2
        }
    }

    Text {
        id: label
        anchors.centerIn: ribbon
        transform: Rotation {
            angle: 30
            origin.x: label.width + labelPadding/2
        }
    }
}
