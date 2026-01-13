#ifndef MEMORYMAP_H
#define MEMORYMAP_H

#include <cstdint>
#include <vector>
#include <memory>
#include <functional> // for std::function

// Forward declaration
class XtensaCPU;

/**
 * @brief Manages the entire memory address space of the emulated ESP32.
 * Handles reads/writes to RAM, ROM, Flash, and Peripheral registers.
 */
class MemoryMap {
public:
    enum class MemoryType {
        DRAM,
        IRAM,
        Flash,
        ROM,
        RTC_FAST,
        RTC_SLOW,
        Peripheral
    };

    /**
     * @brief Callback function type for peripheral register access.
     * @param cpu The CPU that initiated the access.
     * @param address The full 32-bit address being accessed.
     * @param value For writes: the value to write. For reads: ignored.
     * @param isWrite True if this is a write operation, false for read.
     * @return For reads: the value to return to the CPU. For writes: ignored.
     */
    using PeripheralCallback = std::function<uint32_t(XtensaCPU* cpu, uint32_t address, uint32_t value, bool isWrite)>;

    MemoryMap();

    /**
     * @brief Loads a firmware binary into the Flash memory region.
     * @param filePath Path to the .bin file.
     * @throws std::runtime_error if the file cannot be loaded or is too large.
     */
    void loadFirmware(const std::string& filePath);

    /**
     * @brief Registers a callback for a specific peripheral register range.
     * @param baseAddress Start address of the peripheral block.
     * @param size Size of the block in bytes.
     * @param callback The function to call on access.
     */
    void registerPeripheralHandler(uint32_t baseAddress, size_t size, PeripheralCallback callback);

    // Core memory access methods called by XtensaCPU
    uint32_t read(XtensaCPU* cpu, uint32_t address);
    void write(XtensaCPU* cpu, uint32_t address, uint32_t value);

private:
    struct MemoryRegion {
        uint32_t baseAddress;
        size_t size;
        MemoryType type;
        std::unique_ptr<uint8_t[]> data; // nullptr for peripherals
    };

    struct PeripheralHandler {
        uint32_t baseAddress;
        size_t size;
        PeripheralCallback callback;
    };

    std::vector<MemoryRegion> m_memoryRegions;
    std::vector<PeripheralHandler> m_peripheralHandlers;

    void initializeDefaultMap();
    void addMemoryRegion(uint32_t baseAddress, size_t size, MemoryType type);
    MemoryRegion* findMemoryRegion(uint32_t address);
    PeripheralHandler* findPeripheralHandler(uint32_t address);
};

#endif // MEMORYMAP_H