// uartperipheral.h
#ifndef UARTPERIPHERAL_H
#define UARTPERIPHERAL_H

#include "peripheralcomponent.h"
#include <iostream>

class UartPeripheral : public PeripheralComponent {
public:
    // UART0 в ESP32: базовый адрес 0x3FF40000, размер 0x100, прерывание №14
    explicit UartPeripheral()
        : PeripheralComponent(0x3FF40000, 0x100, 14) {}

    uint32_t read(uint32_t addr) override;
    void write(uint32_t addr, uint32_t value) override;
    void onTick(uint64_t currentTick) override;
};

#endif // UARTPERIPHERAL_H