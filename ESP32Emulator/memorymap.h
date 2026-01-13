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
        RTC_FAST,  // RTC Fast Memory (accessible by main CPU)
        RTC_SLOW,  // RTC Slow Memory (for ULP coprocessor)
        Peripheral // Memory-mapped I/O registers
    };

    // Тип для callback-функций периферии
    using PeripheralCallback = std::function<uint32_t(XtensaCPU*, uint32_t addr, uint32_t value, bool isWrite)>;

    MemoryMap();
    
    // === Методы для работы с памятью ===
    uint32_t readInstruction(uint32_t address);
    uint32_t readData(uint32_t address);
    void writeData(uint32_t address, uint32_t value);

    void loadFirmware(const std::string& filePath);
    void registerPeripheralHandler(uint32_t baseAddress, size_t size, PeripheralCallback callback);

private:
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