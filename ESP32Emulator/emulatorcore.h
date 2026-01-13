#ifndef EMULATORCORE_H
#define EMULATORCORE_H

#include "peripheralcomponent.h"
#include "xtensacpu.h"
#include "memorymap.h"
#include "firmwareloader.h"

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <functional>
#include <mutex>

class EmulatorCore
{
public:
    explicit EmulatorCore();
    ~EmulatorCore();

    // Загрузка прошивки (ELF или BIN)
    bool loadFirmware(const std::string& path);

    // Управление выполнением
    void start();   // Запускает эмуляцию в отдельном потоке
    void stop();    // Останавливает эмуляцию
    void step();    // Выполняет один такт CPU

    // Добавление периферии
    void addPeripheral(std::unique_ptr<PeripheralComponent> peripheral);

    // Чтение/запись в адресное пространство SoC
    uint32_t readMemory(uint32_t address);
    void writeMemory(uint32_t address, uint32_t value);

    // Тайминг
    void tick(); // Выполняет один такт всей системы
    void executeForUs(uint64_t us); // Выполняет указанное количество микросекунд
	void scheduleEvent(uint64_t tick, std::function<void()> callback);
    bool isRunning() const { return m_running.load(); }
	uint64_t getCurrentTick() const { return m_globalTick.load(); } 
	
private:
    void emulationLoop(); // Основной цикл эмуляции
    void dispatchPendingInterrupts();

    // Компоненты
    std::unique_ptr<XtensaCPU> m_cpu0;        // PRO_CPU
    std::unique_ptr<XtensaCPU> m_cpu1;        // APP_CPU
    std::unique_ptr<MemoryMap> m_memoryMap;

    std::vector<std::unique_ptr<PeripheralComponent>> m_peripherals;

    // События и прерывания
    struct TimedEvent {
        uint64_t tick;
        std::function<void()> callback;
        bool operator>(const TimedEvent& other) const { return tick > other.tick; }
    };
    std::priority_queue<TimedEvent, std::vector<TimedEvent>, std::greater<TimedEvent>> m_eventQueue;
    std::mutex m_eventMutex;

    // Управление выполнением
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_globalTick{0}; // Глобальный тактовый счётчик (в тактах APB = 80 МГц)
    std::thread m_emulationThread;
};

#endif // EMULATORCORE_H