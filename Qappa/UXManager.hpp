#pragma once

#include "ComponentManager.hpp"

#include <QObject>

class UXManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(ComponentManager* componentManager READ componentManager WRITE setComponentManager
               NOTIFY componentManagerChanged)
    ComponentManager* m_componentManager;

public:
    explicit UXManager(ComponentManager* componentManager, QObject *parent = nullptr);
    virtual ~UXManager();

    ComponentManager* componentManager() const { return m_componentManager; }

public slots:
    /*!
     * \brief Start the application by creating the AppManager
     *
     * \param appManagerFilename Name of the AppManager's QML file
     * \param appManagerProperties Any extra properties the AppManager should be initialized with
     *
     * This function is used to start the application GUI by creating the AppManager, which will oversee the GUI
     * operations from there. By default, this function will search for an AppManager.qml in qrc:/ and qrc:/qml, and
     * initialize it with componentManager and uxManager properties.
     *
     * If desired, the client can override the default filename or properties by passing explicit arguments here.
     */
    void begin(QUrl appManagerFilename = QStringLiteral("AppManager.qml"), QVariantMap appManagerProperties = {});

    void setComponentManager(ComponentManager* componentManager) {
        if (m_componentManager != componentManager)
            emit componentManagerChanged(m_componentManager = componentManager);
    }

signals:
    void componentManagerChanged(ComponentManager* componentManager);
};
