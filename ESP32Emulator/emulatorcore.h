// emulatorcore.h
#ifndef EMULATORCORE_H
#define EMULATORCORE_H

#include "xtensacpu.h"
#include "memorymap.h"
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>

class PeripheralComponent; // Forward declaration

class EmulatorCore {
public:
    EmulatorCore();
    ~EmulatorCore();

    bool loadFirmware(const std::string& path);
    
    void start();
    void stop();
    void step(); // Execute a single tick

    // Планировщик событий
    void scheduleEvent(uint64_t tick, std::function<void()> callback);
    void scheduleEventRelative(uint64_t ticksFromNow, std::function<void()> callback);

    // API для отладки и интеграции
    uint32_t readMemory(uint32_t address); // Чтение данных
    void writeMemory(uint32_t address, uint32_t value); // Запись данных

    void addPeripheral(std::unique_ptr<PeripheralComponent> peripheral);
    void requestInterrupt(int irqNumber); // Вызывается из периферии

    bool isRunning() const;

private:
    struct ScheduledEvent {
        uint64_t tick;
        std::function<void()> callback;
        bool operator>(const ScheduledEvent& other) const { return tick > other.tick; }
    };

    // Константы
    static constexpr uint64_t APB_CLK_HZ = 80'000'000ULL;
    static constexpr uint64_t TICKS_PER_US = APB_CLK_HZ / 1'000'000ULL;

    // Состояние эмулятора
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_globalTick{0};
	std::thread m_emulationThread;
    
	// Системные компоненты
    std::unique_ptr<MemoryMap> m_memoryMap;
    std::unique_ptr<XtensaCPU> m_cpu0; // PRO_CPU
    std::unique_ptr<XtensaCPU> m_cpu1; // APP_CPU

    // Прерывания
    std::atomic<uint32_t> m_pendingIrqMask{0}; // Битовая маска ожидающих IRQ

    // Периферия и события
    std::vector<std::unique_ptr<PeripheralComponent>> m_peripherals;
    std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, std::greater<ScheduledEvent>> m_eventQueue;
    std::mutex m_eventMutex;

    // Основной цикл эмуляции
    void tick();
    void executeForUs(uint64_t us);
    void dispatchPendingInterrupts();
};

#endif // EMULATORCORE_H