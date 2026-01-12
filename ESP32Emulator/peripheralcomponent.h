#ifndef PERIPHERALCOMPONENT_H
#define PERIPHERALCOMPONENT_H

#include <cstdint>

class PeripheralComponent
{
public:
    virtual ~PeripheralComponent() = default;

    virtual void init() = 0;
    virtual void deinit() = 0;
    virtual void tick(uint32_t deltaTimeMs) = 0;
    virtual uint32_t readRegister(uint32_t addr) = 0;
    virtual void writeRegister(uint32_t addr, uint32_t value) = 0;
};

#endif // PERIPHERALCOMPONENT_H