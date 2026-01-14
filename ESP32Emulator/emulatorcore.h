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
#include <vector>
#include <filesystem>

class PeripheralComponent; // Forward declaration

/**
 * @brief Основной класс эмулятора ESP32.
 * Управляет двумя ядрами Xtensa LX6, памятью, периферией и планировщиком событий.
 * Поддерживает Harvard-архитектуру, прерывания и совместимость с бинарниками ESP-IDF.
 */
class EmulatorCore {
public:
    EmulatorCore();
    ~EmulatorCore();

    /**
     * @brief Загружает прошивку в XIP-регион внешней флеш-памяти (0x400C2000).
     * После загрузки PRO_CPU устанавливается на точку входа приложения (0x400D0000).
     * APP_CPU остаётся выключенным до явной инициализации.
     * @param path Путь к .bin файлу, сгенерированному ESP-IDF.
     * @return true при успехе, false при ошибке.
     */
    bool loadFirmware(const std::string& path);

    /**
     * @brief Запускает эмуляцию в отдельном потоке.
     */
    void start();

    /**
     * @brief Останавливает эмуляцию и дожидается завершения потока.
     */
    void stop();

    /**
     * @brief Выполняет один такт эмуляции (один вызов tick()).
     */
    void step();

    // === Планировщик событий ===
    void scheduleEvent(uint64_t tick, std::function<void()> callback);
    void scheduleEventRelative(uint64_t ticksFromNow, std::function<void()> callback);

    // === API для отладки и интеграции ===
    uint32_t readMemory(uint32_t address);  // Чтение данных из DRAM/DROM/периферии
    void writeMemory(uint32_t address, uint32_t value); // Запись в RAM/периферию

    /**
     * @brief Регистрирует периферийное устройство в системе.
     * Периферия получает доступ к таймеру, прерываниям и шине памяти.
     */
    void addPeripheral(std::unique_ptr<PeripheralComponent> peripheral);

    /**
     * @brief Запрашивает прерывание от периферийного устройства.
     * Вызывается из методов PeripheralComponent.
     */
    void requestInterrupt(int irqNumber);

    bool isRunning() const;
	bool compileProject(const std::filesystem::path& projectPath);
    std::string getEspIdfPath() const;
    void setEspIdfPath(const std::string& path);
private:
	uint32_t m_firmwareEntryPoint = 0x400D0000;
    struct ScheduledEvent {
        uint64_t tick;
        std::function<void()> callback;
        bool operator>(const ScheduledEvent& other) const { return tick > other.tick; }
    };

    // Константы тактирования (APB_CLK = 80 MHz)
    static constexpr uint64_t APB_CLK_HZ = 80'000'000ULL;
    static constexpr uint64_t TICKS_PER_US = APB_CLK_HZ / 1'000'000ULL;
	
    // Состояние эмулятора
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_globalTick{0};
    std::thread m_emulationThread;

    // Системные компоненты
    std::unique_ptr<MemoryMap> m_memoryMap;
    std::unique_ptr<XtensaCPU> m_cpu0; // PRO_CPU (ядро 0)
    std::unique_ptr<XtensaCPU> m_cpu1; // APP_CPU (ядро 1)
	std::string m_espIdfPath;
    // Прерывания: битовая маска ожидающих IRQ (0–31)
    std::atomic<uint32_t> m_pendingIrqMask{0};

    // Периферия и события
    std::vector<std::unique_ptr<PeripheralComponent>> m_peripherals;
    std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, std::greater<ScheduledEvent>> m_eventQueue;
    std::mutex m_eventMutex;

    // Основной цикл эмуляции
    void tick();
    void executeForUs(uint64_t us);
    void dispatchPendingInterrupts();
	bool runCommand(const std::string& cmd, const std::filesystem::path& cwd, std::string& output);
};

#endif // EMULATORCORE_H