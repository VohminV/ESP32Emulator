// peripheralcomponent.h
#ifndef PERIPHERALCOMPONENT_H
#define PERIPHERALCOMPONENT_H

#include <cstdint>
#include <cstddef>
#include <functional>

// Forward declaration
class PeripheralInterface;

class PeripheralComponent {
public:
    // Интерфейс для взаимодействия с системой
    struct SystemInterface {
        std::function<void(int irqNumber)> requestInterrupt;
        std::function<void(uint64_t ticks, std::function<void()>)> scheduleEventRelative;
        std::function<uint64_t()> getCurrentTick;
    };

    PeripheralComponent(uint32_t baseAddress, size_t size, int interruptNumber);
    
    // Метод для установки системного интерфейса (вызывается EmulatorCore)
    void setSystemInterface(SystemInterface sysInterface);

    // Виртуальные методы для работы с периферией
    virtual uint32_t read(uint32_t address) = 0;
    virtual void write(uint32_t address, uint32_t value) = 0;
    virtual void onTick(uint64_t currentTick) = 0; // Вызывается каждый такт эмуляции

    // Геттеры
    uint32_t getBaseAddress() const { return m_baseAddress; }
    size_t getSize() const { return m_size; }

protected:
    const uint32_t m_baseAddress;
    const size_t m_size;
    const int m_interruptNumber;
    SystemInterface m_sysInterface;

    // Удобные методы-обёртки
    void requestInterrupt() {
        if (m_sysInterface.requestInterrupt && m_interruptNumber >= 0) {
            m_sysInterface.requestInterrupt(m_interruptNumber);
        }
    }

    void scheduleEventInUs(uint64_t microseconds, std::function<void()> callback) {
        if (m_sysInterface.scheduleEventRelative) {
            constexpr uint64_t TICKS_PER_US = 80; // APB_CLK_HZ / 1'000'000
            m_sysInterface.scheduleEventRelative(microseconds * TICKS_PER_US, std::move(callback));
        }
    }
};

#endif // PERIPHERALCOMPONENT_H