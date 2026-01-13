#include "firmwareloader.h"
#include "memorymap.h"

bool FirmwareLoader::load(const std::string& path, MemoryMap& memoryMap) {
    m_entryPoint = 0x40000000; // Заглушка
    return true; // Всегда успешно
}

bool FirmwareLoader::loadElf(const std::string&, MemoryMap&) { return false; }
bool FirmwareLoader::loadBin(const std::string&, MemoryMap&) { return false; }