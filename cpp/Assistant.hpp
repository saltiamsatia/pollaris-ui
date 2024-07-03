#pragma once

#include <Task.hpp>
#include <FpsTimer.hpp>

#include <QCursor>
#include <QObject>
#include <QQuickImageProvider>

class Assistant : public QObject {
    Q_OBJECT

    Q_PROPERTY(QList<Task*> tasks READ tasks NOTIFY tasksChanged)
    QList<Task*> taskList;

public:
    explicit Assistant(QObject *parent = nullptr);
    virtual ~Assistant();

    static QQuickImageProvider* getLogoProvider();
    static QByteArray getLoggingTag(const Task* t);
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message);

    Q_INVOKABLE QPoint mousePosition() { return QCursor::pos(); }

    Q_INVOKABLE Task* createTask();

    QList<Task*> tasks() const { return taskList; }

signals:
    void tasksChanged(QList<Task*> tasks);

    //! Emitted once per frame interval
    void frameInterval();
    //! Emitted if frames are dropped, with the number of dropped frames
    void framesDropped(int dropped);
};
