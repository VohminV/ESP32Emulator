// xtensacpu.cpp
#include "xtensacpu.h"
#include "memorymap.h"
#include <stdexcept>
#include <iostream>
#include <cstring>

// === Глобальные константы (только для этого .cpp файла) ===
namespace {
    constexpr uint32_t EXC_ILLEGAL           = 2;
    constexpr uint32_t EXC_INSTR_ERROR       = 3;  // Невалидный адрес инструкции
    constexpr uint32_t EXC_LOAD_STORE_ERROR  = 4;  // Ошибка доступа к данным
    constexpr uint32_t EXC_LEVEL1_INTERRUPT  = 6;  // Прерывание уровня 1

    // Уровни приоритетов IRQ в ESP32 (аппаратно фиксированы)
    constexpr uint8_t IRQ_PRIORITY[32] = {
        1, 1, 1, 1, 1, 1, 1, 1, // GPIO0–7
        3, 3, 3, 3,             // Timer0–3
        5, 5,                   // UART0, UART1
        7,                      // SPI0
        9,                      // I2C
        11,                     // WiFi
        13,                     // BT
        15, 15, 15, 15, 15, 15, 15, 15 // остальные — высший приоритет
    };
}

XtensaCPU::XtensaCPU(MemoryMap* memoryMap, bool isProCpu)
    : m_memoryMap(memoryMap)
    , m_isProCpu(isProCpu)
{
    reset();
}

void XtensaCPU::reset() {
    std::memset(m_gpr, 0, sizeof(m_gpr));
    std::memset(m_specialRegs, 0, sizeof(m_specialRegs));

    auto& p = ps();
    p.intlevel   = 0;
    p.excm       = 0;
    p.um         = 0;
    p.ring       = 0;
    p.owb        = 0;
    p.callinc    = 0;
    p.intenable  = 1;

    m_pendingIrq = 0;
    m_exceptionPending = false;
    m_pc = m_isProCpu ? 0x40000400 : 0x00000000;
}

void XtensaCPU::executeCycle() {
    if (!m_isProCpu && m_pc == 0) {
        return; // APP_CPU не запущен
    }

    if (m_exceptionPending) {
        handlePendingException();
        return;
    }

    if (ps().intenable && !ps().excm) {
        uint32_t enabledIrqs = m_pendingIrq & m_specialRegs[INTENABLE];
        if (enabledIrqs != 0) {
            int best_irq = -1;
            int best_prio = -1;
            for (int i = 0; i < 32; ++i) {
                if ((enabledIrqs >> i) & 1) {
                    int prio = IRQ_PRIORITY[i];
                    if (prio > static_cast<int>(ps().intlevel) && prio > best_prio) {
                        best_prio = prio;
                        best_irq = i;
                    }
                }
            }
            if (best_irq >= 0) {
                enterExceptionMode(EXC_LEVEL1_INTERRUPT, 0);
                m_pendingIrq &= ~(1U << best_irq);
                return;
            }
        }
    }

    uint32_t instr;
    try {
        instr = fetchInstruction();
    } catch (...) {
        return;
    }
    decodeAndExecute(instr);
}

uint32_t XtensaCPU::fetchInstruction() {
    uint32_t value;
    try {
        value = m_memoryMap->readInstruction(m_pc);
    } catch (const std::exception&) {
        m_specialRegs[EXCCAUSE] = EXC_INSTR_ERROR;
        m_exceptionPending = true;
        throw;
    }
    return value;
}

void XtensaCPU::decodeAndExecute(uint32_t instr) {
    bool is_16bit = (instr & 0xFFFF0000) == 0;

    if (is_16bit) {
        uint32_t opcode = instr & 0xF;
        uint32_t rt = (instr >> 12) & 0xF;
        switch (opcode) {
            case 0x2: { // l32r aX, imm20
                uint32_t imm20 = (instr >> 4) & 0xFFFFF;
                int32_t sign_extended = static_cast<int32_t>(imm20 << 12) >> 12;
                m_gpr[rt] = sign_extended;
                m_pc += 2;
                return;
            }
            case 0x5: // nop.n
                m_pc += 2;
                return;
            default:
                m_specialRegs[EXCCAUSE] = EXC_ILLEGAL;
                m_exceptionPending = true;
                return;
        }
    }

    uint32_t op0 = (instr >> 24) & 0xFF;
    uint32_t rs = (instr >> 8) & 0xF;
    uint32_t rt = (instr >> 4) & 0xF;
    uint32_t rd = instr & 0xF;

    if (op0 == 0x00) {
        m_specialRegs[EXCSAVE1] = m_pc + 3;
        m_pc = m_gpr[rs];
        return;
    } else if (op0 == 0x01) {
        m_pc = m_specialRegs[EXCSAVE1];
        return;
    } else if (op0 == 0x02) {
        m_gpr[rt] = ps().intlevel;
        ps().intlevel = rs & 0xF;
        m_pc += 3;
        return;
    } else if (op0 == 0x03) {
        ps().intlevel = rs & 0xF;
        m_pc += 3;
        return;
    } else if ((instr & 0xFFF00000) == 0x00400000) {
        m_gpr[rt] = m_gpr[rs];
        m_pc += 3;
        return;
    } else if ((instr & 0xFFF00000) == 0x00500000) {
        m_gpr[rd] = m_gpr[rs] + m_gpr[rt];
        m_pc += 3;
        return;
    }

    if ((instr & 0xF0000000) == 0xC0000000) {
        m_pc += 3;
        return;
    }

    m_specialRegs[EXCCAUSE] = EXC_ILLEGAL;
    m_exceptionPending = true;
}

void XtensaCPU::requestInterrupt(int irqNumber) {
    if (irqNumber >= 0 && irqNumber < 32) {
        m_pendingIrq |= (1U << irqNumber);
    }
}

void XtensaCPU::handlePendingException() {
    uint32_t cause = m_specialRegs[EXCCAUSE];
    enterExceptionMode(cause, 0);
    m_exceptionPending = false;
}

void XtensaCPU::enterExceptionMode(uint32_t cause, uint32_t /*vectorBase*/) {
    m_specialRegs[EPC1] = m_pc;
    m_specialRegs[EXCSAVE1] = ps().raw();

    ps().excm = 1;
    ps().intlevel = 15;
    ps().intenable = 0;

    if (cause == EXC_LEVEL1_INTERRUPT) {
        m_pc = 0x40000700;
    } else {
        m_pc = 0x40000600;
    }
}