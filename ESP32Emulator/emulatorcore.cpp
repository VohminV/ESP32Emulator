#include "emulatorcore.h"
#include "peripheralcomponent.h" // Assuming you have this header
#include <iostream>
#include <cassert>
#include <fstream>

// Константы ESP32
constexpr uint64_t APB_CLK_HZ = 80'000'000ULL; // Тактовая частота шины APB
constexpr uint64_t TICKS_PER_US = APB_CLK_HZ / 1'000'000ULL; // 80 тактов на микросекунду

EmulatorCore::EmulatorCore()
    // Передаем указатель на m_memoryMap в конструкторы CPU
    : m_cpu0(std::make_unique<XtensaCPU>(m_memoryMap.get(), true))  // PRO_CPU
    , m_cpu1(std::make_unique<XtensaCPU>(m_memoryMap.get(), false)) // APP_CPU
    , m_memoryMap(std::make_unique<MemoryMap>())
{
    // MemoryMap уже инициализируется в своем конструкторе
}

EmulatorCore::~EmulatorCore() {
    stop();
}

bool EmulatorCore::loadFirmware(const std::string& path) {
    try {
        m_memoryMap->loadFirmware(path);
        
        // Для простоты установим PC обоих ядер на стандартную точку входа.
        // В реальности загрузчик из ROM должен это сделать.
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
    // Регистрация обработчика в MemoryMap через callback
    auto baseAddr = peripheral->getBaseAddress();
    auto size = peripheral->getSize(); // Предполагается, что PeripheralComponent знает свой размер

    m_memoryMap->registerPeripheralHandler(baseAddr, size,
        [this, periphPtr = peripheral.get()](XtensaCPU* cpu, uint32_t addr, uint32_t val, bool isWrite) -> uint32_t {
            if (isWrite) {
                periphPtr->write(cpu, addr, val);
                return 0; // Значение игнорируется при записи
            } else {
                return periphPtr->read(cpu, addr);
            }
        }
    );

    m_peripherals.push_back(std::move(peripheral));
}

uint32_t EmulatorCore::readMemory(uint32_t address) {
    // Для внешнего API используем CPU0 как контекст
    return m_memoryMap->read(m_cpu0.get(), address);
}

void EmulatorCore::writeMemory(uint32_t address, uint32_t value) {
    // Для внешнего API используем CPU0 как контекст
    m_memoryMap->write(m_cpu0.get(), address, value);
}

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

    // 2. Обновление состояния всех периферийных устройств
    for (auto& p : m_peripherals) {
        p->onTick(currentTick);
    }

    // 3. Выполнение одного такта CPU
    // Теперь CPU сами вызывают MemoryMap с передачей своего указателя
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
    // Проверяем флаг, установленный методом requestInterrupt
    int pendingIrq = m_pendingInterrupt.load();
    if (pendingIrq >= 0) {
        m_cpu0->requestInterrupt(pendingIrq); // PRO_CPU обрабатывает прерывания
        m_pendingInterrupt.store(-1); // Сброс флага
    }
}

// Этот метод вызывается из периферийных компонентов
void EmulatorCore::requestInterrupt(int irqNumber) {
    m_pendingInterrupt.store(irqNumber);
}

void EmulatorCore::scheduleEvent(uint64_t tick, std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_eventQueue.push({tick, std::move(callback)});
}

void EmulatorCore::scheduleEventRelative(uint64_t ticksFromNow, std::function<void()> callback) {
    uint64_t currentTick = m_globalTick.load();
    scheduleEvent(currentTick + ticksFromNow, std::move(callback));
}

bool EmulatorCore::isRunning() const {
    return m_running.load();
}