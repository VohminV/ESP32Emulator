#ifndef EMULATORCORE_H
#define EMULATORCORE_H

#include "xtensacpu.h"
#include "memorymap.h"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <functional>
#include <mutex>

// Forward declaration for PeripheralComponent
class PeripheralComponent;

/**
 * @brief The main coordinator of the entire ESP32 emulation.
 * Manages CPUs, memory, peripherals, and the global timing model.
 */
class EmulatorCore {
public:
    EmulatorCore();
    ~EmulatorCore();

    /**
     * @brief Loads a firmware binary into the emulated Flash.
     * @param path Path to the .bin file.
     * @return True on success, false otherwise.
     */
    bool loadFirmware(const std::string& path);

    /**
     * @brief Starts the emulation in a separate thread.
     */
    void start();

    /**
     * @brief Stops the emulation thread.
     */
    void stop();

    /**
     * @brief Executes a single emulation tick (for debugging).
     */
    void step();

    /**
     * @brief Adds a peripheral device to the system.
     * @param peripheral Unique pointer to the peripheral.
     */
    void addPeripheral(std::unique_ptr<PeripheralComponent> peripheral);

    // --- Public API for interaction ---
    uint32_t readMemory(uint32_t address);
    void writeMemory(uint32_t address, uint32_t value);
    void executeForUs(uint64_t us);

    // --- Callbacks for peripherals ---
    /**
     * @brief Schedules an event to be executed at a specific global tick.
     * @param tick The global tick count when the event should fire.
     * @param callback The function to execute.
     */
    void scheduleEvent(uint64_t tick, std::function<void()> callback);

    /**
     * @brief Requests an interrupt from a peripheral.
     * This is called by peripheral components.
     * @param irqNumber The interrupt number to request.
     */
    void requestInterrupt(int irqNumber);
	
	void scheduleEventRelative(uint64_t ticksFromNow, std::function<void()> callback);
	/**
     * @brief Checks if the emulation is currently running.
     * @return True if running, false otherwise.
     */
    bool isRunning() const;

private:
    // Main emulation loop
    void tick();
    void dispatchPendingInterrupts();

    // Core components
    std::unique_ptr<XtensaCPU> m_cpu0; // PRO_CPU
    std::unique_ptr<XtensaCPU> m_cpu1; // APP_CPU
    std::unique_ptr<MemoryMap> m_memoryMap;

    // Peripherals
    std::vector<std::unique_ptr<PeripheralComponent>> m_peripherals;
    std::atomic<int> m_pendingInterrupt{ -1 };

    // Threading and state
    std::atomic<bool> m_running{ false };
    std::thread m_emulationThread;
    std::atomic<uint64_t> m_globalTick{ 0 };

    // Event system
    struct ScheduledEvent {
        uint64_t tick;
        std::function<void()> callback;
        bool operator>(const ScheduledEvent& other) const { return tick > other.tick; }
    };
    std::priority_queue<ScheduledEvent, std::vector<ScheduledEvent>, std::greater<ScheduledEvent>> m_eventQueue;
    std::mutex m_eventMutex;
};

#endif // EMULATORCORE_H