import QtQuick 2.14
import QtQuick.Window 2.14
import QtQuick.Controls 2.14

import "JsUtils.js" as Utils

QtObject {
    id: typingAnimation
    
    // *** Configuration properties ***
    // The object whose property should be animated
    property QtObject target
    // The property to animate
    property string property: "text"
    // If false, does not animate, but simply completes immediately
    property bool enabled: true
    // The text to type
    property string text: ""
    // The maximum number of characters to type at a time
    property int maxCharactersTyped: 3
    // The maximum number of frames to pause between typing characters
    property int maxFramesBetweenTyping: 4
    // Set to cause typing to pause briefly after typing a given character sequence. Use to pause between sentences
    property string pauseAfterText: ". "
    // The scalar to multiply the normal delay by during a brief pause in typing
    property int pauseDelayFactor: 5
    // Whether to attempt to type HTML tags all in one burst or not
    property bool checkHtml: true

    // *** Status/output properties ***
    // Whether the animation is currently running or not (brief pauses still count as running)
    property bool running: typeof text === "string" && charactersTyped < text.length
    // The number of characters typed so far
    property int charactersTyped: 0
    // The number of frames until more characters are typed
    property int framesToWait: 0

    // Start the animation with the supplied text to be typed
    function start(newText) {
        if (!!newText)
            text = newText
        if (!enabled) {
            complete()
            return
        }

        target[property] = ""
        charactersTyped = 0
        if (!!text)
            framesToWait = Utils.gaussianRand(1, 5)
        else
            framesToWait = 0
    }
    // Finish the animation immediately, setting final property values
    function complete() {
        target[property] = text
        charactersTyped = text.length
        framesToWait = 0
    }

    // Emitted when the animation stops, whether by finishing naturally, or by the complete() method
    signal stopped

    property Connections frameHandler: Connections {
        target: context && context.assistant? context.assistant : null
        enabled: typingAnimation.running

        function onFrameInterval() {
            if (--typingAnimation.framesToWait > 0)
                return
            
            let remainingText = typingAnimation.text.slice(typingAnimation.charactersTyped)
            let characters = ""

            // If we've got an HTML tag coming up, pull it out while we select the characters to type; we'll add it
            // back in again in a moment
            let tags = []
            while (checkHtml && remainingText.slice(0, maxCharactersTyped).indexOf('<') > -1) {
                let tag = {
                    "index": remainingText.indexOf('<'),
                    "value": remainingText.slice(remainingText.indexOf('<'), remainingText.indexOf('>')+1)
                }
                tags.push(tag)
                remainingText = remainingText.slice(0, tag.index) + remainingText.slice(tag.index + tag.value.length)
            }

            // If we're near the end of a sentence, just take the rest of the sentence; otherwise, take up to
            // maxCharactersTyped characters
            if (remainingText.slice(0, maxCharactersTyped+pauseAfterText.length-1).indexOf(pauseAfterText) >= 0)
                characters = remainingText.slice(0, remainingText.indexOf(pauseAfterText)+pauseAfterText.length)
            else
                characters = remainingText.slice(0, Utils.gaussianRand(1, maxCharactersTyped))

            // Re-insert any tags we removed
            while (tags.length) {
                let tag = tags.pop()
                if (characters.length > tag.index)
                    characters = characters.slice(0, tag.index) + tag.value + characters.slice(tag.index)
            }
            
            typingAnimation.target[property] += characters
            typingAnimation.charactersTyped  += characters.length
            
            let delayFrames = Utils.gaussianRand(1, maxFramesBetweenTyping)
            // Add an extra delay at the end of sentences.
            if (characters.endsWith(pauseAfterText))
                delayFrames *= pauseDelayFactor
            typingAnimation.framesToWait += delayFrames
            if (typingAnimation.framesToWait < 1)
                onFrameInterval()
        }
        function onFramesDropped(frames) {
            typingAnimation.framesToWait -= frames
            onFrameInterval()
        }
    }

    onTextChanged: start()
    onRunningChanged: if (!running) stopped()
}
