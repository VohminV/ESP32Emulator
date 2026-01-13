#ifndef XTENSACPU_H
#define XTENSACPU_H

#include <cstdint>
#include <cstddef>

class MemoryMap;

/**
 * @brief Полнофункциональный эмулятор ядра Xtensa LX6/LX7 для ESP32.
 * Поддерживает Harvard-архитектуру, прерывания до 15 уровня, DSP-расширения и работу двух ядер.
 */
class XtensaCPU {
public:
    // Специальные регистры Xtensa (ESP32-специфичные)
    enum SpecialRegister : uint8_t {
        PC = 0,
        PS = 1,
        SAR = 2,
        EXCCAUSE = 3,
        EPC1 = 4,
        EXCSAVE1 = 5,
        INTENABLE = 6,
        INTERRUPT = 7,
        INTSET = 8,
        INTCLEAR = 9,
        MAX_SPECIAL_REGS = 10
    };

    /**
     * @brief Конструктор.
     * @param memoryMap Указатель на модель памяти системы.
     * @param isProCpu true для PRO_CPU (ядро 0), false для APP_CPU (ядро 1).
     */
    explicit XtensaCPU(MemoryMap* memoryMap, bool isProCpu);

    /**
     * @brief Сброс CPU в начальное состояние (как после POR).
     */
    void reset();

    /**
     * @brief Выполнение одного цикла CPU: fetch → decode → execute → handle exceptions.
     */
    void executeCycle();

    /**
     * @brief Запрос прерывания от периферии.
     * @param irqNumber Номер IRQ (0–31).
     */
    void requestInterrupt(int irqNumber);

    // --- Геттеры/сеттеры для интеграции ---
    uint32_t getPC() const { return m_pc; }
    void setPC(uint32_t pc) { m_pc = pc; }

private:
    // Регистр состояния процессора (PS) — битовые поля
    struct ProcessorStatus {
        uint32_t intlevel : 4;   // Текущий уровень маскирования прерываний (0–15)
        uint32_t excm : 1;       // Режим исключения (1 = активен)
        uint32_t reserved1 : 1;
        uint32_t um : 1;         // User Mode (всегда 0 для ESP32)
        uint32_t reserved2 : 1;
        uint32_t ring : 2;       // Уровень привилегий (0 = kernel)
        uint32_t owb : 4;        // Window Base (для register windows)
        uint32_t callinc : 2;    // Call Increment (для вызовов функций)
        uint32_t reserved3 : 14;
        uint32_t intenable : 1;  // Глобальное разрешение прерываний

        // Удобные методы доступа
        uint32_t raw() const { return *reinterpret_cast<const uint32_t*>(this); }
        void setRaw(uint32_t value) { *reinterpret_cast<uint32_t*>(this) = value; }
    };

    MemoryMap* m_memoryMap;
    const bool m_isProCpu;

    // Регистры общего назначения A0–A15
    static constexpr size_t NUM_GPR = 16;
    uint32_t m_gpr[NUM_GPR] = {};

    // Счётчик команд
    uint32_t m_pc = 0x400D0000; // Точка входа после загрузчика

    // Специальные регистры
    uint32_t m_specialRegs[MAX_SPECIAL_REGS] = {};
    ProcessorStatus& m_ps = reinterpret_cast<ProcessorStatus&>(m_specialRegs[PS]);

    // Внутреннее состояние прерываний
    uint32_t m_pendingIrq = 0;   // Битовая маска ожидающих IRQ
    bool m_exceptionPending = false; // Флаг ожидающего исключения

    // Вспомогательные методы
    uint32_t fetchInstruction();
    void decodeAndExecute(uint32_t instruction);
    void handlePendingException();
    void enterExceptionMode(uint32_t cause, uint32_t vectorBase);
    uint32_t getVectorAddress(uint32_t cause) const;
};

#endif // XTENSACPU_H