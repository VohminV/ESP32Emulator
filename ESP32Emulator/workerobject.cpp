// workerobject.cpp
#include "workerobject.h"
#include "uartperipheral.h"
#include "frc1timer.h"
#include "dportperipheral.h"
#include <QDebug>

WorkerObject::WorkerObject(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &WorkerObject::onTimerTimeout);
    m_timer.setInterval(1); // 1 ms ≈ 1000 Hz
}

void WorkerObject::onStartRequested() {
    if (m_core.isRunning()) return;

    // === 1. Загружаем прошивку (если ещё не загружена) ===
    if (!m_core.isFirmwareLoaded()) {
        if (m_firmwarePath.isEmpty()) {
            emit logMessage("❌ Путь к прошивке не задан!\n");
            return;
        }
        if (!m_core.loadFirmware(m_firmwarePath.toStdString())) {
            emit logMessage("❌ Ошибка загрузки прошивки!\n");
            return;
        }
        emit logMessage("✅ Прошивка загружена\n");
    }

    // === 2. Добавляем периферию (только один раз) ===
    if (!m_peripheralsAdded) {
        try {
            m_core.addPeripheral(std::make_unique<UartPeripheral>());
            m_core.addPeripheral(std::make_unique<Frc1Timer>(&m_core));
            m_core.addPeripheral(std::make_unique<DportPeripheral>(&m_core));
            m_peripheralsAdded = true;
            emit logMessage("✅ Периферия добавлена: UART, FRC1, DPORT\n");
        } catch (const std::exception& e) {
            emit logMessage(QString("❌ Ошибка инициализации периферии: %1\n").arg(e.what()));
            return;
        }
    }

    // === 3. Запускаем эмуляцию ===
    m_core.start();
    m_timer.start();
    emit logMessage("▶ Эмуляция стартовала\n");
    emit emulationStateChanged(true);
}

void WorkerObject::onStopRequested() {
    if (m_core.isRunning()) {
        m_timer.stop();
        m_core.stop();
        emit logMessage("⏹ Эмуляция остановлена\n");
        emit emulationStateChanged(false);
    }
}

void WorkerObject::onStepRequested() {
    if (!m_core.isFirmwareLoaded()) {
        emit logMessage("⚠ Сначала загрузите прошивку!\n");
        return;
    }
    if (!m_peripheralsAdded) {
        // Можно добавить периферию и здесь, но лучше в onStart
    }
    m_core.step();
    emit logMessage("StepThrough выполнен\n");
}

void WorkerObject::onTimerTimeout() {
    if (m_core.isRunning()) {
        m_core.step();
    }
}