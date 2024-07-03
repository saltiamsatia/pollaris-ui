#include "Plugin.hpp"
#include "ComponentManager.hpp"
#include "UXManager.hpp"
#include <QObject>

void QappaPlugin::registerTypes(const char* uri)

{

    qmlRegisterUncreatableType<ComponentManager>(uri, 1, 0, "ComponentManager",
                                                 QStringLiteral("ComponentManager can only be created in C++"));
    qmlRegisterUncreatableType<UXManager>(uri, 1, 0, "UXManager",
                                          QStringLiteral("UXManager can only be created in C++"));
}
