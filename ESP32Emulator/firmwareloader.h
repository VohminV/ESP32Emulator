#ifndef FIRMWARELOADER_H
#define FIRMWARELOADER_H

#include <string>
#include <cstdint>
#include <memory>

// Предварительное объявление
class MemoryMap;

class FirmwareLoader
{
public:
    FirmwareLoader() = default;
    ~FirmwareLoader() = default;

    /**
     * @brief Загружает прошивку (ELF или raw BIN) в указанную карту памяти.
     *
     * Для ELF:
     * - Парсит секции .text, .rodata → загружает в IROM/DROM (Flash)
     * - Парсит секцию .data → загружает в DRAM
     * - Обнуляет .bss в DRAM
     * - Извлекает точку входа (entry point)
     *
     * Для BIN:
     * - Загружает весь образ по фиксированному адресу (например, 0x1000)
     *
     * @param path Путь к файлу прошивки (.elf или .bin)
     * @param memoryMap Ссылка на карту памяти для записи
     * @return true при успехе, false при ошибке
     */
    bool load(const std::string& path, MemoryMap& memoryMap);

    /**
     * @brief Возвращает адрес точки входа (PC после сброса).
     * Действителен только после успешного вызова load().
     */
    uint32_t getEntryPoint() const { return m_entryPoint; }

private:
    bool loadElf(const std::string& path, MemoryMap& memoryMap);
    bool loadBin(const std::string& path, MemoryMap& memoryMap);

    // Внутренние данные
    uint32_t m_entryPoint = 0x40000000; // Значение по умолчанию для ESP32 PRO_CPU
};

#endif // FIRMWARELOADER_H