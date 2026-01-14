#include "emulatorcore.h"
#include "peripheralcomponent.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>
#include <cstdlib>
#include <memory>
#include <array>
#include <filesystem>
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif



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
        // Загружаем образ — MemoryMap сам:
        // - читает весь .bin
        // - парсит заголовок
        // - копирует сегменты в RAM (включая .data)
        // - оставляет .bss нулевой (RAM изначально нулевая)
        m_memoryMap->loadEspImage(path);

        // Получаем точку входа (уже реализовано вами)
        m_firmwareEntryPoint = m_memoryMap->getEntryPoint();

        // Устанавливаем PC PRO_CPU
        m_cpu0->setPC(m_firmwareEntryPoint);
        // APP_CPU остаётся на 0 — запустится позже через DPORT

        std::cout << "Firmware loaded. Entry: 0x" << std::hex << m_firmwareEntryPoint << std::dec << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Firmware load error: " << e.what() << std::endl;
        return false;
    }
}

void EmulatorCore::startAppCpu() {
    if (m_cpu1->getPC() == 0) {
        // Точка входа APP_CPU — обычно та же, что и у PRO_CPU,
        // но в реальности она задаётся через ROM-код.
        // Для MVP используем ту же точку входа.
        m_cpu1->setPC(m_firmwareEntryPoint);
        std::cout << "APP_CPU started at 0x" << std::hex << m_firmwareEntryPoint << std::dec << "\n";
    }
}

// --- Вспомогательная функция запуска команды ---
bool EmulatorCore::runCommand(const std::string& cmd, const std::filesystem::path& cwd, std::string& output) {
    std::string fullCmd;
#ifdef _WIN32
    fullCmd = "cd /D \"" + cwd.string() + "\" && " + cmd + " 2>&1";
    FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
    fullCmd = "cd \"" + cwd.string() + "\" && " + cmd + " 2>&1";
    FILE* pipe = popen(fullCmd.c_str(), "r");
#endif
    if (!pipe) return false;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#ifdef _WIN32
    int result = _pclose(pipe);
#else
    int result = pclose(pipe);
#endif
    return result == 0;
}

std::string EmulatorCore::getEspIdfPath() const {
    if (!m_espIdfPath.empty()) return m_espIdfPath;
    const char* env = std::getenv("IDF_PATH");
    return env ? std::string(env) : "";
}

void EmulatorCore::setEspIdfPath(const std::string& path) {
    m_espIdfPath = path;
}

bool EmulatorCore::compileProject(const std::filesystem::path& projectPath) {
    if (!std::filesystem::exists(projectPath / "CMakeLists.txt")) {
        std::cerr << "Error: Not a valid ESP-IDF project (missing CMakeLists.txt)\n";
        return false;
    }

    std::string idfPath = getEspIdfPath();
    if (idfPath.empty()) {
        std::cerr << "Error: IDF_PATH not set and not provided\n";
        return false;
    }

    std::string idfPy = idfPath + "/tools/idf.py";
    if (!std::filesystem::exists(idfPy)) {
        std::cerr << "Error: idf.py not found at " << idfPy << "\n";
        return false;
    }

    std::string cmd = "python \"" + idfPy + "\" build";
    std::string output;
    std::cout << "Building project in " << projectPath << "...\n";

    if (!runCommand(cmd, projectPath, output)) {
        std::cerr << "Build failed:\n" << output << std::endl;
        return false;
    }

    std::cout << "Build succeeded.\n";
    // Путь к бинарнику: build/<project_name>.bin
    // Но лучше использовать flash_args для объединения всех частей
    return true;
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

    PeripheralComponent::SystemInterface sysInterface;
    sysInterface.requestInterrupt = [this](int irq) { this->requestInterrupt(irq); };
    sysInterface.scheduleEventRelative = [this](uint64_t ticks, auto cb) {
        this->scheduleEventRelative(ticks, std::move(cb));
    };
    sysInterface.getCurrentTick = [this]() { return this->m_globalTick.load(); };

    peripheral->setSystemInterface(std::move(sysInterface));

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
    if (pendingMask == 0) return;

    // Находим все ожидающие IRQ
    uint32_t mask = pendingMask;
    while (mask != 0) {
        int irq = __builtin_ctz(mask); // младший установленный бит

        // В ESP32 прерывания могут быть направлены на любое ядро.
        // Для простоты направим всё на PRO_CPU, но в будущем можно добавить матрицу.
        m_cpu0->requestInterrupt(irq);

        // Сбрасываем флаг
        m_pendingIrqMask.fetch_and(~(1U << irq));

        // Переходим к следующему биту
        mask &= ~(1U << irq);
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