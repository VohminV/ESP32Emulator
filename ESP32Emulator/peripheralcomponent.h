#ifndef PERIPHERALCOMPONENT_H
#define PERIPHERALCOMPONENT_H

#include <cstdint>
#include <functional>

// Forward declarations
class EmulatorCore;
class XtensaCPU;

/**
 * @brief Abstract base class for all emulated peripheral devices (UART, Timer, GPIO, etc.).
 * Provides a standard interface for the EmulatorCore to interact with peripherals.
 */
class PeripheralComponent {
public:
    /**
     * @brief Constructor.
     * @param baseAddress The base memory address of the peripheral's register block.
     * @param size The size of the register block in bytes.
     * @param interruptNumber The interrupt number associated with this peripheral (or -1 if none).
     */
    PeripheralComponent(uint32_t baseAddress, size_t size, int interruptNumber = -1);

    virtual ~PeripheralComponent() = default;

    // --- Getters for EmulatorCore ---
    uint32_t getBaseAddress() const { return m_baseAddress; }
    size_t getSize() const { return m_size; }
    int getInterruptNumber() const { return m_interruptNumber; }

    // --- Callbacks from MemoryMap (must be implemented by derived classes) ---
    /**
     * @brief Called when the CPU reads from this peripheral's address space.
     * @param cpu Pointer to the CPU that initiated the read.
     * @param address The full address being read.
     * @return The 32-bit value to return to the CPU.
     */
    virtual uint32_t read(XtensaCPU* cpu, uint32_t address) = 0;

    /**
     * @brief Called when the CPU writes to this peripheral's address space.
     * @param cpu Pointer to the CPU that initiated the write.
     * @param address The full address being written to.
     * @param value The 32-bit value being written.
     */
    virtual void write(XtensaCPU* cpu, uint32_t address, uint32_t value) = 0;

    // --- Lifecycle and event callbacks (must be implemented by derived classes) ---
    /**
     * @brief Called once per global emulation tick.
     * Used for internal state updates (e.g., timer counting, UART shift register).
     * @param currentTick The current global tick count.
     */
    virtual void onTick(uint64_t currentTick) = 0;

    // --- Utility methods provided by the base class ---
    /**
     * @brief Sets the parent EmulatorCore for callbacks.
     * Called automatically by EmulatorCore::addPeripheral.
     * @param core Pointer to the parent emulator core.
     */
    void setEmulatorCore(EmulatorCore* core) { m_emulatorCore = core; }

    /**
     * @brief Schedules an event to occur after a specified number of microseconds.
     * @param microseconds Delay in microseconds.
     * @param callback The function to call when the event fires.
     */
    void scheduleEventInUs(uint64_t microseconds, std::function<void()> callback);

    /**
     * @brief Schedules an event to occur at a specific global tick.
     * @param tick The global tick count for the event.
     * @param callback The function to call when the event fires.
     */
    void scheduleEventAtTick(uint64_t tick, std::function<void()> callback);

    /**
     * @brief Requests an interrupt from the EmulatorCore.
     * This should be called when the peripheral needs CPU attention.
     */
    void requestInterrupt();

protected:
    EmulatorCore* m_emulatorCore = nullptr;

private:
    const uint32_t m_baseAddress;
    const size_t m_size;
    const int m_interruptNumber;
};

#endif // PERIPHERALCOMPONENT_H