import QtQuick 2.14
import QtQuick.Window 2.14

import "JsUtils.js" as Utils

Item {
    // The implicit height of the control will dynamically adjust as a function of the textual content
    implicitHeight: dialogText.height + dialogFooterArea.height

    property alias animateTyping: typingAnimation.enabled
    property alias text: typingAnimation.text
    property alias font: dialogText.font
    property Item inputArea: inputAreaItem
    property alias previousVisible: previousButton.visible
    property alias nextVisible: nextButton.visible
    property alias previousEnabled: previousMouseArea.enabled
    property alias nextEnabled: nextMouseArea.enabled

    property bool isTyping: opacityAnimation.running || typingAnimation.running

    signal previousClicked()
    signal nextClicked()
    signal returnPressed()
    signal spacePressed()
    signal escapePressed()

    Keys.onReturnPressed: returnPressed()
    Keys.onSpacePressed: spacePressed()
    Keys.onEscapePressed: escapePressed()

    function write(newText, then) {
        if (isTyping) {
            completeTyping()
            Utils.nextFrame(() => write(newText, then))
            return
        }

        if (typeof then === "function")
            Utils.connectOnce(typingAnimation.stopped, then)

        if (text !== newText)
            text = newText
        else
            // If the text isn't actually changing since last time, just pump the signal to restart the animations
            typingAnimation.textChanged()
    }
    function completeTyping() {
        if (opacityAnimation.running) {
            opacityAnimation.complete()
            typingAnimation.text = ""
        } else {
            typingAnimation.complete()
        }
    }

    // White, semi-transparent background for the dialog text and footer
    Rectangle {
        id: assistantDialogRectangle
        radius: 5
        opacity: .7
        /* Fill the parent
         * As the parent's height is dynamically calculated with the height of the dialog text,
         * the background's height will also dynamically adjust.
         */
        anchors.fill: parent
    }


    // Foreground
    Item {
        id: internalForeground
         anchors {
             // Fill the parent's width
             left: parent.left
             right: parent.right
             margins: 10
             // The implicit height is derived from the the implicit height of the dialog text
             // plus the explicit height of the footer area
         }

        AsiText {
            id: dialogText
            anchors {
                // Anchor the top, left, and right
                top: parent.top
                left: parent.left
                right: parent.right
                // Do not anchor the bottom to permit the implicit height to vary
                margins: 5 // Add some buffer between the text and the edges
            }
            font.pointSize: 16
        }

        Item {
            id: dialogFooterArea
            anchors {
                left: internalForeground.left
                right: internalForeground.right
                top: dialogText.bottom
            }
            // Define a fixed height that is proportional to the width
            height: width / 6

            Image {
                id: previousButton
                source: "image://assistant/vee"
                rotation: 90
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                }
                height: parent.height
                width: height

                MouseArea {
                    id: previousMouseArea
                    anchors.fill: parent
                    onClicked: previousClicked()
                }
            }
            Item {
                id: inputAreaItem
                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: previousButton.right
                    right: nextButton.left
                }
            }
            Image {
                id: nextButton
                source: "image://assistant/vee"
                rotation: -90
                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                }
                height: parent.height
                width: height

                MouseArea {
                    id: nextMouseArea
                    anchors.fill: parent
                    onClicked: nextClicked()
                }
            }
        }
    }

    TypingAnimation {
        id: typingAnimation
        target: dialogText

        Behavior on text {
            id: textTypeBehavior
            SequentialAnimation {
                id: opacityAnimation
                PropertyAnimation {
                    target: dialogText
                    property: "opacity"
                    to: 0
                }
                ScriptAction {
                    script: {
                        dialogText.text = ""
                        dialogText.opacity = 1
                    }
                }
                PropertyAction { target: typingAnimation; property: "text" }
            }
        }
    }
}
