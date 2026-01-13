#ifndef PERIPHERALCOMPONENT_H
#define PERIPHERALCOMPONENT_H

#include <cstdint>
#include <functional>

// Предварительное объявление
class EmulatorCore;

/**
 * @brief Абстрактный базовый класс для всех периферийных устройств ESP32.
 *
 * Обеспечивает:
 * - Единый интерфейс для чтения/записи регистров.
 * - Интеграцию с глобальным таймером эмулятора через onTick().
 * - Возможность планирования отложенных событий (например, UART-передача).
 * - Механизм запроса прерываний.
 */
class PeripheralComponent
{
public:
    explicit PeripheralComponent(uint32_t baseAddress);
    virtual ~PeripheralComponent() = default;

    // Запрет копирования и перемещения
    PeripheralComponent(const PeripheralComponent&) = delete;
    PeripheralComponent& operator=(const PeripheralComponent&) = delete;

    /**
     * @brief Чтение из регистра периферии по смещению.
     * Вызывается MemoryMap при обращении к адресу [baseAddress + offset].
     */
    virtual uint32_t readRegister(uint32_t offset) = 0;

    /**
     * @brief Запись в регистр периферии по смещению.
     * Вызывается MemoryMap при записи по адресу [baseAddress + offset].
     */
    virtual void writeRegister(uint32_t offset, uint32_t value) = 0;

    /**
     * @brief Коллбэк, вызываемый EmulatorCore на каждом такте (tick).
     * Используется для обновления внутреннего состояния, связанного со временем
     * (например, инкремент таймера, обработка битов UART).
     */
    virtual void onTick(uint64_t globalTick) = 0;

    // --- Вспомогательные методы для интеграции ---

    /**
     * @brief Устанавливает указатель на основной эмулятор.
     * Необходим для доступа к глобальному времени, планировщику событий и памяти.
     */
    void setEmulatorCore(EmulatorCore* core) { m_emulatorCore = core; }

    /**
     * @brief Планирует выполнение события в будущем.
     * Удобная обёртка для EmulatorCore::scheduleEvent.
     */
    void scheduleEventInUs(uint64_t microseconds, std::function<void()> callback);

    /**
     * @brief Планирует выполнение события в будущем (в тактах).
     * Удобная обёртка для EmulatorCore::scheduleEvent.
     */
    void scheduleEventAtTick(uint64_t tick, std::function<void()> callback);

    /**
     * @brief Запрашивает прерывание у CPU.
     * Реализация зависит от конкретного устройства и его номера IRQ.
     */
    virtual void requestInterrupt() = 0;

    // --- Геттеры ---

    uint32_t getBaseAddress() const { return m_baseAddress; }
    EmulatorCore* getEmulatorCore() const { return m_emulatorCore; }

protected:
    EmulatorCore* m_emulatorCore = nullptr;
    const uint32_t m_baseAddress;
};

#endif // PERIPHERALCOMPONENT_H