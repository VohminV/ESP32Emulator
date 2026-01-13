#include "xtensacpu.h"
#include "memorymap.h" // Ensure MemoryMap is defined
#include <cstring> // for memset
#include <stdexcept>

XtensaCPU::XtensaCPU(MemoryMap* memoryMap, bool isProCpu)
    : m_memoryMap(memoryMap), m_isProCpu(isProCpu) {
    if (!m_memoryMap) {
        throw std::invalid_argument("MemoryMap cannot be null");
    }
    reset();
}

void XtensaCPU::reset() {
    // Clear all general-purpose registers
    std::memset(m_generalRegisters, 0, sizeof(m_generalRegisters));
    
    // Reset PC to the standard entry point
    m_pc = 0x400D0000;

    // Initialize key special registers
    m_specialRegisters[PS] = 0x0000000F; // Kernel mode, interrupts masked
    m_specialRegisters[EPC1] = 0;
    m_specialRegisters[EXCCAUSE] = 0;
    m_specialRegisters[INTENABLE] = 0;

    // Clear pending interrupts
    m_pendingInterrupt = 0;
}

void XtensaCPU::executeCycle() {
    // Check for pending interrupts first
    if (m_pendingInterrupt != 0) {
        handleInterrupt();
        return;
    }

    // Simple fetch-execute cycle
    try {
        uint32_t instruction = fetchInstruction();
        
        // For this minimal version, we do nothing with the instruction.
        // In a real implementation, you would decode and execute it here.
        // This is just a placeholder to show the cycle structure.

        // Move PC to the next instruction (assuming 32-bit for simplicity)
        m_pc += 4;
    } catch (const std::exception& e) {
        // In a full implementation, this would trigger an exception.
        // For now, we just re-throw for the EmulatorCore to handle.
        throw;
    }
}

uint32_t XtensaCPU::fetchInstruction() {
    if (!m_memoryMap) {
        throw std::runtime_error("MemoryMap is not initialized");
    }
    return m_memoryMap->read(this, m_pc);
}

void XtensaCPU::requestInterrupt(int irqNumber) {
    // Set the corresponding bit in the pending interrupt register
    m_pendingInterrupt |= (1U << irqNumber);
}

void XtensaCPU::handleInterrupt() {
    // Save current PC to EPC1
    m_specialRegisters[EPC1] = m_pc;
    
    // Set a dummy exception cause (e.g., timer interrupt)
    m_specialRegisters[EXCCAUSE] = 6; 
    
    // Disable interrupts in PS (clear IE bit - bit 4 in this simplified model)
    m_specialRegisters[PS] &= ~(1U << 4);
    
    // Jump to a fixed interrupt vector address
    m_pc = 0x40000400; // Standard ESP32 interrupt vector in ROM
    
    // Clear the pending interrupt flag
    m_pendingInterrupt = 0;
}