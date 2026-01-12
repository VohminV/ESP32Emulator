#ifndef EMULATORCORE_H
#define EMULATORCORE_H

#include "peripheralcomponent.h"
#include <vector>
#include <memory>

class EmulatorCore
{
public:
    EmulatorCore();
    ~EmulatorCore();

    void start();
    void stop();
    void step();

    void addPeripheral(std::unique_ptr<PeripheralComponent> peripheral);

    bool isRunning() const { return m_running; }

private:
    std::vector<std::unique_ptr<PeripheralComponent>> m_peripherals;
    bool m_running = false;
};

#endif // EMULATORCORE_H