import QtQuick 2.15

import Qappa 1.0

Blank {
    id: currentBlank
    //! The position of the divider between the two frames, as a ratio of screen width
    property real dividerPosition: .5

    Frame {
        id: leftFrame
        anchors {
            top: parent.top
            bottom: parent.bottom
            left: parent.left
        }
        width: currentBlank.width * dividerPosition
    }
    Frame {
        id: rightFrame
        anchors {
            top: parent.top
            bottom: parent.bottom
            right: parent.right
        }
        width: currentBlank.width * (1-dividerPosition)
    }
}
