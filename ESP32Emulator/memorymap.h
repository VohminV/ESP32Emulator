#ifndef MEMORYMAP_H
#define MEMORYMAP_H

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include "memoryblock.h"

// Предварительные объявления
class PeripheralComponent;
class MemoryBlock;

// Типы регионов памяти ESP32
enum class MemoryRegionType {
    IRAM,   // Instruction RAM (исполняемая, быстрая SRAM)
    DRAM,   // Data RAM (общая SRAM)
    IROM,   // Instruction ROM (Flash, исполняемая)
    DROM,   // Data ROM (Flash, только чтение)
    RTC_FAST, // RTC Fast Memory
    PERIPHERAL, // Регистры периферийных устройств
    INVALID
};

// Описание одного региона виртуального адресного пространства
struct MemoryRegion {
    uint32_t base_address = 0;
    size_t size = 0;
    MemoryRegionType type = MemoryRegionType::INVALID;
    bool read_allowed = false;
    bool write_allowed = false;
    bool execute_allowed = false;

    // Указатель на обработчик: либо MemoryBlock, либо PeripheralComponent
    union {
        MemoryBlock* memory_block = nullptr;
        PeripheralComponent* peripheral;
    } handler;

    // Для регионов PERIPHERAL этот флаг указывает, что handler.peripheral действителен
    bool is_peripheral = false;
};

class MemoryMap
{
public:
    MemoryMap();
    ~MemoryMap();

    // Инициализация стандартной карты памяти ESP32
    void initializeDefaultMap();

    // Динамическая переконфигурация (эмуляция MMU)
    bool mapRegion(uint32_t virt_addr, uint32_t phys_addr, size_t size, MemoryRegionType type);
    bool unmapRegion(uint32_t virt_addr, size_t size);

    // Регистрация периферийного устройства по его базовому адресу
    void registerPeripheral(uint32_t base_addr, PeripheralComponent* peripheral);

    // Основные операции чтения/записи
    uint32_t read(uint32_t address);
    void write(uint32_t address, uint32_t value);

    // Вспомогательные методы для FirmwareLoader
    void writeBlock(uint32_t address, const uint8_t* data, size_t size);
    void zeroBlock(uint32_t address, size_t size);

private:
    // Внутренние вспомогательные методы
    MemoryRegion* findRegion(uint32_t address);
    MemoryBlock* getOrCreateMemoryBlock(MemoryRegionType type, uint32_t base, size_t size);

    // Стандартные регионы памяти ESP32 (примерные адреса и размеры)
    static constexpr uint32_t IRAM_BASE = 0x40080000;
    static constexpr size_t IRAM_SIZE = 0x20000; // 128 KB

    static constexpr uint32_t DRAM_BASE = 0x3FFB0000;
    static constexpr size_t DRAM_SIZE = 0x50000; // 320 KB

    static constexpr uint32_t IROM_BASE = 0x400D0000;
    static constexpr size_t IROM_SIZE = 0x1000000; // 16 MB (макс. Flash)

    static constexpr uint32_t DROM_BASE = 0x3F400000;
    static constexpr size_t DROM_SIZE = 0x1000000; // 16 MB

    static constexpr uint32_t RTC_FAST_BASE = 0x3FF80000;
    static constexpr size_t RTC_FAST_SIZE = 0x2000; // 8 KB

    // Хранение данных
    std::vector<MemoryRegion> m_regions;
    std::unordered_map<MemoryRegionType, std::unique_ptr<MemoryBlock>> m_memoryBlocks;
};

#endif // MEMORYMAP_H