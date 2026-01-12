#ifndef WORKEROBJECT_H
#define WORKEROBJECT_H

#include "emulatorcore.h"
#include <QObject>
#include <QTimer>

class WorkerObject : public QObject
{
    Q_OBJECT

public:
    explicit WorkerObject(QObject *parent = nullptr);

public slots:
    void onStartRequested();
    void onStopRequested();
    void onStepRequested();

signals:
    void logMessage(const QString& msg);
    void emulationStateChanged(bool running);

private slots:
    void onTimerTimeout();

private:
    EmulatorCore m_core;
    QTimer m_timer;
};

#endif // WORKEROBJECT_H