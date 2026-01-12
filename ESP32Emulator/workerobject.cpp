#include "workerobject.h"
#include <QMetaObject>

WorkerObject::WorkerObject(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &WorkerObject::onTimerTimeout);
    m_timer.setInterval(1); // 1 ms ≈ 1000 Hz
}

void WorkerObject::onStartRequested() {
    if (!m_core.isRunning()) {
        m_core.start();
        m_timer.start();
        emit logMessage("Эмуляция стартовала\n");
        emit emulationStateChanged(true);
    }
}

void WorkerObject::onStopRequested() {
    if (m_core.isRunning()) {
        m_timer.stop();
        m_core.stop();
        emit logMessage("Эмуляция остановлена\n");
        emit emulationStateChanged(false);
    }
}

void WorkerObject::onStepRequested() {
    m_core.step();
    emit logMessage("Выполнен один шаг эмуляции\n");
}

void WorkerObject::onTimerTimeout() {
    m_core.step();
}