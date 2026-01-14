// dportperipheral.h
#ifndef DPORTPERIPHERAL_H
#define DPORTPERIPHERAL_H

#include "peripheralcomponent.h"

class EmulatorCore;

class DportPeripheral : public PeripheralComponent {
public:
    explicit DportPeripheral(EmulatorCore* core, uint32_t baseAddr = 0x3FF00000);
    ~DportPeripheral() override = default;

    uint32_t read(uint32_t addr) override;
    void write(uint32_t addr, uint32_t value) override;
    void onTick(uint64_t globalTick) override {}

private:
    EmulatorCore* m_core;
};

#endif // DPORTPERIPHERAL_H