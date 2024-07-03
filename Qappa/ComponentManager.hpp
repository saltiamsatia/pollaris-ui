#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>
#include <QQmlComponent>

/*!
 * \brief The ComponentManager class helps with the creation of components and objects thereof
 */
class ComponentManager : public QObject {
    Q_OBJECT

    class Incubator;

    QQmlEngine* engine;

    std::vector<std::unique_ptr<Incubator>> incubators;

    QHash<QUrl, QQmlComponent*> fileCache;
    QHash<QByteArray, QQmlComponent*> sourceCache;

    QJSValue jsCast(QObject* o) {
        if (engine == nullptr) {
            qCritical("Unable to get QML engine in ComponentManager!");
            return QJSValue(QJSValue::NullValue);
        }
        return engine->toScriptValue(o);
    }

public:
    explicit ComponentManager(QQmlEngine* engine, QObject* parent = nullptr);
    virtual ~ComponentManager();

    using CreatedCallback = std::function<void(QObject*)>;

public slots:
    void createObject(QUrl fileLocation, CreatedCallback callback,
                      QObject* parent = nullptr, QVariantMap properties = {});
    void createObject(QQmlComponent* component, CreatedCallback callback,
                      QObject* parent = nullptr, QVariantMap properties = {});
    void createObject(QByteArray qmlSource, QUrl virtualLocation, CreatedCallback callback,
                      QObject* parent = nullptr, QVariantMap properties = {});

    // Mirrors of the other createObject calls, but these take a javascript callback function rather than C++
    void createFromFile(QUrl fileLocation, QJSValue callback, QObject* parent, QVariantMap properties) {
        auto cb = [this, cb=std::move(callback)](QObject* o) mutable { cb.call({jsCast(o)}); };
        return createObject(fileLocation, std::move(cb), parent, properties);
    }
    void createFromComponent(QQmlComponent* component, QJSValue callback, QObject* parent, QVariantMap properties) {
        auto cb = [this, cb=std::move(callback)](QObject* o) mutable { cb.call({jsCast(o)}); };
        return createObject(component, std::move(cb), parent, properties);
    }
    void createFromSource(QByteArray qmlSource, QUrl virtualLocation, QJSValue callback,
                      QObject* parent, QVariantMap properties) {
        auto cb = [this, cb=std::move(callback)](QObject* o) mutable { cb.call({jsCast(o)}); };
        return createObject(qmlSource, virtualLocation, std::move(cb), parent, properties);
    }

private:
    template<typename CB>
    void beginCreation(QQmlComponent* component, QObject* parent, QVariantMap properties, CB callback);
    template<typename CB>
    void finishCreation(QQmlComponent* component, QObject* parent, QVariantMap properties, CB callback);
};
