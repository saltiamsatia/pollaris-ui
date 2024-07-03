import QtQuick 2.14

Item {
    id: headerItem
    height: visibleHeader.childrenRect.height + faderHeight
    z: 10
    
    property real faderHeight: uiSpacing/2
    property color faderColor: "white"
    default property alias content: visibleHeader.data
    
    Rectangle {
        // Hide list items as they scroll up beneath the header, and off the top of the window
        id: headerRect
        x: 0
        // y is the UI's top, not the parent's top
        y: mapFromGlobal(0, 0).y
        width: parent.width
        height: parent.mapToGlobal(0, 0).y + headerItem.height
        
        property real percentFaded: headerItem.faderHeight/height
        
        // Add a nice fade-out effect as list items scroll up from view
        gradient: Gradient {
            GradientStop { position: 0.0; color: headerItem.faderColor }
            GradientStop { position: 1.0-headerRect.percentFaded; color: headerItem.faderColor }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
    Item { id: visibleHeader; width: parent.width }
}
