#include "memorymap.h"
#include "xtensacpu.h"
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <iostream>

struct EspImageHeader {
    uint8_t magic;
    uint8_t segments;
    uint8_t flash_mode;
    uint8_t flash_size_freq;
    uint32_t entry_addr;
} __attribute__((packed));

struct SegmentHeader {
    uint32_t load_addr;
    uint32_t size;
} __attribute__((packed));
	

MemoryMap::MemoryMap() {
    initializeDefaultMap();
}

void MemoryMap::initializeDefaultMap() {
    m_memoryRegions.clear();
    m_peripheralHandlers.clear();

    // === Внутренняя память (ESP32 Datasheet v5.2, Table 4-1) ===
    addMemoryRegion(0x40000000, 0x60000, MemoryType::ROM);        // Boot ROM (384 KB)
    addMemoryRegion(0x3FF90000, 0x10000, MemoryType::ROM);        // Internal ROM 1 (64 KB)

    addMemoryRegion(0x40070000, 0x30000, MemoryType::IRAM);       // IRAM0 (192 KB)
    addMemoryRegion(0x400A0000, 0x20000, MemoryType::IRAM);       // IRAM1 alias (128 KB)

    addMemoryRegion(0x3FFE0000, 0x20000, MemoryType::DRAM);       // DRAM (128 KB)
    addMemoryRegion(0x3FFAE000, 0x32000, MemoryType::DRAM);       // DRAM2 (200 KB)

    addMemoryRegion(0x3FF80000, 0x2000, MemoryType::RTC_FAST);    // RTC FAST (8 KB)
    addMemoryRegion(0x400C0000, 0x2000, MemoryType::RTC_FAST);    // RTC FAST alias

    addMemoryRegion(0x50000000, 0x2000, MemoryType::RTC_SLOW);    // RTC SLOW (8 KB)

    // === Внешняя Flash (XIP — execute-in-place) ===
    // DROM: данные из флеша (константы)
    addMemoryRegion(0x3F400000, 0x400000, MemoryType::Flash);     // DROM (4 MB)
    // IROM: код из флеша (приложение)
    addMemoryRegion(0x400D0000, 0x330000, MemoryType::Flash);     // IROM (3.3 MB)

    // === Периферийные регистры ===
    addMemoryRegion(0x3FF00000, 0x10000, MemoryType::Peripheral); // Peripheral base

    // Инициализация выделенной памяти
    for (auto& region : m_memoryRegions) {
        if (region.type != MemoryType::Peripheral) {
            region.data = std::make_unique<uint8_t[]>(region.size);
            if (region.type == MemoryType::Flash || region.type == MemoryType::ROM) {
                std::memset(region.data.get(), 0xFF, region.size); // Flash/ROM = 0xFF по умолчанию
            } else {
                std::memset(region.data.get(), 0x00, region.size); // RAM = 0
            }
        }
    }
}

void MemoryMap::addMemoryRegion(uint32_t baseAddress, size_t size, MemoryType type) {
    m_memoryRegions.push_back({baseAddress, size, type, nullptr});
}

void MemoryMap::loadEspImage(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open image: " + path);
    }

    EspImageHeader hdr;
    file.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != 0xE9) {
        throw std::runtime_error("Invalid ESP image magic");
    }

    std::cout << "ESP Image: " << (int)hdr.segments << " segments, entry=0x"
              << std::hex << hdr.entry_addr << std::dec << "\n";

    // Загружаем каждый сегмент
    for (int i = 0; i < hdr.segments; ++i) {
        SegmentHeader seg;
        file.read(reinterpret_cast<char*>(&seg), sizeof(seg));
        if (seg.size == 0) continue;

        std::vector<uint8_t> data(seg.size);
        file.read(reinterpret_cast<char*>(data.data()), seg.size);

        std::cout << "  Segment " << i << ": "
                  << "load_addr=0x" << std::hex << seg.load_addr
                  << ", size=" << std::dec << seg.size << " bytes\n";

        // Пишем данные по целевому адресу в память
        for (size_t j = 0; j < seg.size; ++j) {
            writeData(seg.load_addr + j, data[j]);
        }
    }

    // Сохраняем точку входа для EmulatorCore
    m_entryPoint = hdr.entry_addr;
}

void MemoryMap::loadFirmware(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open firmware file: " + filePath);
    }

    std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        throw std::runtime_error("Empty firmware file");
    }

    file.seekg(0, std::ios::beg);

    // Загружаем в IROM-регион (0x400D0000), как это делает второй загрузчик
    MemoryRegion* iromRegion = nullptr;
    for (auto& region : m_memoryRegions) {
        if (region.type == MemoryType::Flash && region.baseAddress == 0x400D0000) {
            iromRegion = &region;
            break;
        }
    }

    if (!iromRegion) {
        throw std::runtime_error("IROM Flash region not found in memory map");
    }

    if (static_cast<size_t>(fileSize) > iromRegion->size) {
        throw std::runtime_error("Firmware too large for IROM region");
    }

    file.read(reinterpret_cast<char*>(iromRegion->data.get()), fileSize);
    std::cout << "Loaded " << fileSize << " bytes into IROM (0x400D0000)\n";
}

void MemoryMap::registerPeripheralHandler(uint32_t baseAddress, size_t size, PeripheralCallback callback) {
    m_peripheralHandlers.push_back({baseAddress, size, callback});
}

// --- Harvard-архитектура ---

uint32_t MemoryMap::readInstruction(uint32_t address) {
    auto* region = findMemoryRegion(address);
    if (!region) {
        throw std::runtime_error("Instruction fetch from unmapped address: 0x" + std::to_string(address));
    }

    // Только эти типы могут содержать исполняемый код
    if (region->type != MemoryType::IRAM &&
        region->type != MemoryType::ROM &&
        region->type != MemoryType::Flash) {
        throw std::runtime_error("Illegal instruction fetch from non-code region: 0x" + std::to_string(address));
    }

    if (region->type == MemoryType::Peripheral) {
        return 0; // Не должно происходить
    }

    size_t offset = address - region->baseAddress;
    if (offset + sizeof(uint32_t) > region->size) {
        throw std::runtime_error("Instruction fetch out of bounds");
    }

    uint32_t value;
    std::memcpy(&value, region->data.get() + offset, sizeof(value));
    return value;
}

uint32_t MemoryMap::readData(uint32_t address) {
    auto* region = findMemoryRegion(address);
    if (!region) {
        return 0; // Чтение из "дыры" → 0 (как в реальном железе)
    }

    if (region->type == MemoryType::Peripheral) {
        auto* handler = findPeripheralHandler(address);
        if (handler) {
            return handler->callback(nullptr, address, 0, false);
        }
        return 0;
    }

    size_t offset = address - region->baseAddress;
    if (offset + sizeof(uint32_t) > region->size) {
        return 0;
    }

    uint32_t value;
    std::memcpy(&value, region->data.get() + offset, sizeof(value));
    return value;
}

void MemoryMap::writeData(uint32_t address, uint32_t value) {
    auto* region = findMemoryRegion(address);
    if (!region) {
        return; // Игнорируем запись в неотображённые адреса
    }

    if (region->type == MemoryType::Peripheral) {
        auto* handler = findPeripheralHandler(address);
        if (handler) {
            handler->callback(nullptr, address, value, true);
        }
        return;
    }

    // Защита записи: ROM и Flash только для чтения
    if (region->type == MemoryType::ROM || region->type == MemoryType::Flash) {
        return; // В реальности — игнорируется или вызывает исключение
    }

    size_t offset = address - region->baseAddress;
    if (offset + sizeof(uint32_t) <= region->size) {
        std::memcpy(region->data.get() + offset, &value, sizeof(value));
    }
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