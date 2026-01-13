#include "peripheralcomponent.h"
#include "emulatorcore.h"

PeripheralComponent::PeripheralComponent(uint32_t baseAddress)
    : m_baseAddress(baseAddress) {}

void PeripheralComponent::scheduleEventInUs(uint64_t microseconds, std::function<void()> callback)
{
    if (m_emulatorCore) {
        uint64_t ticks = microseconds * 80;
        uint64_t currentTick = m_emulatorCore->getCurrentTick();
        m_emulatorCore->scheduleEvent(currentTick + ticks, std::move(callback));
    }
}

void PeripheralComponent::scheduleEventAtTick(uint64_t tick, std::function<void()> callback) {
    if (m_emulatorCore) {
        m_emulatorCore->scheduleEvent(tick, std::move(callback));
    }
}

// Чисто виртуальные методы — оставьте как есть (реализуйте в наследниках)
uint32_t PeripheralComponent::readRegister(uint32_t) { return 0; }
void PeripheralComponent::writeRegister(uint32_t, uint32_t) {}
void PeripheralComponent::onTick(uint64_t) {}
void PeripheralComponent::requestInterrupt() {}