#ifndef XTENSACPU_H
#define XTENSACPU_H

#include <cstdint>
#include <cstddef> // for size_t

// Forward declaration to avoid circular dependency
class MemoryMap;

/**
 * @brief Minimal Xtensa CPU emulator core.
 * Implements the essential state and a basic execution cycle.
 */
class XtensaCPU {
public:
    // Constants for special registers (simplified)
    enum SpecialRegister {
        PC = 0,
        PS = 1,
        SAR = 2,
        EXCCAUSE = 3,
        EPC1 = 4,
        INTENABLE = 5,
        MAX_SPECIAL_REGS = 6
    };

    /**
     * @brief Constructor.
     * @param memoryMap Pointer to the system's memory map.
     * @param isProCpu Flag to identify PRO_CPU (true) or APP_CPU (false).
     */
    XtensaCPU(MemoryMap* memoryMap, bool isProCpu);

    /**
     * @brief Resets the CPU to its initial state.
     */
    void reset();

    /**
     * @brief Executes a single CPU cycle.
     * This is the main entry point called by EmulatorCore::tick().
     */
    void executeCycle();

    /**
     * @brief Requests an interrupt.
     * @param irqNumber The interrupt number to request.
     */
    void requestInterrupt(int irqNumber);

    // --- Getters/Setters for EmulatorCore integration ---
    uint32_t getPC() const { return m_pc; }
    void setPC(uint32_t pc) { m_pc = pc; }

private:
    MemoryMap* m_memoryMap;
    const bool m_isProCpu;

    // General Purpose Registers A0-A15
    static constexpr size_t NUM_GPR = 16;
    uint32_t m_generalRegisters[NUM_GPR] = {};

    // Program Counter
    uint32_t m_pc = 0x400D0000; // Entry point after 2nd stage bootloader

    // Special Registers (simplified set)
    uint32_t m_specialRegisters[MAX_SPECIAL_REGS] = {};

    // Interrupt state
    uint32_t m_pendingInterrupt = 0;

    // Private helper methods
    uint32_t fetchInstruction();
    void handleInterrupt();
};

#endif // XTENSACPU_H