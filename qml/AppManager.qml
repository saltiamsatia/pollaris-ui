import QtQuick 2.15
import QtQuick.Window 2.15

import Qt.labs.settings 1.1

import Qappa 1.0
import Pollaris.Utilities 1.0
import "JsUtils.js" as Utils
import "AppManager.Startup.js" as Startup

Assistant {
    id: assistant
    objectName: "appManager"

    required property ComponentManager componentManager
    required property UXManager uxManager

    property AssistantLogo assistantLogo
    property AssistantDialogArea dialogArea
    property BlockchainInterface blockchain
    property Window dialogWindow

    Component.onCompleted: {
        Utils.initialize(componentManager)
        context.assistant = assistant
        // Pass in Screen, as it seems a code-behind javascript lib has no access to attached properties or something
        Startup.startup(Screen)

        // When quitting, explicitly destroy the visor and thus all visual items. This suppresses a spurious warning
        // about QTimer only being used in threads started with QThread.
        Qt.application.aboutToQuit.connect(() => visor.destroy())
    }

    function launchApp(preloadedUi) {
        dialogWindow.hide()
        dialogWindow.destroy()
        preloadedUi.show()

        // Go ahead and preload the transaction manager UI for later
        componentManager.createFromFile("qrc:/qml/TransactionManagerUI.qml",
                                        (trxMgr) => context.transactionManager = trxMgr, assistant, {})
    }

    property Window visor: Window {
        flags: Qt.WindowFullScreen |Qt.FramelessWindowHint |Qt.WindowTransparentForInput |Qt.BypassWindowManagerHint
        x: 0; y: 0
        width: Screen.width; height: Screen.height
        color: "transparent"
        visible: true
    }

    property Settings settings: Settings {
        id: assistantSettings
        // category: "Assistant"
        category: "Distractor"

        property url blockchainNodeUrl
        property string inviteCode
        property string voterPublicKey
        property string deploymentKey
        property string deploymentLocation
        property string voterName
    }

    property KeyManager keyManager: KeyManager {

    }

    property TlsPskSession tlsSession: TlsPskSession {
        keyManager: assistant.keyManager
    }
}
