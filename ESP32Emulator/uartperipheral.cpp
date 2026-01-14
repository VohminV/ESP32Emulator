// uartperipheral.cpp
#include "uartperipheral.h"

uint32_t UartPeripheral::read(uint32_t addr) {
    // Минимальная эмуляция: всегда возвращаем 0 (FIFO пуст)
    return 0;
}

void UartPeripheral::write(uint32_t addr, uint32_t value) {
    // Регистр UART_FIFO_REG находится по смещению 0x00
    if ((addr & 0xFF) == 0x00) {
        char c = static_cast<char>(value & 0xFF);
        std::cout << c << std::flush;

        // Опционально: генерируем прерывание по завершению передачи
        // requestInterrupt(); // раскомментировать, если нужно
    }
}

void UartPeripheral::onTick(uint64_t /*currentTick*/) {
    // UART не требует активной обработки на каждый такт
}