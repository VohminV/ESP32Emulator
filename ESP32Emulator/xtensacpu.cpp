#include "xtensacpu.h"

XtensaCPU::XtensaCPU(int core_id) : m_coreId(core_id) {
    m_pc = 0x40000000; // Вектор сброса по умолчанию
}

void XtensaCPU::executeCycle() {
    // TODO: Реализовать один такт CPU
}

void XtensaCPU::setPC(uint32_t pc) {
    m_pc = pc;
}

uint32_t XtensaCPU::readRegister(int reg_num) const {
    if (reg_num >= 0 && reg_num < static_cast<int>(m_regs.size())) {
        return m_regs[reg_num];
    }
    return 0;
}

void XtensaCPU::writeRegister(int reg_num, uint32_t value) {
    if (reg_num >= 0 && reg_num < static_cast<int>(m_regs.size())) {
        m_regs[reg_num] = value;
    }
}

void XtensaCPU::requestInterrupt(int irq_number) {
    m_pendingInterrupt = irq_number;
}