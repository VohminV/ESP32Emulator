#ifndef XTENSACPU_H
#define XTENSACPU_H

#include <cstdint>
#include <array>
#include <functional>

class EmulatorCore;

struct CacheLine {
    uint32_t tag = 0;
    std::array<uint8_t, 32> data{}; // 32 байта на линию
    bool valid = false;
    bool dirty = false; // Только для D-Cache
};

class XtensaCPU
{
public:
    explicit XtensaCPU(int core_id); // 0 = PRO_CPU, 1 = APP_CPU
    ~XtensaCPU() = default;

    // Выполняет ОДНУ стадию конвейера (один такт CPU)
    void executeCycle();

    // Управление состоянием
    void setPC(uint32_t pc);
    uint32_t getPC() const { return m_pc; }

    // Обработка прерываний (вызывается из EmulatorCore::dispatchPendingInterrupts)
    void requestInterrupt(int irq_number);
    bool hasPendingInterrupt() const { return m_pendingInterrupt >= 0; }

    // Чтение/запись регистров (для MemoryMap и отладки)
    uint32_t readRegister(int reg_num) const;
    void writeRegister(int reg_num, uint32_t value);

    // Интеграция с эмулятором
    void setEmulatorCore(EmulatorCore* core) { m_emulatorCore = core; }

private:
    EmulatorCore* m_emulatorCore = nullptr;
    const int m_coreId;

    // Регистры общего назначения A0–A15
    static constexpr size_t NUM_GENERAL_REGS = 16;
    std::array<uint32_t, NUM_GENERAL_REGS> m_regs{};

    // Специальные регистры (минимум)
    uint32_t m_pc = 0x40000000; // Вектор сброса по умолчанию
    uint32_t m_ps = 0;          // Program Status (упрощённо)

    // Прерывания
    bool m_interruptsEnabled = true;
    int m_pendingInterrupt = -1; // Номер ожидающего IRQ

    // === Кэш-подсистема (структура готова к реализации) ===
    struct InstructionCache {
        static constexpr size_t NUM_LINES = 512;
        std::array<CacheLine, NUM_LINES> lines;
    };

    struct DataCache {
        static constexpr size_t NUM_LINES = 512;
        std::array<CacheLine, NUM_LINES> lines;
    };

    InstructionCache m_icache;
    DataCache m_dcache;
};

#endif // XTENSACPU_H