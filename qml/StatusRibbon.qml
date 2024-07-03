import QtQuick 2.14

import Pollaris.Utilities 1.0

Ribbon {
    property int status
    
    text: {
        var draft = qsTr("Draft")
        var pending = qsTr("Pending")
        if (status === Pollaris.DraftAdd || status === Pollaris.DraftEdit || status === Pollaris.DraftDelete)
            return draft
        if (status === Pollaris.PendingAdd || status === Pollaris.PendingEdit || status === Pollaris.PendingDelete)
            return pending
        return ""
    }
    color: {
        if (status === Pollaris.DraftAdd || status === Pollaris.DraftEdit || status === Pollaris.DraftDelete)
            return "grey"
        if (status === Pollaris.PendingAdd || status === Pollaris.PendingEdit || status === Pollaris.PendingDelete)
            return "yellow"
        return "black"
    }
    ribbonOpacity: .4
    visible: text !== ""
}
