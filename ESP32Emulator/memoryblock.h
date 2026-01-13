// memoryblock.h
#ifndef MEMORYBLOCK_H
#define MEMORYBLOCK_H

#include <cstdint>
#include <vector>

class MemoryBlock
{
public:
    explicit MemoryBlock(size_t size) : m_data(size, 0) {}
    ~MemoryBlock() = default;

    uint8_t& at(size_t index) { return m_data.at(index); }
    const uint8_t& at(size_t index) const { return m_data.at(index); }

private:
    std::vector<uint8_t> m_data;
};

#endif // MEMORYBLOCK_H