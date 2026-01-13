#include "memorymap.h"
#include "xtensacpu.h"
#include <fstream>
#include <cstring>
#include <stdexcept>

MemoryMap::MemoryMap() {
    initializeDefaultMap();
}

void MemoryMap::initializeDefaultMap() {
    m_memoryRegions.clear();
    m_peripheralHandlers.clear();

    // === Внутренняя память (согласно Table 4-1, ESP32 Datasheet v5.2) ===
    addMemoryRegion(0x40000000, 0x60000, MemoryType::ROM);        // Boot ROM (384 KB)
    addMemoryRegion(0x3FF90000, 0x10000, MemoryType::ROM);        // Internal ROM 1 (64 KB)

    addMemoryRegion(0x40070000, 0x30000, MemoryType::IRAM);       // IRAM0 (192 KB)
    addMemoryRegion(0x400A0000, 0x20000, MemoryType::IRAM);       // IRAM1 alias (128 KB)

    addMemoryRegion(0x3FFE0000, 0x20000, MemoryType::DRAM);       // DRAM (128 KB)
    addMemoryRegion(0x3FFAE000, 0x32000, MemoryType::DRAM);       // DRAM2 (200 KB)

    addMemoryRegion(0x3FF80000, 0x2000, MemoryType::RTC_FAST);    // RTC FAST (8 KB)
    addMemoryRegion(0x400C0000, 0x2000, MemoryType::RTC_FAST);    // RTC FAST alias

    addMemoryRegion(0x50000000, 0x2000, MemoryType::RTC_SLOW);    // RTC SLOW (8 KB)

    // === Внешняя Flash (XIP) ===
    addMemoryRegion(0x3F400000, 0x400000, MemoryType::Flash);     // External Flash (4 MB)
    addMemoryRegion(0x400C2000, 0xB3F000, MemoryType::Flash);     // XIP region (11 MB + 248 KB)

    // === Периферийные регистры ===
    addMemoryRegion(0x3FF00000, 0x10000, MemoryType::Peripheral); // Peripheral base
    // Конкретные блоки (UART0, TIMG0 и др.) будут обрабатываться через callback'и

    // Инициализация выделенной памяти
    for (auto& region : m_memoryRegions) {
        if (region.type != MemoryType::Peripheral) {
            region.data = std::make_unique<uint8_t[]>(region.size);
            if (region.type == MemoryType::Flash) {
                std::memset(region.data.get(), 0xFF, region.size); // Flash инициализируется 0xFF
            } else {
                std::memset(region.data.get(), 0x00, region.size); // RAM — нулями
            }
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

    // Ищем основной регион Flash (XIP)
    MemoryRegion* flashRegion = nullptr;
    for (auto& region : m_memoryRegions) {
        if (region.type == MemoryType::Flash && region.baseAddress == 0x400C2000) {
            flashRegion = &region;
            break;
        }
    }

    if (!flashRegion || static_cast<size_t>(fileSize) > flashRegion->size) {
        throw std::runtime_error("Firmware too large or Flash region not found");
    }

    file.read(reinterpret_cast<char*>(flashRegion->data.get()), fileSize);
}

void MemoryMap::registerPeripheralHandler(uint32_t baseAddress, size_t size, PeripheralCallback callback) {
    m_peripheralHandlers.push_back({baseAddress, size, callback});
}

// --- Harvard-архитектура: отдельные методы для инструкций и данных ---

uint32_t MemoryMap::readInstruction(uint32_t address) {
    auto* region = findMemoryRegion(address);
    if (!region) {
        throw std::runtime_error("Instruction fetch from unmapped address: 0x" + std::to_string(address));
    }

    // Инструкции можно читать только из IRAM, ROM, Flash
    if (region->type != MemoryType::IRAM &&
        region->type != MemoryType::ROM &&
        region->type != MemoryType::Flash) {
        throw std::runtime_error("Illegal instruction fetch from data region: 0x" + std::to_string(address));
    }

    if (region->type == MemoryType::Peripheral) {
        // Это не должно происходить при правильной настройке
        return 0;
    }

    size_t offset = address - region->baseAddress;
    return *reinterpret_cast<uint32_t*>(region->data.get() + offset);
}

uint32_t MemoryMap::readData(uint32_t address) {
    auto* region = findMemoryRegion(address);
    if (!region) {
        // Чтение из неотображённого адреса → возврат 0 (как в реальном железе)
        return 0;
    }

    if (region->type == MemoryType::Peripheral) {
        auto* handler = findPeripheralHandler(address);
        if (handler) {
            return handler->callback(nullptr, address, 0, false);
        }
        return 0; // По умолчанию
    }

    size_t offset = address - region->baseAddress;
    return *reinterpret_cast<uint32_t*>(region->data.get() + offset);
}

void MemoryMap::writeData(uint32_t address, uint32_t value) {
    auto* region = findMemoryRegion(address);
    if (!region) {
        // Запись в неотображённый адрес игнорируется
        return;
    }

    if (region->type == MemoryType::Peripheral) {
        auto* handler = findPeripheralHandler(address);
        if (handler) {
            handler->callback(nullptr, address, value, true);
        }
        return;
    }

    // Защита от записи в ROM/Flash/IRAM (в общем случае)
    if (region->type == MemoryType::ROM ||
        region->type == MemoryType::Flash ||
        region->type == MemoryType::IRAM) {
        // В реальном ESP32 запись в IRAM разрешена, но для простоты пока запрещаем
        // TODO: реализовать защиту по MPU/MMU
        return;
    }

    size_t offset = address - region->baseAddress;
    *reinterpret_cast<uint32_t*>(region->data.get() + offset) = value;
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