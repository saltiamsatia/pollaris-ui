import QtQuick 2.15

import Qappa 1.0

Item {
    id: currentPanel

    property Frame frame

    property bool mounted: !!frame

    //! Mount this panel in the provided frame, displacing any panel that might have previously been mounted there.
    //! If a panel was displaced, it is returned.
    function mount(frame) {
        return frame.install(currentPanel)
    }
}
