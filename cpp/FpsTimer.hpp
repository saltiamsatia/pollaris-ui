#pragma once

#include <QTimer>
#include <QDebug>

class FpsTimer : public QObject {
    Q_OBJECT

    QTimer timer;
    std::chrono::time_point<std::chrono::steady_clock> lastFired;
    //! The number of milliseconds after which a frame will be regarded as dropped
    const std::chrono::milliseconds DROP_THRESHOLD = std::chrono::milliseconds(30);

public:
    explicit FpsTimer(QObject* parent = nullptr) : QObject(parent) {
        timer.setTimerType(Qt::PreciseTimer);
        connect(&timer, &QTimer::timeout, this, &FpsTimer::timerFired, Qt::DirectConnection);
        lastFired = std::chrono::steady_clock::now();
        timer.setSingleShot(true);
        timer.start(std::chrono::milliseconds(1000/60));
    }
    virtual ~FpsTimer() {}

signals:
    //! Emitted once per frame
    void triggered();

    //! Emitted whenever a frame was dropped, with the number of dropped frames
    void framesDropped(int dropped);

private slots:
    void timerFired() {
        // Reset the timer immediately
        timer.start(std::chrono::milliseconds(1000/60));
        // Record the time since last fired
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFired);
        // Update the last fired time
        lastFired = now;

        if (elapsed > DROP_THRESHOLD) {
            int dropped = elapsed / std::chrono::milliseconds(1000/60);
            emit framesDropped(dropped);
        }

        emit triggered();
    }
};
