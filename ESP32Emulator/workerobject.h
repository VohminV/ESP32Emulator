// workerobject.h
#ifndef WORKEROBJECT_H
#define WORKEROBJECT_H

#include <QObject>
#include <QTimer>
#include "emulatorcore.h"

class WorkerObject : public QObject
{
    Q_OBJECT
public:
    explicit WorkerObject(QObject *parent = nullptr);
    void setFirmwarePath(const QString& path) { m_firmwarePath = path; }

public slots:
    void onStartRequested();
    void onStopRequested();
    void onStepRequested();

signals:
    void logMessage(const QString& message);
    void emulationStateChanged(bool running);

private slots:
    void onTimerTimeout();

private:
    EmulatorCore m_core;          
    QTimer m_timer;
    QString m_firmwarePath;
    bool m_peripheralsAdded = false;
};

#endif // WORKEROBJECT_H