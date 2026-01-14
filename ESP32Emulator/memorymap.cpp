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
    // === ШАГ 1: Загружаем ВЕСЬ файл в m_flashImage ===
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open image: " + path);
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_flashImage.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_flashImage.data()), size)) {
        throw std::runtime_error("Failed to read image data");
    }

    // === ШАГ 2: Парсим заголовок из m_flashImage ===
    if (size < sizeof(EspImageHeader)) {
        throw std::runtime_error("Image too small");
    }

    EspImageHeader* hdr = reinterpret_cast<EspImageHeader*>(m_flashImage.data());
    if (hdr->magic != 0xE9) {
        throw std::runtime_error("Invalid ESP image magic");
    }

    m_entryPoint = hdr->entry_addr;

    std::cout << "ESP Image: " << (int)hdr->segments << " segments, entry=0x"
              << std::hex << m_entryPoint << std::dec << "\n";

    // === ШАГ 3: Парсим сегменты и копируем их в RAM ===
    size_t offset = sizeof(EspImageHeader);
    for (int i = 0; i < hdr->segments; ++i) {
        if (offset + sizeof(SegmentHeader) > m_flashImage.size()) {
            throw std::runtime_error("Truncated image");
        }

        SegmentHeader* seg = reinterpret_cast<SegmentHeader*>(m_flashImage.data() + offset);
        offset += sizeof(SegmentHeader);

        if (seg->size == 0) continue;
        if (offset + seg->size > m_flashImage.size()) {
            throw std::runtime_error("Segment overflows image");
        }

        std::cout << "  Segment " << i << ": "
                  << "load_addr=0x" << std::hex << seg->load_addr
                  << ", size=" << std::dec << seg->size << " bytes\n";

        // Копируем данные сегмента в RAM (DRAM/DROM)
        for (size_t j = 0; j < seg->size; ++j) {
            writeData(seg->load_addr + j, m_flashImage[offset + j]);
        }

        offset += seg->size;
    }
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

uint32_t MemoryMap::readInstruction(uint32_t addr) {
    // IROM0: 0x400D0000–0x40400000 → отображается во flash (XIP)
    if (addr >= 0x400D0000 && addr < 0x40400000) {
        size_t offset = addr - 0x400D0000;
        if (offset + 3 < m_flashImage.size()) {
            // Little-endian: ESP32 — LE
            return (static_cast<uint32_t>(m_flashImage[offset + 3]) << 24) |
                   (static_cast<uint32_t>(m_flashImage[offset + 2]) << 16) |
                   (static_cast<uint32_t>(m_flashImage[offset + 1]) << 8)  |
                   (static_cast<uint32_t>(m_flashImage[offset + 0]));
        }
        throw std::runtime_error("Instruction fetch out of flash bounds");
    }

    // DROM0: 0x3F400000–0x3F800000 → тоже из flash, но как данные
    if (addr >= 0x3F400000 && addr < 0x3F800000) {
        size_t offset = addr - 0x3F400000;
        if (offset + 3 < m_flashImage.size()) {
            return (static_cast<uint32_t>(m_flashImage[offset + 3]) << 24) |
                   (static_cast<uint32_t>(m_flashImage[offset + 2]) << 16) |
                   (static_cast<uint32_t>(m_flashImage[offset + 1]) << 8)  |
                   (static_cast<uint32_t>(m_flashImage[offset + 0]));
        }
    }

    // ROM: 0x40000000–0x40070000
    if (addr >= 0x40000000 && addr < 0x40070000) {
        // TODO: добавить ROM-образ (минимум — заглушку)
        return 0x00000000; // или выбросить исключение
    }

    // По умолчанию — читаем из DRAM как данные
    return readData(addr);
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