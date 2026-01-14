// frc1timer.cpp
#include "frc1timer.h"
#include "emulatorcore.h"
#include <iostream>

Frc1Timer::Frc1Timer(EmulatorCore* core, uint32_t baseAddr)
    : PeripheralComponent(baseAddr, 0x20, TIMER_IRQ)
    , m_core(core)
{
}

uint32_t Frc1Timer::read(uint32_t addr) {
    switch (addr & 0x1F) {
        case 0x00: return m_counter;           // FRC1_COUNT
        case 0x0C: return m_reload;            // FRC1_RELOAD
        default: return 0;
    }
}

void Frc1Timer::write(uint32_t addr, uint32_t value) {
    switch (addr & 0x1F) {
        case 0x04: // FRC1_LOAD
            m_counter = value;
            break;

        case 0x08: // FRC1_CTRL
            if ((value & 1) && !m_enabled) {
                m_enabled = true;

                // Используем std::function для рекурсии
                std::function<void()> tickCallback;
                tickCallback = [this, &tickCallback]() {
                    if (!m_enabled || !m_core->isRunning()) return;
                    m_counter++;
                    if (m_counter >= m_reload && m_reload > 0) {
                        m_counter = 0;
                        m_core->requestInterrupt(TIMER_IRQ);
                    }
                    // Рекурсивный вызов через захваченную ссылку
                    m_core->scheduleEventRelative(1000, tickCallback);
                };

                m_core->scheduleEventRelative(1000, tickCallback);
            } else if (!(value & 1)) {
                m_enabled = false;
            }
            break;

        case 0x0C: // FRC1_RELOAD
            m_reload = value;
            break;
    }
}