#include "xtensacpu.h"
#include "MemoryMap.h" // Предполагается, что MemoryMap.h существует

// Коды исключений (EXCCAUSE)
constexpr uint32_t EXC_ILLEGAL = 2;
constexpr uint32_t EXC_INSTR_ERROR = 3;
constexpr uint32_t EXC_LOAD_STORE_ERROR = 4;
constexpr uint32_t EXC_LEVEL1_INTERRUPT = 6;

XtensaCPU::XtensaCPU(MemoryMap* memoryMap, bool isProCpu)
    : m_memoryMap(memoryMap), m_isProCpu(isProCpu) {
    reset();
}

void XtensaCPU::reset() {
    // Инициализация GPR нулями (A0=0 по соглашению ABI)
    for (auto& reg : m_gpr) reg = 0;
    
    // Начальный PC зависит от ядра
    m_pc = m_isProCpu ? 0x400D0000 : 0x400D0004;
    
    // Сброс специальных регистров
    for (auto& reg : m_specialRegs) reg = 0;
    
    // Настройка PS: разрешить прерывания, уровень 0, режим ядра
    m_ps.intlevel = 0;
    m_ps.excm = 0;
    m_ps.um = 0;
    m_ps.ring = 0;
    m_ps.intenable = 1;
    
    m_pendingIrq = 0;
    m_exceptionPending = false;
}

void XtensaCPU::executeCycle() {
    if (m_exceptionPending) {
        handlePendingException();
        return;
    }

    const uint32_t instr = fetchInstruction();
    decodeAndExecute(instr);
}

uint32_t XtensaCPU::fetchInstruction() {
    // Harvard-архитектура: выборка только из пространства инструкций
    try {
        return m_memoryMap->readInstruction(m_pc);
    } catch (...) {
        // Ошибка доступа к памяти → исключение
        m_specialRegs[EXCCAUSE] = EXC_INSTR_ERROR;
        m_exceptionPending = true;
        return 0;
    }
}

void XtensaCPU::decodeAndExecute(uint32_t instr) {
    const uint32_t opcode = instr & 0xF; // Младшие 4 бита для 16-битных инстр.
    
    // Обработка 16-битных инструкций
    if ((instr & 0xFFFF0000) == 0) {
        const uint32_t imm = (instr >> 4) & 0xFFFFF; // 20-битный литерал
        const uint32_t rt = (instr >> 12) & 0xF;     // Целевой регистр
        
        switch (opcode) {
            case 0x2: // l32r (Load 32-bit literal)
                m_gpr[rt] = static_cast<int32_t>(imm << 12) >> 12; // Sign-extend 20→32
                m_pc += 2;
                return;
                
            case 0x5: // nop
                m_pc += 2;
                return;
        }
    }
    
    // Обработка 32-битных инструкций
    const uint32_t op2 = (instr >> 12) & 0xF;
    const uint32_t rs = (instr >> 8) & 0xF;
    const uint32_t rt = (instr >> 4) & 0xF;
    const uint32_t rd = instr & 0xF;
    
    switch (op2) {
        case 0x0: // callx0
            m_specialRegs[EXCSAVE1] = m_pc + 3; // Адрес возврата
            m_pc = m_gpr[rs];
            return;
            
        case 0x1: // retw.n (Return from windowed call)
            m_pc = m_specialRegs[EXCSAVE1];
            return;
            
        case 0x2: // rsil (Rotate and Set Interrupt Level)
            m_gpr[rt] = m_ps.intlevel;
            m_ps.intlevel = rs; // rs содержит новый уровень
            m_pc += 3;
            return;
            
        case 0x3: // waiti (Wait for Interrupt)
            // Эмуляция: просто ждём до следующего прерывания
            m_pc += 3;
            return;
            
        default:
            // Неизвестная инструкция → исключение
            m_specialRegs[EXCCAUSE] = EXC_ILLEGAL;
            m_exceptionPending = true;
            return;
    }
}

void XtensaCPU::requestInterrupt(int irqNumber) {
    if (irqNumber < 0 || irqNumber >= 32) return;
    
    const uint32_t irqMask = 1U << irqNumber;
    
    // Проверка глобального разрешения и маски INTENABLE
    if (m_ps.intenable && (m_specialRegs[INTENABLE] & irqMask)) {
        m_pendingIrq |= irqMask;
    }
}

void XtensaCPU::handlePendingException() {
    // Определение причины исключения
    uint32_t cause = m_specialRegs[EXCCAUSE];
    
    // Для прерываний: проверяем ожидающие IRQ
    if (cause == 0 && m_pendingIrq) {
        // Находим IRQ с highest priority (старший бит)
        const int irq = 31 - __builtin_clz(m_pendingIrq);
        cause = EXC_LEVEL1_INTERRUPT;
        m_pendingIrq &= ~(1U << irq); // Сбрасываем флаг
    }
    
    // Вход в режим исключения
    enterExceptionMode(cause, 0x40000000); // База векторов для ESP32
    m_exceptionPending = false;
}

void XtensaCPU::enterExceptionMode(uint32_t cause, uint32_t vectorBase) {
    // Сохранение контекста
    m_specialRegs[EPC1] = m_pc;          // Адрес ошибочной инструкции
    m_specialRegs[EXCSAVE1] = m_ps.raw(); // Сохранение PS
    
    // Настройка нового состояния
    m_ps.excm = 1;                       // Режим исключения
    m_ps.intlevel = 15;                  // Маскируем все прерывания
    m_ps.intenable = 0;
    
    // Загрузка вектора прерывания
    m_pc = getVectorAddress(cause);
}

uint32_t XtensaCPU::getVectorAddress(uint32_t cause) const {
    // ESP32 использует фиксированные векторы:
    // 0x40000000 + (cause * 0x100)
    return 0x40000000 + (cause << 8);
}