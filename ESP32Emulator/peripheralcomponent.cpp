// peripheralcomponent.cpp
#include "peripheralcomponent.h"

PeripheralComponent::PeripheralComponent(uint32_t baseAddress, size_t size, int interruptNumber)
    : m_baseAddress(baseAddress)
    , m_size(size)
    , m_interruptNumber(interruptNumber) {}

void PeripheralComponent::setSystemInterface(PeripheralComponent::SystemInterface sysInterface) {
    m_sysInterface = std::move(sysInterface);
}