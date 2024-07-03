import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14

import Qappa 1.0
import Pollaris.Utilities 1.0
import "JsUtils.js" as Utils

Window {
    id: assistantWindow
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    height: Math.min(seat.height + assistantDialogArea.height*.7, Screen.desktopAvailableHeight - assistantWindow.y)

    Component.onCompleted: {
        // Because this window's width is bound to its height, the width is negative when it is first created until
        // bindings settle out. For this reason, it is necessary to wait before using the window.
        Utils.connectUntil(contentItem.widthChanged, (width) => {
                               if (width <= 0)
                                   return false
                               ready()
                               return true
                           })
    }

    required property Assistant assistant

    property AssistantDialogArea dialogArea: assistantDialogArea
    property AssistantSeat assistantSeat: seat
    property Item rootItem: assistantWindow.contentItem

    function fadeDialogIn(then) {
        if (typeof then !== "function")
            then = () => {}
        Utils.connectOnce(dialogFadeIn.finished, then)
        dialogFadeIn.start()
    }

    // Emitted to notify that the window is ready to be used; don't use it before!
    signal ready()

    PropertyAnimation {
        id: dialogFadeIn
        target: assistantDialogArea
        property: "opacity"
        to: 1
        duration: 300
        easing.type: Easing.InCubic
    }

    AssistantSeat {
        id: seat
        anchors {
            left: rootItem.left
            right: rootItem.right
            top: rootItem.top
        }
        height: width
    }
    MouseArea {
        id: assistantWindowMouseArea
        // We interpret a click anywhere in the assistant window as a click on the next button.
        anchors.fill: parent
        onClicked: assistantDialogArea.nextClicked()
    }

    AssistantDialogArea {
        id: assistantDialogArea
        anchors {
            left: rootItem.left
            right: rootItem.right
            margins: 20

            // Overlap with the bottom 1/5 of the seat
            top: seat.top
            topMargin: seat.height * 4 / 5
        }
        // The implicit height will dynamically adjust based on the textual content
        z: seat.z + 1
        opacity: 0
        focus: true
    }
}
