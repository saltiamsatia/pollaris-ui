#include "UXManager.hpp"

#include <QFile>

UXManager::UXManager(ComponentManager* componentManager, QObject *parent)
    : QObject(parent), m_componentManager(componentManager) {}
UXManager::~UXManager() {}

void UXManager::begin(QUrl appManagerFilename, QVariantMap appManagerProperties) {
    if (m_componentManager == nullptr) {
        qCritical() << "Unable to start application: componentManager is null!";
        return;
    }

    QFile qmlFile(appManagerFilename.toString());
    QUrl fileUrl;
    if (appManagerFilename.isRelative()) {
        // Caveat: see comment below
        QStringList prefixes{QStringLiteral("qrc:/"), QStringLiteral("qrc:/qml/")};
        while (!qmlFile.exists() && !prefixes.empty()) {
            fileUrl = prefixes.takeFirst() + appManagerFilename.toString();
            // NOTE: The use of .mid(3) here chops off the "qrc", as is necessary for QFile. This only works if all
            // prefixes start with "qrc:", so if that ever changes, make sure to change this too :)
            qmlFile.setFileName(fileUrl.toString().mid(3));
        }
        if (!qmlFile.exists()) {
            qCritical() << "Unable to start application: could not find app manager" << appManagerFilename;
            return;
        }
    }

    auto componentMgrName = QStringLiteral("componentManager");
    auto uxMgrName = QStringLiteral("uxManager");
    if (!appManagerProperties.contains(componentMgrName))
        appManagerProperties.insert(componentMgrName, QVariant::fromValue(m_componentManager));
    if (!appManagerProperties.contains(uxMgrName))
        appManagerProperties.insert(uxMgrName, QVariant::fromValue(this));

    m_componentManager->createObject(fileUrl, [](QObject*){}, this, appManagerProperties);
}
