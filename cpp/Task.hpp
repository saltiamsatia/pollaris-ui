#pragma once

#include <QObject>

class Assistant;

class Task : public QObject {
    Q_OBJECT

    Assistant* assistant;
    Q_PROPERTY(Assistant* assistant READ getAssistant CONSTANT)

public:
    explicit Task(Assistant* assistant);

    Assistant* getAssistant() const { return assistant; }

signals:

};

QDebug operator<<(QDebug dbg, const Task& task);
