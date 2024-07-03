.pragma library

.import Pollaris.Utilities 1.0 as Pollaris
.import Qappa 1.0 as Qappa

var componentManager

function initialize(componentMgr) {
    componentManager = componentMgr
}

// Check that all fields in obj1 match fields in obj2 exactly
// If mismatches is an object, it will be populated with the obj2 values that didn't match
function matchFields(obj1, obj2, mismatches) {
    var match = true
    Object.keys(obj1).forEach(function(field) {
        if (obj1[field] !== obj2[field]) {
            if (JSON.stringify(obj1[field]) === JSON.stringify(obj2[field]))
                // False alarm, javascript just sucks at comparisons
                return
            match = false
            if (typeof mismatches === "object" && mismatches !== null)
                mismatches[field] = obj2[field]
        }
    })
    return match
}

// This function adapted from https://stackoverflow.com/a/11390499
function deepCopy(p) {
    let c = c || {};
    for (let i in p) {
        if (typeof p[i] === 'object') {
            c[i] = (p[i].constructor === Array) ? [] : {};
            deepCopy(p[i], c[i]);
        } else {
            c[i] = p[i];
        }
    }
    return c;
}

// Set a timer to call callback after delay milliseconds. Returns a function which can be called to cancel the timer
function setTimeout(delay, callback) {
    if (typeof callback !== "function") {
        console.error("Invalid callback to setTimeout:", callback)
        console.trace()
        return
    }

    var cancelObject = {"canceled": false, "fired": false}

    let onCreated = (timer) => {
        if (cancelObject.canceled) {
            timer.destroy()
            return
        }
        cancelObject.timer = timer
        timer.triggered.connect(function() {
            cancelObject.fired = true
            timer.destroy()
            callback()
        })
        timer.start()
    }

    componentManager.createFromSource("import QtQuick 2.15; Timer {}", "Utils.setTimeout::callback", onCreated,
                                      componentManager, {"interval": delay})

    return () => {
        if (!!cancelObject.timer && !cancelObject.canceled && !cancelObject.fired)
            cancelObject.timer.destroy()
        else
            cancelObject.canceled = true
    }
}

// Invoke callback on next frame
function nextFrame(callback) {
    return setTimeout(16, callback)
}

// When the signal is emitted next time only, call then()
function connectOnce(sig, then) {
    if (typeof then !== "function") {
        console.error("Invalid callback to connectOnce:", then)
        console.trace()
        return
    }

    let handler = () => { sig.disconnect(handler); then.apply(null, arguments) }
    sig.connect(handler)
}

// Each time signal is emitted, call then(), until the first time then() returns true
function connectUntil(sig, then) {
    if (typeof then !== "function") {
        console.error("Invalid callback to connectUntil:", then)
        console.trace()
        return
    }

    let handler = () => {
        if (then.apply(null, arguments))
            sig.disconnect(handler)
    }
    sig.connect(handler)
}

// This function adapted from https://stackoverflow.com/a/39187274
function gaussianRand() {
    let rand = 0;

    for (let i = 0; i < 7; i += 1)
        rand += Math.random();

    if (arguments.length === 2) {
        let lower = arguments[0]
        let upper = arguments[1]
        return Math.floor(lower + gaussianRand() * (upper - lower + 1));
    }
    return rand / 7;
}

// Adapted from https://stackoverflow.com/a/51636258
// String literal template tag which cleans consecutive whitespace from multiline strings
function multiln(strings, ...interpolations) {
    let combined = strings.reduce(function(result, string, index) {
        return result + string + (interpolations[index] || "")
    })
    let cleaned = combined.replace(/\s\s+/g, ' ')
    return cleaned
}
