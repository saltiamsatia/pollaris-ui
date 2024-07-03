import QtQuick 2.0
import QtQuick.Controls 2.15

/*
 * The component customizes the standard Qt Text by
 * dynamically adjusting its implicit height in response to the current text.
 * The adjusted height thereby dynamically activates the scroll bar.
 *
 * The height of the text content is determined by how the text is laid out
 * and wrapped and on any possibly "rich text" formatting which may further
 * alter the text's height.
 *
 * The "standard" height of one line is calculated based on the font
 * that is set as the default font for the component.  The text content
 * will use the font assigned to the component unless "rich text" formatting
 * codes (e.g. "<h1>Header</h1>") are embedded into the text.  Therefore,
 * the natural "content height" will be determined upon layout of the new text.
 *
 * While the content height is less than the height of 3.5 "standard" lines,
 * the component's height will be set to 3.5 "standard" lines.
 *
 * While the content height is between the height of 3.5 and 5 "standard" lines,
 * the component's height will be set to the content height.
 *
 * While the content height is greater than the height of 5 "standard" lines,
 * the component's height will be set to 5 "standard" lines.
 */
Item {
    id: internalContainer

    // Expose Text properties of the internal Text QML instance
    // that may be externally configured
    property alias text: t.text
    property alias font: t.font

    // The scroll view (and its scrollbar) activates
    ScrollView {
        id: sv
        anchors {
            left: parent.left // Necessary for horizontal wrapping of text
            right: parent.right // Necessary for horizontal wrapping of text
            top: parent.top // Necessary for vertical clipping of text
            bottom: parent.bottom // Necessary for vertical clipping of text
        }
        clip: true // Necessary for vertical clipping of text

        // (Optionally) Always hide the horizontal ScrollBar
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        // Display the vertical scrollbar if the text exceeds the 5-line height
        ScrollBar.vertical.policy: {
            if (t.contentHeight > fiveLineHeight) {
                return ScrollBar.AlwaysOn
            } else {
                return ScrollBar.AlwaysOff
            }
        }

        // The main Text component
        Text {
            property int cumulativeHeight: 0
            id: t
            horizontalAlignment: Text.AlignJustify // Increase likelihood of occupying the width
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere // Necessary for horizontal wrapping
            width: sv.width // Necessary for horizontal wrapping

            /*
             * Monitor the natural height of the text content as it is being laid out
             */
            onLineLaidOut: function(line) {
                // Reset when laying out the first line
                if (line.number === 0) {
                    cumulativeHeight = 0
                }

                // Tally the cumulative height
                cumulativeHeight += line.height

                // Adjust the height of the component when laying out the last line of text
                if (line.isLast) {
                    adjustHeight(cumulativeHeight)
                }
            }
        }
    }

    /*
     * Internal helpers for dynamically calculating the height of the component
     */
    Text {
        id: referenceOneLine
        visible: false
        text: "Line 1"
        font: t.font // Use the main Text's font
    }
    property alias oneLineHeight: referenceOneLine.height
    property int threeAndHalfLineHeight: oneLineHeight * 3.5 // Cache for computational performance

    Text {
        id: referenceFiveLines
        visible: false
        text: "Line 1\nLine 2\nLine 3\nLine 4\nLine 5"
        font: t.font // Use the main Text's font
    }
    property alias fiveLineHeight: referenceFiveLines.height // Cache for computational performance

    /*
     * Dynamically adjust the height of the text component
     * in response to the height of the text content
     */
    function adjustHeight(newContentHeight) {
        if (newContentHeight >= fiveLineHeight) {
            // Expand enough to display a maximum of 4.7 "standard" height lines
            internalContainer.implicitHeight = fiveLineHeight * .94

        } else if (newContentHeight > threeAndHalfLineHeight) {
            // Expand enough to display the lines (i.e. 4 or 5 "standard" height lines)
            internalContainer.implicitHeight = newContentHeight

        } else {
            // Expand enough to display a a maximum of 3.5 "standard" height lines
            internalContainer.implicitHeight = threeAndHalfLineHeight
        }
    }
}
