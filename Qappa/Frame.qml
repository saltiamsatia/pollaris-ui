import QtQuick 2.0

import Qappa 1.0

Item {
    id: currentFrame
    property Panel panel

    property bool filled: !!panel

    //! Install the provided panel into the frame, displacing any panel that may have previously been installed.
    //! If a panel was displaced, it is returned.
    function install(panel) {
        let displaced
        if (filled) {
            currentFrame.panel.frame = undefined
            displaced = currentFrame.panel
        }

        currentFrame.panel = panel
        panel.frame = currentFrame

        return displaced
    }

}
