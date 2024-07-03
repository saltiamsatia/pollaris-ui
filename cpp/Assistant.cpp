#include <Infrastructure/typelist.hpp>

#include <Assistant.hpp>
#include <Task.hpp>

#include <QCache>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QReadWriteLock>

// File-local, mutex-protected pointer to a shared FpsTimer for all assistants
static FpsTimer* timer = nullptr;
static QReadWriteLock timerLock;

// File-local, mutex-protected list of known assistant objects, allows finding all assistants in the address space
static QSet<Assistant*> knownAssistants;
static QReadWriteLock assistantsLock;

// File-local, mutex-protected set of known task objects, allows checking validity of task pointers written to log
static QMap<uint32_t, const Task*> knownTasks;
static QReadWriteLock tasksLock;

struct Read;
struct Write;
template<typename LockType, typename Callable>
void withLock(QReadWriteLock& lock, Callable&& c) {
    using namespace infra::typelist;
    static_assert(contains<list<Read, Write>, LockType>(), "Lock type must be either read or write");
    if constexpr (std::is_same_v<LockType, Read>)
        lock.lockForRead();
    else
        lock.lockForWrite();
    c();
    lock.unlock();
}

FpsTimer* fpsTimer() {
    FpsTimer* timer = nullptr;
    withLock<Read>(timerLock, [&timer] { timer = ::timer; });
    if (timer != nullptr)
        return timer;

    withLock<Write>(timerLock, [&timer] { timer = ::timer = new FpsTimer(); });
    return timer;
}

Assistant::Assistant(QObject *parent) : QObject(parent) {
    withLock<Write>(assistantsLock, [this] { knownAssistants.insert(this); });

    auto timer = fpsTimer();
    connect(timer, &FpsTimer::triggered, this, &Assistant::frameInterval);
    connect(timer, &FpsTimer::framesDropped, this, [this](int dropped) {
        qInfo() << "Dropped frames:" << dropped;
        emit framesDropped(dropped);
    });
}
Assistant::~Assistant() {
    // Remove ourselves from known assistants list, and check if we're the last one
    bool lastOut = false;
    withLock<Write>(assistantsLock, [this, &lastOut] {
        knownAssistants.remove(this);
        if (knownAssistants.isEmpty())
            lastOut = true;
    });

    // If no more assistants exist, destroy the shared FpsTimer too
    if (lastOut)
        withLock<Write>(timerLock, [] { delete timer; timer = nullptr; });
}

QByteArray Assistant::getLoggingTag(const Task* t) {
    auto key = qHash(t);
    withLock<Write>(tasksLock, [&key, t] {
        while (knownTasks.contains(key))
            ++key;
        knownTasks[key] = t;
    });
    return QByteArray().setNum(key, 16).prepend("Task(0x").append(')');
}

void Assistant::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    QByteArray logLine = message.toLocal8Bit();
    const char *file = context.file ? context.file : "";

    // Search the log line for a Task code tag
    bool foundTask = false;
    const static auto taskPrefix = QLatin1String("Task(0x");
    int taskPos = logLine.indexOf(taskPrefix);
    int endPos = logLine.indexOf(')', taskPos);
    if (taskPos != -1 && endPos != -1) {
        // Decode the task code
        auto pointerPos = taskPos+taskPrefix.size();
        auto length = endPos - pointerPos;
        bool convertOk = false;
        auto taskCode = logLine.mid(pointerPos, length).toUInt(&convertOk, 16);

        // Fetch task pointer from code
        const Task* task = nullptr;
        withLock<Read>(tasksLock, [&task, &taskCode] { task = knownTasks.take(taskCode); });

        if (convertOk && task != nullptr) {
            // Remove the whole Task(0x...) segment from the string
            logLine.remove(taskPos, length+taskPrefix.size()+1);
            // If there's an extra space, remove it as well
            if (logLine.length() > taskPos && logLine[taskPos] == ' ')
                logLine.remove(taskPos, 1);

            // TODO: Associate the log with the task
            foundTask = true;
            Assistant* assistant = task->getAssistant();
        }
    }

    // If the log is not associated with any task, notify all known assistants of it.
    if (!foundTask) {
        // TODO: Notify all assistants of the log
    }


#define BASEFORM ": [%s:%u] %s\n"
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug" BASEFORM, file, context.line, logLine.constData());
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info" BASEFORM, file, context.line, logLine.constData());
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning" BASEFORM, file, context.line, logLine.constData());
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical" BASEFORM, file, context.line, logLine.constData());
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal" BASEFORM, file, context.line, logLine.constData());
        break;
    }
#undef BASEFORM
}

Task* Assistant::createTask() {
    Task* t = new Task(this);
    taskList.append(t);
    emit tasksChanged(taskList);
    return t;
}

// Constexpr sqrt based on Newton-Raphson method. Adapted from https://stackoverflow.com/a/34134071
double constexpr nr_sqrt(double x, double a = 1, double b = 0) { return (a == b)? a : nr_sqrt(x, 0.5*(a+x/a), a); }

void drawRing(QPainter* painter, QSizeF size) {
    // Leave 1 pixel padding around entire path
    auto edge = std::min(size.width(), size.height()) - 2;
    QSizeF canvasSize(edge, edge);
    QPainterPath path;
    constexpr auto ringScale = .1;
    constexpr auto root2 = nr_sqrt(2.0);
    auto ringWidth = edge * ringScale;
    auto gapWidth = ringWidth / 2;
    auto leg = gapWidth / root2;

    // First, draw the outer circle of the ring filling the entire canvas
    path.addEllipse(QRectF(QPointF(), canvasSize));

    // Cut out the center of the ring
    {
        QPainterPath cutout;
        cutout.addEllipse(QRectF(QPointF(ringWidth, ringWidth), canvasSize*(1 - 2*ringScale)));
        path = path.subtracted(cutout);
    }

    // Cut out the four gaps in the ring
    {
        QPainterPath cutout;
        cutout.moveTo(ringWidth/2, edge/2 + leg);
        cutout.lineTo(cutout.currentPosition().x() + ringWidth, cutout.currentPosition().y() - ringWidth);
        cutout.lineTo(cutout.currentPosition().x() - leg, cutout.currentPosition().y() - leg);
        cutout.lineTo(cutout.currentPosition().x() - ringWidth*2, cutout.currentPosition().y() + ringWidth*2);
        cutout.lineTo(cutout.currentPosition().x() + leg, cutout.currentPosition().y() + leg);

        cutout.moveTo(edge/2 - leg, ringWidth/2);
        cutout.lineTo(cutout.currentPosition().x() + ringWidth, cutout.currentPosition().y() + ringWidth);
        cutout.lineTo(cutout.currentPosition().x() + leg, cutout.currentPosition().y() - leg);
        cutout.lineTo(cutout.currentPosition().x() - ringWidth*2, cutout.currentPosition().y() - ringWidth*2);
        cutout.lineTo(cutout.currentPosition().x() - leg, cutout.currentPosition().y() + leg);

        cutout.moveTo(edge - ringWidth/2, edge/2 - leg);
        cutout.lineTo(cutout.currentPosition().x() - ringWidth, cutout.currentPosition().y() + ringWidth);
        cutout.lineTo(cutout.currentPosition().x() + leg, cutout.currentPosition().y() + leg);
        cutout.lineTo(cutout.currentPosition().x() + ringWidth*2, cutout.currentPosition().y() - ringWidth*2);
        cutout.lineTo(cutout.currentPosition().x() - leg, cutout.currentPosition().y() - leg);

        cutout.moveTo(edge/2 - leg, edge - ringWidth/2);
        cutout.lineTo(cutout.currentPosition().x() + ringWidth, cutout.currentPosition().y() + ringWidth);
        cutout.lineTo(cutout.currentPosition().x() + leg, cutout.currentPosition().y() - leg);
        cutout.lineTo(cutout.currentPosition().x() - ringWidth*2, cutout.currentPosition().y() - ringWidth*2);
        cutout.lineTo(cutout.currentPosition().x() - leg, cutout.currentPosition().y() + leg);

        path = path.subtracted(cutout);
    }

    path.translate(1, 1);
    painter->fillPath(path, QBrush(Qt::black));
    painter->strokePath(path, QPen(Qt::white, canvasSize.width() * .001));
}

void drawVee(QPainter* painter, QSizeF size) {
    // Leave 1 pixel padding around entire path
    auto edge = std::min(size.width(), size.height()) - 2;
    QSizeF canvasSize(edge, edge);
    QPainterPath path;
    auto veeHeight = canvasSize.height() * .421;
    auto veeWidth = veeHeight * 1.1;
    auto veeSlope = 1.52;
    auto pointSlope = 1.538;
    auto hCenter = canvasSize.width() / 2;
    // The vee sits slightly below vertical center
    auto veeBase = canvasSize.height()/2 + veeHeight/1.6;
    auto veeTop = veeBase - veeHeight;
    auto veeLeft = canvasSize.width()/2 - veeWidth/2;
    auto veeRight = veeLeft + veeWidth;

    // Start at bottom point
    QPointF bottomPoint(hCenter, veeBase);
    auto deltaX = veeRight - hCenter;
    // Draw a line up to the right edge
    QPointF rightEdge(veeRight, veeBase - (deltaX*veeSlope));
    auto deltaY = rightEdge.y() - veeTop;
    // Draw a line up to the top of the right point
    QPointF rightTip(rightEdge.x() + deltaY/-pointSlope, veeTop);
    // Draw a line back to the center, now on the higher edge of the V
    deltaX = rightTip.x() - hCenter;
    QPointF higherEdgeBottomPoint(hCenter, rightTip.y() + deltaX*veeSlope);
    // We've now drawn the right half of the vee. Calculate the left half points by reflection.
    QPointF leftTip(veeLeft + (veeRight - rightTip.x()), veeTop);
    QPointF leftEdge(veeLeft, rightEdge.y());

    QPolygonF vee({bottomPoint, rightEdge, rightTip, higherEdgeBottomPoint, leftTip, leftEdge, bottomPoint});
    path.addPolygon(vee);

    path.translate(1, 1);
    painter->fillPath(path, QBrush(Qt::black));
    painter->strokePath(path, QPen(Qt::white, canvasSize.width() * .001));
}

class AssistantImageProvider : public QQuickImageProvider {
public:
    AssistantImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override {
        QSize paintSize(100, 100);
        if (!requestedSize.isEmpty())
            paintSize = requestedSize;
        *size = paintSize;

        QImage image(paintSize, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);

        if (id == "ring")
            drawRing(&painter, paintSize);
        else if (id == "vee")
            drawVee(&painter, paintSize);
        else
            qCritical() << "AssistantImageProvider: Asked to provide image with unknown ID" << id;

        return image;
    }
};

QQuickImageProvider* Assistant::getLogoProvider() { return new AssistantImageProvider(); }
