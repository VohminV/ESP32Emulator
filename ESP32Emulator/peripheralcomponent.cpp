#include "peripheralcomponent.h"
#include "emulatorcore.h"
#include <atomic>

// We need to know the APB clock frequency to convert microseconds to ticks.
// This should ideally be defined in a common config header.
constexpr uint64_t APB_CLK_HZ = 80'000'000ULL;
constexpr uint64_t TICKS_PER_US = APB_CLK_HZ / 1'000'000ULL; // 80

PeripheralComponent::PeripheralComponent(uint32_t baseAddress, size_t size, int interruptNumber)
    : m_baseAddress(baseAddress)
    , m_size(size)
    , m_interruptNumber(interruptNumber) {}

void PeripheralComponent::scheduleEventInUs(uint64_t microseconds, std::function<void()> callback) {
    if (m_emulatorCore) {
        // Since we don't have direct access to the current tick,
        // we rely on the fact that the event will be scheduled relative to "now".
        // The EmulatorCore's scheduler handles the absolute tick calculation.
        uint64_t ticks = microseconds * TICKS_PER_US;
        m_emulatorCore->scheduleEventRelative(ticks, std::move(callback));
    }
}

void PeripheralComponent::scheduleEventAtTick(uint64_t tick, std::function<void()> callback) {
    if (m_emulatorCore) {
        m_emulatorCore->scheduleEvent(tick, std::move(callback));
    }
}

void PeripheralComponent::requestInterrupt() {
    if (m_emulatorCore && m_interruptNumber >= 0) {
        m_emulatorCore->requestInterrupt(m_interruptNumber);
    }
}