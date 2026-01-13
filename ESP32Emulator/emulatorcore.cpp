#include "emulatorcore.h"
#include "peripheralcomponent.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>

EmulatorCore::EmulatorCore()
    : m_memoryMap(std::make_unique<MemoryMap>())
    , m_cpu0(std::make_unique<XtensaCPU>(m_memoryMap.get(), true))   // PRO_CPU
    , m_cpu1(std::make_unique<XtensaCPU>(m_memoryMap.get(), false))  // APP_CPU
{
    // MemoryMap уже инициализирован в своем конструкторе
}

EmulatorCore::~EmulatorCore() {
    stop();
}

bool EmulatorCore::loadFirmware(const std::string& path) {
    try {
        m_memoryMap->loadFirmware(path);
        // Для простоты: оба ядра начинают с точки входа приложения.
        // В продвинутой версии здесь должна быть эмуляция Boot ROM.
        constexpr uint32_t ENTRY_POINT = 0x400D0000;
        m_cpu0->setPC(ENTRY_POINT);
        m_cpu1->setPC(ENTRY_POINT);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Firmware load error: " << e.what() << std::endl;
        return false;
    }
}

void EmulatorCore::start() {
    if (m_running.exchange(true)) return;
    m_emulationThread = std::thread([this]() {
        while (m_running.load()) {
            tick();
        }
    });
}

void EmulatorCore::stop() {
    if (!m_running.exchange(false)) return;
    if (m_emulationThread.joinable()) {
        m_emulationThread.join();
    }
}

void EmulatorCore::step() {
    tick();
}

void EmulatorCore::addPeripheral(std::unique_ptr<PeripheralComponent> peripheral) {
    auto baseAddr = peripheral->getBaseAddress();
    auto size = peripheral->getSize();

    // Создаём интерфейс для этой периферии
    PeripheralComponent::SystemInterface sysInterface;
    sysInterface.requestInterrupt = [this](int irq) { this->requestInterrupt(irq); };
    sysInterface.scheduleEventRelative = [this](uint64_t ticks, auto cb) {
        this->scheduleEventRelative(ticks, std::move(cb));
    };
    sysInterface.getCurrentTick = [this]() { return this->m_globalTick.load(); };

    // Передаём интерфейс периферии
    peripheral->setSystemInterface(std::move(sysInterface));

    // Регистрируем callback в MemoryMap
    m_memoryMap->registerPeripheralHandler(baseAddr, size,
        [periphPtr = peripheral.get()](XtensaCPU* /*cpu*/, uint32_t addr, uint32_t val, bool isWrite) -> uint32_t {
            if (isWrite) {
                periphPtr->write(addr, val);
                return 0;
            } else {
                return periphPtr->read(addr);
            }
        }
    );

    m_peripherals.push_back(std::move(peripheral));
}

// --- Отладочные методы ---
uint32_t EmulatorCore::readMemory(uint32_t address) {
    return m_memoryMap->readData(address);
}

void EmulatorCore::writeMemory(uint32_t address, uint32_t value) {
    m_memoryMap->writeData(address, value);
}
// -------------------------

void EmulatorCore::tick() {
    const uint64_t currentTick = m_globalTick.fetch_add(1);

    // 1. Обработка запланированных событий
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        while (!m_eventQueue.empty() && m_eventQueue.top().tick <= currentTick) {
            auto event = m_eventQueue.top();
            m_eventQueue.pop();
            event.callback();
        }
    }

    // 2. Обновление состояния периферии
    for (auto& p : m_peripherals) {
        p->onTick(currentTick);
    }

    // 3. Выполнение одного цикла для каждого CPU
    // CPU сами заботятся о вызове правильных методов MemoryMap (fetch vs load/store)
    m_cpu0->executeCycle();
    m_cpu1->executeCycle();

    // 4. Диспетчеризация прерываний
    dispatchPendingInterrupts();
}

void EmulatorCore::executeForUs(uint64_t us) {
    const uint64_t targetTicks = m_globalTick.load() + us * TICKS_PER_US;
    while (m_globalTick.load() < targetTicks && m_running.load()) {
        tick();
    }
}

void EmulatorCore::dispatchPendingInterrupts() {
    const uint32_t pendingMask = m_pendingIrqMask.load();
    if (pendingMask != 0) {
        // В реальном ESP32 прерывания могут маршрутизироваться на любое ядро.
        // Для простоты отправим все на PRO_CPU (m_cpu0).
        m_cpu0->requestInterrupt(__builtin_ctz(pendingMask)); // Берем младший установленный бит
        // Сбрасываем флаг этого IRQ
        m_pendingIrqMask.fetch_and(~(1U << __builtin_ctz(pendingMask)));
    }
}

// Этот метод вызывается из периферийных компонентов
void EmulatorCore::requestInterrupt(int irqNumber) {
    if (irqNumber >= 0 && irqNumber < 32) {
        m_pendingIrqMask.fetch_or(1U << irqNumber);
    }
}

void EmulatorCore::scheduleEvent(uint64_t tick, std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_eventQueue.push({tick, std::move(callback)});
}

void EmulatorCore::scheduleEventRelative(uint64_t ticksFromNow, std::function<void()> callback) {
    scheduleEvent(m_globalTick.load() + ticksFromNow, std::move(callback));
}

bool EmulatorCore::isRunning() const {
    return m_running.load();
}