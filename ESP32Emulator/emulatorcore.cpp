#include "emulatorcore.h"
#include <iostream>
#include <cassert>
#include <mutex>

// Константы ESP32
constexpr uint64_t APB_CLK_HZ = 80'000'000ULL; // Тактовая частота шины APB
constexpr uint64_t TICKS_PER_US = APB_CLK_HZ / 1'000'000ULL; // 80 тактов на микросекунду

EmulatorCore::EmulatorCore()
    : m_cpu0(std::make_unique<XtensaCPU>(0)) // PRO_CPU
    , m_cpu1(std::make_unique<XtensaCPU>(1)) // APP_CPU
    , m_memoryMap(std::make_unique<MemoryMap>())
{
    // Инициализация MemoryMap стандартной картой ESP32
    m_memoryMap->initializeDefaultMap();

    // Регистрация обработчиков для MemoryMap
    // Пример: регистрация UART0 по адресу 0x3FF40000
    // Это будет сделано в addPeripheral, но здесь можно зарегистрировать заглушки
}

EmulatorCore::~EmulatorCore()
{
    stop();
}

bool EmulatorCore::loadFirmware(const std::string& path)
{
    FirmwareLoader loader;
    if (!loader.load(path, *m_memoryMap)) {
        return false;
    }

    // Установка начального PC для обоих ядер (обычно из вектора сброса)
    m_cpu0->setPC(loader.getEntryPoint());
    m_cpu1->setPC(loader.getEntryPoint());

    return true;
}

void EmulatorCore::start()
{
    if (m_running.exchange(true)) return; // Уже запущен

    m_emulationThread = std::thread([this]() {
        while (m_running.load()) {
            tick();
        }
    });
}

void EmulatorCore::stop()
{
    if (!m_running.exchange(false)) return; // Уже остановлен

    if (m_emulationThread.joinable()) {
        m_emulationThread.join();
    }
}

void EmulatorCore::step()
{
    tick();
}

void EmulatorCore::addPeripheral(std::unique_ptr<PeripheralComponent> peripheral)
{
    // Передаём указатель на EmulatorCore для обратного вызова (например, запрос прерывания)
    peripheral->setEmulatorCore(this);

    // Регистрируем периферию в MemoryMap по её базовому адресу
    m_memoryMap->registerPeripheral(peripheral->getBaseAddress(), peripheral.get());

    m_peripherals.push_back(std::move(peripheral));
}

uint32_t EmulatorCore::readMemory(uint32_t address)
{
    return m_memoryMap->read(address);
}

void EmulatorCore::writeMemory(uint32_t address, uint32_t value)
{
    m_memoryMap->write(address, value);
}

void EmulatorCore::tick()
{
    const uint64_t currentTick = m_globalTick.fetch_add(1);

    // 1. Обработка запланированных событий (таймеры, UART-передача и т.д.)
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

    // 3. Выполнение одного такта CPU (одна стадия конвейера)
    m_cpu0->executeCycle();
    m_cpu1->executeCycle();

    // 4. Диспетчеризация прерываний
    dispatchPendingInterrupts();
}

void EmulatorCore::executeForUs(uint64_t us)
{
    const uint64_t targetTicks = m_globalTick.load() + us * TICKS_PER_US;
    while (m_globalTick.load() < targetTicks && m_running.load()) {
        tick();
    }
}

void EmulatorCore::emulationLoop()
{
    // Этот метод больше не используется напрямую,
    // так как start() вызывает tick() в цикле.
    // Логика полностью содержится в tick().
}

void EmulatorCore::dispatchPendingInterrupts()
{
    // В реальной реализации здесь будет проверка флагов прерываний
    // от периферии и их передача в CPU через m_cpu0->requestInterrupt(...)

    // Пример (заглушка):
    // for (auto& p : m_peripherals) {
    //     if (p->hasPendingInterrupt()) {
    //         int irq_num = p->getInterruptNumber();
    //         m_cpu0->requestInterrupt(irq_num);
    //     }
    // }
}

// Метод для планирования событий из периферии
void EmulatorCore::scheduleEvent(uint64_t tick, std::function<void()> callback)
{
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_eventQueue.push({tick, std::move(callback)});
}