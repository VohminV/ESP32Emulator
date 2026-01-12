#include "emulatorcore.h"
#include <QDebug>

EmulatorCore::EmulatorCore() = default;

EmulatorCore::~EmulatorCore() {
    stop();
}

void EmulatorCore::start() {
    if (m_running) return;
    m_running = true;

    for (auto& p : m_peripherals) {
        p->init();
    }
}

void EmulatorCore::stop() {
    if (!m_running) return;
    m_running = false;

    for (auto& p : m_peripherals) {
        p->deinit();
    }
}

void EmulatorCore::step() {
    if (!m_running) return;
    constexpr uint32_t STEP_TIME_MS = 1;
    for (auto& p : m_peripherals) {
        p->tick(STEP_TIME_MS);
    }
}

void EmulatorCore::addPeripheral(std::unique_ptr<PeripheralComponent> peripheral) {
    m_peripherals.push_back(std::move(peripheral));
}