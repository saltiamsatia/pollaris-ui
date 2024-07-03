#include <Task.hpp>
#include <Assistant.hpp>

#include <QDebug>

Task::Task(Assistant* assistant) : QObject(assistant), assistant(assistant) {}

QDebug operator<<(QDebug dbg, const Task& task) {
    return (dbg.noquote() << Assistant::getLoggingTag(&task)).quote();
}
