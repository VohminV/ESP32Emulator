#include "memorymap.h"
#include "xtensacpu.h" // Include CPU definition for callbacks
#include <fstream>
#include <cstring>
#include <stdexcept>

MemoryMap::MemoryMap() {
    initializeDefaultMap();
}

void MemoryMap::initializeDefaultMap() {
    m_memoryRegions.clear();
    m_peripheralHandlers.clear();

    // === Внутренняя память (Internal SRAM) ===
    addMemoryRegion(0x3FFB0000, 0x80000, MemoryType::DRAM); // 512 KB DRAM
    addMemoryRegion(0x40080000, 0x40000, MemoryType::IRAM); // 256 KB IRAM

    // === Внешняя флеш-память (External Flash) ===
    addMemoryRegion(0x3F000000, 0x400000, MemoryType::Flash); // 4 MB Flash

    // === ROM (Boot ROM) ===
    addMemoryRegion(0x40000000, 0x8000, MemoryType::ROM); // 32 KB Boot ROM

    // === RTC Memory ===
    addMemoryRegion(0x50000000, 0x2000, MemoryType::RTC_FAST); // 8 KB Fast
    addMemoryRegion(0x50002000, 0x2000, MemoryType::RTC_SLOW); // 8 KB Slow

    // === Периферийные регистры (Peripheral Registers) ===
    // Регионы объявлены, но обработчики будут зарегистрированы позже через registerPeripheralHandler
    addMemoryRegion(0x3FF40000, 0x1000, MemoryType::Peripheral); // UART0
    addMemoryRegion(0x3FF5F000, 0x1000, MemoryType::Peripheral); // TIMG0

    // Инициализация выделенной памяти для каждого региона
    for (auto& region : m_memoryRegions) {
        if (region.type != MemoryType::Peripheral) {
            region.data = std::make_unique<uint8_t[]>(region.size);
            std::memset(region.data.get(), 0xFF, region.size); // Flash обычно инициализируется 0xFF
        }
    }
}

void MemoryMap::addMemoryRegion(uint32_t baseAddress, size_t size, MemoryType type) {
    m_memoryRegions.push_back({baseAddress, size, type, nullptr});
}

void MemoryMap::loadFirmware(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open firmware file: " + filePath);
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Найти регион Flash
    MemoryRegion* flashRegion = nullptr;
    for (auto& region : m_memoryRegions) {
        if (region.type == MemoryType::Flash) {
            flashRegion = &region;
            break;
        }
    }

    if (!flashRegion) {
        throw std::runtime_error("Flash memory region not found");
    }

    if (static_cast<size_t>(fileSize) > flashRegion->size) {
        throw std::runtime_error("Firmware file is larger than Flash region");
    }

    // Загрузить файл в память Flash
    file.read(reinterpret_cast<char*>(flashRegion->data.get()), fileSize);
    // Оставшаяся часть Flash остаётся заполненной 0xFF
}

void MemoryMap::registerPeripheralHandler(uint32_t baseAddress, size_t size, PeripheralCallback callback) {
    m_peripheralHandlers.push_back({baseAddress, size, callback});
}

uint32_t MemoryMap::read(XtensaCPU* cpu, uint32_t address) {
    auto* memRegion = findMemoryRegion(address);
    if (!memRegion) {
        throw std::runtime_error("Read from unmapped address: 0x" + std::to_string(address));
    }

    if (memRegion->type == MemoryType::Peripheral) {
        auto* periphHandler = findPeripheralHandler(address);
        if (periphHandler) {
            // Вызвать зарегистрированный обработчик
            return periphHandler->callback(cpu, address, 0, false);
        } else {
            // Нет обработчика, возвращаем 0 по умолчанию
            return 0;
        }
    }

    // Чтение из обычной памяти
    size_t offset = address - memRegion->baseAddress;
    return *reinterpret_cast<uint32_t*>(memRegion->data.get() + offset);
}

void MemoryMap::write(XtensaCPU* cpu, uint32_t address, uint32_t value) {
    auto* memRegion = findMemoryRegion(address);
    if (!memRegion) {
        throw std::runtime_error("Write to unmapped address: 0x" + std::to_string(address));
    }

    if (memRegion->type == MemoryType::Peripheral) {
        auto* periphHandler = findPeripheralHandler(address);
        if (periphHandler) {
            // Вызвать зарегистрированный обработчик
            periphHandler->callback(cpu, address, value, true);
        }
        // Если обработчика нет, запись просто игнорируется
        return;
    }

    // Запись в обычную память
    size_t offset = address - memRegion->baseAddress;
    *reinterpret_cast<uint32_t*>(memRegion->data.get() + offset) = value;
}

MemoryMap::MemoryRegion* MemoryMap::findMemoryRegion(uint32_t address) {
    for (auto& region : m_memoryRegions) {
        if (address >= region.baseAddress && address < (region.baseAddress + region.size)) {
            return &region;
        }
    }
    return nullptr;
}

MemoryMap::PeripheralHandler* MemoryMap::findPeripheralHandler(uint32_t address) {
    for (auto& handler : m_peripheralHandlers) {
        if (address >= handler.baseAddress && address < (handler.baseAddress + handler.size)) {
            return &handler;
        }
    }
    return nullptr;
}