// memorymap.h
#ifndef MEMORYMAP_H
#define MEMORYMAP_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>

class XtensaCPU;

class MemoryMap {
public:
    enum class MemoryType {
        IRAM,      // Instruction RAM (for code execution)
        DRAM,      // Data RAM
        Flash,     // External QSPI Flash (XIP)
        ROM,       // Internal Boot ROM
        RTC_FAST,  // RTC Fast Memory
        RTC_SLOW,  // RTC Slow Memory
        Peripheral // Memory-mapped I/O registers
    };

    using PeripheralCallback = std::function<uint32_t(XtensaCPU*, uint32_t addr, uint32_t value, bool isWrite)>;

    MemoryMap();

    // Harvard interface
    uint32_t readInstruction(uint32_t address);
    uint32_t readData(uint32_t address);
    void writeData(uint32_t address, uint32_t value);

    // Firmware loading
    void loadFirmware(const std::string& filePath);
    void loadEspImage(const std::string& path); 

    // Peripheral registration
    void registerPeripheralHandler(uint32_t baseAddress, size_t size, PeripheralCallback callback);

    // Entry point for ESP32 image
    uint32_t getEntryPoint() const { return m_entryPoint; } // ← ЗАКРЫВАЮЩАЯ СКОБКА ДОБАВЛЕНА

private:
    uint32_t m_entryPoint = 0x400D0000;

    struct MemoryRegion {
        uint32_t baseAddress;
        size_t size;
        MemoryType type;
        std::unique_ptr<uint8_t[]> data;
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