// xtensacpu.h
#ifndef XTENSACPU_H
#define XTENSACPU_H

#include <cstdint>
#include <cstring>

class MemoryMap;

class XtensaCPU {
public:
    explicit XtensaCPU(MemoryMap* memoryMap, bool isProCpu);
    void reset();
    void executeCycle();
    void requestInterrupt(int irqNumber);

    uint32_t getPC() const { return m_pc; }
    void setPC(uint32_t pc) { m_pc = pc; }

    // Специальные регистры Xtensa (согласно ISA)
    static constexpr int PS        = 230;
    static constexpr int EXCCAUSE  = 232;
    static constexpr int EPC1      = 234;
    static constexpr int EXCSAVE1  = 235;
    static constexpr int INTENABLE = 236;

    uint32_t getSpecialReg(int reg) const { return m_specialRegs[reg]; }
    void setSpecialReg(int reg, uint32_t value) { m_specialRegs[reg] = value; }

private:
    struct ProcessorStatus {
        uint32_t raw() const { return *reinterpret_cast<const uint32_t*>(this); }
        uint32_t& raw() { return *reinterpret_cast<uint32_t*>(this); }

        uint32_t : 1;               // bit 0 — reserved
        uint32_t intlevel : 4;      // bits 1–4
        uint32_t : 2;               // bits 5–6 — reserved
        uint32_t excm : 1;          // bit 7
        uint32_t um : 1;            // bit 8
        uint32_t ring : 2;          // bits 9–10
        uint32_t owb : 4;           // bits 11–14
        uint32_t : 1;               // bit 15 — reserved
        uint32_t callinc : 2;       // bits 16–17
        uint32_t : 13;              // bits 18–30 — reserved
        uint32_t intenable : 1;     // bit 31
    };

    // Безопасный доступ к PS через reinterpret_cast к адресу
    ProcessorStatus& ps() {
        return *reinterpret_cast<ProcessorStatus*>(&m_specialRegs[PS]);
    }
    const ProcessorStatus& ps() const {
        return *reinterpret_cast<const ProcessorStatus*>(&m_specialRegs[PS]);
    }

    MemoryMap* m_memoryMap;
    bool m_isProCpu;
    uint32_t m_gpr[32] = {};
    uint32_t m_specialRegs[256] = {};
    uint32_t m_pc = 0;
    uint32_t m_pendingIrq = 0;
    bool m_exceptionPending = false;

    void handlePendingException();
    void enterExceptionMode(uint32_t cause, uint32_t vectorBase);
    uint32_t fetchInstruction();
    void decodeAndExecute(uint32_t instr);
};

#endif // XTENSACPU_H