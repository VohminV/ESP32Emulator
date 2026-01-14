// frc1timer.h
#ifndef FRC1TIMER_H
#define FRC1TIMER_H

#include "peripheralcomponent.h"
#include <functional>

class EmulatorCore;

class Frc1Timer : public PeripheralComponent {
public:
    explicit Frc1Timer(EmulatorCore* core, uint32_t baseAddr = 0x3FF1F000);
    ~Frc1Timer() = default;

    uint32_t read(uint32_t addr) override;
    void write(uint32_t addr, uint32_t value) override;
    void onTick(uint64_t globalTick) override {}

private:
    EmulatorCore* m_core;
    uint32_t m_counter = 0;
    uint32_t m_reload = 0;
    bool m_enabled = false;
    static constexpr int TIMER_IRQ = 9; // FRC1 → interrupt 9
};

#endif // FRC1TIMER_H