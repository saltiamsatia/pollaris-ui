#include "ComponentManager.hpp"

#include <QDebug>
#include <QQmlIncubator>

ComponentManager::ComponentManager(QQmlEngine* engine, QObject* parent) : QObject(parent), engine(engine) {
    if (engine != nullptr)
        connect(engine, &QQmlEngine::destroyed, this, [this] { this->engine = nullptr; });
}
ComponentManager::~ComponentManager() {}

bool checkEngine(QQmlEngine* engine, bool asynchronous = true) {
    if (engine == nullptr) {
        qCritical() << "Unable to get QML engine in ComponentManager!";
        return false;
    }
    if (asynchronous && engine->incubationController() == nullptr)
        qWarning() << "QML engine has no incubation controller! Component creation will be synchronous.";
    return true;
}

void ComponentManager::createObject(QUrl fileLocation, CreatedCallback callback,
                                    QObject* parent, QVariantMap properties) {
    // If we're creating the AppManager, no window has been created and thus creation is synchronous. This is not a
    // bug, so suppress the warning.
    if (!checkEngine(engine, !fileLocation.toString().contains("AppManager"))) {
        callback(nullptr);
        return;
    }
    if (!callback) {
        qCritical() << "Asked to create object, but given null callback!";
        return;
    }

    QQmlComponent* component = nullptr;
    if (fileCache.contains(fileLocation)) {
        component = fileCache[fileLocation];
    } else {
        component = new QQmlComponent(engine, fileLocation, QQmlComponent::Asynchronous, this);
        fileCache.insert(fileLocation, component);
    }

    beginCreation(component, parent, properties, std::move(callback));
}

void ComponentManager::createObject(QQmlComponent* component, CreatedCallback callback,
                                    QObject* parent, QVariantMap properties) {
    if (component == nullptr) {
        qWarning() << "Asked to create object from null component!";
        callback(nullptr);
        return;
    }
    if (!callback) {
        qCritical() << "Asked to create object, but given null callback!";
        return;
    }

    if (!checkEngine(engine)) {
        callback(nullptr);
        return;
    }

    beginCreation(component, parent, properties, std::move(callback));
}

void ComponentManager::createObject(QByteArray qmlSource, QUrl virtualLocation, CreatedCallback callback,
                                    QObject* parent, QVariantMap properties) {
    if (!checkEngine(engine)) {
        callback(nullptr);
        return;
    }
    if (!callback) {
        qCritical() << "Asked to create object, but given null callback!";
        return;
    }

    QQmlComponent* component = nullptr;
    if (sourceCache.contains(qmlSource)) {
        component = sourceCache[qmlSource];
    } else {
        component = new QQmlComponent(engine, this);
        component->setData(qmlSource, virtualLocation);
        sourceCache.insert(qmlSource, component);
    }

    beginCreation(component, parent, properties, std::move(callback));
}

class ComponentManager::Incubator : public QQmlIncubator {
    QObject* parent;
    CreatedCallback callback;

public:
    Incubator(QObject* parent, QVariantMap properties, CreatedCallback callback)
        : parent(parent), callback(std::move(callback)) {
        setInitialProperties(properties);
    }

    // QQmlIncubator interface
protected:
    void statusChanged(Status status) override {
        if (status == Status::Error) {
            qWarning() << "Error when incubating object:" << errors();
            callback(nullptr);
        }
        if (status == Status::Ready) {
            auto o = object();
            o->setParent(parent);
            QQmlEngine::setObjectOwnership(o, QQmlEngine::JavaScriptOwnership);
            callback(o);
        }
    }
};

template<typename CB>
void ComponentManager::beginCreation(QQmlComponent* component, QObject* parent, QVariantMap properties, CB callback) {
    if (component == nullptr) {
        qCritical() << "Tried to begin creation of a null component!";
        callback(nullptr);
        return;
    }

    if (!component->isLoading()) {
        finishCreation(component, parent, properties, std::move(callback));
        return;
    }
    connect(component, &QQmlComponent::statusChanged, this,
            [this, component, parent, properties, cb=std::move(callback)] {
        finishCreation(component, parent, properties, std::move(cb));
    });
}

template<typename CB>
void ComponentManager::finishCreation(QQmlComponent* component, QObject* parent, QVariantMap properties, CB callback) {
    if (component == nullptr) {
        qCritical() << "Tried to begin incubation of a null component!";
        callback(nullptr);
        return;
    }

    if (component->isLoading()) {
        qInfo() << "Odd... ComponentManager::finishCreation called for component which is still loading";
        return;
    }
    if (component->isNull()) {
        qInfo() << "Odd... ComponentManager::finishCreation called for component which is null";
        return;
    }

    if (component->isError()) {
        qWarning() << "Failed to create component for" << component->url()
                   << "\n    Error:" << component->errorString();
        callback(nullptr);
        return;
    }

    // This fancy dance is to avoid leaking Incubators. We save the Incubator in a unique_ptr owned by the parent,
    // and we wrap the callback so we can delete that unique_ptr when the callback finishes. Then we store the
    // unique_ptr before activating the Incubator so if the incubator somehow finished synchronoously, it still works.
    auto cb = [this, cb=std::move(callback), index=incubators.size()] (QObject* o) {
        cb(o);
        incubators.erase(incubators.begin() + index);
    };
    auto incubator = std::make_unique<Incubator>(parent, properties, std::move(cb));
    auto& savedIncubator = *incubator;
    incubators.push_back(std::move(incubator));
    component->create(savedIncubator);
}
