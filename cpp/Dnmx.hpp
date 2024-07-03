#pragma once

#include <QQmlPropertyMap>

// dnmx, or "dynamics" is a property which can be added to QObject-derived classes which are handled by javascript,
// allowing the javascript to attach various dynamic properties to the object to simplify their accounting.
// The following macro can be used within a class definition to add a dnmx property to the current object.
#define ADD_DNMX \
    private: \
        QQmlPropertyMap* m_dnmx = new QQmlPropertyMap(this); \
        Q_PROPERTY(QQmlPropertyMap* dnmx READ dnmx CONSTANT) \
    public: \
        QQmlPropertyMap* dnmx() { return m_dnmx; } \
    private:
