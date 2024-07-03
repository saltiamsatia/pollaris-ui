import QtQuick 2.15
import QtQuick.Window 2.15

import Pollaris.Utilities 1.0
import "JsUtils.js" as Utils

Item {
    id: logo
    width: 200
    height: 200
    z: 100
    state: "unseated"

    required property Window visor
    required property Assistant assistant
    property AssistantSeat newSeat: null

    property AssistantSeat seat
    property bool isSeated: parent === seat
    property int rotationRpms: 45
    property int maxSize: 0

    function reseat(newSeat, then) {
        if (!newSeat) {
            console.error("Cannot reseat assistant to " + newSeat)
            return
        }
        if (!newSeat.width || !newSeat.height) {
            console.error(`Cannot reseat assistant to seat with dimensions ${newSeat.width}x${newSeat.height}`)
            return
        }

        logo.newSeat = newSeat
        finishReseat.callbacks.push(then)
        console.info("Reseating assistant to", newSeat);

        visor.showFullScreen()
        visor.raise()

        let currentPosition = logo.mapToGlobal(0, 0)
        if (logo.parent !== visor.contentItem)
            Object.assign(logo, {"parent": visor.contentItem, "x": currentPosition.x, "y": currentPosition.y})
        let targetPosition = newSeat.mapToGlobal(0, 0)
        Object.assign(xAnimation, {"from": currentPosition.x, "to": targetPosition.x})
        Object.assign(yAnimation, {"from": currentPosition.y, "to": targetPosition.y})
        Object.assign(sizeAnimation,{"from": logo.width, "to": Math.min(newSeat.width, newSeat.height)})
        reseatAnimation.start()
    }
    function adjustRpms(newValue, then) {
        rotationRpms = newValue
        if (typeof then === "function")
            rpmsAdjusted.callbacks.push(then)
    }

    Behavior on opacity { NumberAnimation { duration: 100 } }
    Behavior on rotationRpms {
        SequentialAnimation {
            NumberAnimation {
                duration: 1000
                easing.type: Easing.OutCubic
            }
            ScriptAction {
                id: rpmsAdjusted
                property var callbacks: []
                script: { while (callbacks.length) callbacks.pop()() }
            }
        }
    }

    SequentialAnimation {
        id: reseatAnimation

        ParallelAnimation {
            PropertyAnimation {
                id: xAnimation
                target: logo
                property: "x"
                duration: 1000
                easing.type: Easing.OutQuad
            }
            PropertyAnimation {
                id: yAnimation
                target: logo
                property: "y"
                duration: 1000
                easing.type: Easing.InQuad
            }
            PropertyAnimation {
                id: sizeAnimation
                target: logo
                properties: "width,height"
                duration: 1000
                easing.type: Easing.OutCubic
            }
        }
        PauseAnimation {
            duration: 100
        }
        ScriptAction {
            id: finishReseat

            property var callbacks: []

            script: {
                Object.assign(logo, {"parent": newSeat, "x": 0, "y": 0, "newSeat": null})
                // On my system, closing windows are faded out based on a screenshot from when they were closed.
                // By delaying a moment, we ensure that screenshot is transparent rather than the last frame of logo.
                Utils.setTimeout(30, visor.hide)

                while (callbacks.length) callbacks.pop()()
            }
        }
    }

    Connections {
        target: assistant
        enabled: visible && rotationRpms !== 0

        function onFrameInterval() { ringImage.updateRotation() }
        function onFramesDropped(frames) { ringImage.updateRotation(frames) }
    }

    Image {
        id: ringImage
        source: "image://assistant/ring"
        anchors.fill: parent
        sourceSize {
            width: maxSize || width
            height: maxSize || height
        }

        // Number of degrees to rotate ring at each frame
        // Calculated as degrees to rotate per millisecond times milliseconds per frame
        property real ringAdjustment: (360*rotationRpms/60000) * (1000/60)

        function updateRotation(steps = 1) {
            rotation = (rotation + steps*ringAdjustment) % 360
        }
    }
    Image {
        id: veeImage
        source: "image://assistant/vee"
        anchors.fill: parent
        sourceSize {
            width: maxSize || width
            height: maxSize || height
        }
    }
}
