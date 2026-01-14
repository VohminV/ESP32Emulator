// dportperipheral.cpp
#include "dportperipheral.h"
#include "emulatorcore.h"

DportPeripheral::DportPeripheral(EmulatorCore* core, uint32_t baseAddr)
    : PeripheralComponent(baseAddr, 0x1000, -1)
    , m_core(core)
{
}

uint32_t DportPeripheral::read(uint32_t addr) {
    // Минимальная поддержка чтения
    return 0;
}

void DportPeripheral::write(uint32_t addr, uint32_t value) {
    uint32_t reg = addr & 0xFFF;

    // DPORT_APPCPU_CTRL_A (0x004): бит 0 — reset, бит 1 — clk_en
    if (reg == 0x004) {
        if (value & 0x02) { // clk_en = 1 → запуск APP_CPU
            m_core->startAppCpu();
        }
    }

    // DPORT_CPU_INTR_FROM_CPU_0 (0x0B0): программное прерывание на другое ядро
    if (reg == 0x0B0) {
        // В ESP32 это вызывает software interrupt (обычно IRQ 26)
        m_core->requestInterrupt(26);
    }
}