import QtQuick 2.14
import QtQuick.Window 2.14

import Pollaris.Utilities 1.0

Window {
    id: startupWindow
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowTransparentForInput | Qt.BypassWindowManagerHint
    x: 0; y: 0
    width: Screen.width
    height: Screen.height

    property Assistant assistant

    //! Run opening animation
    function startup(assistantWindow) {
        if (!assistant) {
            console.error("Assistant is not set! Cannot run startup animation")
            return
        }

        // Set up logo to be small, fast, under the mouse, and invisible
        logo.opacity = 0
        let mousePosition = assistant.mousePosition()
        logo.x = mousePosition.x - 25
        logo.y = mousePosition.y - 25
        logo.width = logo.height = 50
        // Open the window, but everything is invisible right now
        startupWindow.show()

        // Set up the startup animation with the target position and size
        let screenWidth = Screen.width
        let screenHeight = Screen.height
        let edge = Math.min(screenHeight, screenWidth) / 2
        logo.maxSize = edge
        startupAnimation.takeoverWindow = assistantWindow
        startupAnimation.destinationWidth = startupAnimation.destinationHeight = edge
        startupAnimation.destinationX = screenWidth/2 - edge/2
        startupAnimation.destinationY = screenHeight/2 - edge/2
        startupAnimation.targetRpms = assistantWindow.assistantLogo.rotationRpms

        // Start the animmation
        startupAnimation.start()
    }

    signal finished()

    AssistantLogo {
        id: logo
        rotationRpms: 120
    }

    SequentialAnimation {
        id: startupAnimation
        property QtObject takeoverWindow
        property real destinationX
        property real destinationY
        property real destinationWidth
        property real destinationHeight
        property real targetRpms

        PropertyAnimation {
            target: logo
            property: "opacity"
            from: 0; to: 1
            duration: 100
        }
        ParallelAnimation {
            PropertyAnimation {
                target: logo
                property: "x"
                to: startupAnimation.destinationX
                duration: 1000
                easing.type: Easing.OutQuad
            }
            PropertyAnimation {
                target: logo
                property: "y"
                to: startupAnimation.destinationY
                duration: 1000
                easing.type: Easing.InQuad
            }
            PropertyAnimation {
                target: logo
                property: "width"
                to: startupAnimation.destinationWidth
                duration: 1000
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: logo
                property: "height"
                to: startupAnimation.destinationHeight
                duration: 1000
                easing.type: Easing.OutCubic
            }
        }
        ScriptAction {
            script: {
                // Position the takeover window over our logo
                var window = startupAnimation.takeoverWindow
                window.width = logo.width
                window.height = logo.height
                window.x = logo.x
                window.y = startupAnimation.destinationY
                // Synchronize the takeover window's logo with ours
                window.assistantLogo.rotationRpms = logo.rotationRpms
                window.assistantLogo.ringRotation = logo.ringRotation
                // Make our logo imitate takeover window's logo
                logo.rotationRpms = 0
                logo.ringRotation = Qt.binding(function() { return window.assistantLogo.ringRotation; })

                window.takeover()
            }
        }
        PropertyAnimation {
            target: logo
            property: "opacity"
            to: 0
            duration: 100
        }
        PauseAnimation { duration: 100 }
        ScriptAction {
            script: {
                startupWindow.close()
                startupWindow.finished()
                startupWindow.destroy()
            }
        }
    }
}
