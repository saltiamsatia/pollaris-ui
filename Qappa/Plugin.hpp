#pragma once

#include <QObject>

#include <QQmlExtensionPlugin>


// We use the old standard here (QQmlExtensionPlugin rather than QQmlEngineExtensionPlugin) because it has the
// registerTypes method, which we need. The new standard uses the QML_ELEMENT macros which don't work without qmake
class QappaPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "QappaPlugin")

public:
    void registerTypes(const char* uri) override;
};
