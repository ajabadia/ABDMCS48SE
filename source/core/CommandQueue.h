// CommandQueue.h - Cola de comandos sin bloqueo (SPSC).
//
// Un solo productor (hilo de control / UI) y un solo consumidor (hilo de
// audio). La cola es de tamano fijo: el hilo de audio NUNCA reserva
// memoria. Si el productor se adelanta, las ordenes se descartan y se cuenta
// el descarte: es preferible perder un comando de UI que bloquear el audio.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace bdmcs48::core {

struct Command {
    std::uint32_t id = 0;       // identificador del comando
    std::int32_t value = 0;     // valor entero
    std::uint32_t sequence = 0;  // numero de secuencia del productor
};

class CommandQueue {
public:
    static constexpr std::size_t kCapacity = 256; // potencia de dos

    CommandQueue() = default;

    // Productor: encola un comando. Devuelve false si la cola esta llena.
    bool push(const Command& command) {
        const std::size_t write = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t read = readIndex_.load(std::memory_order_acquire);
        if (write - read >= kCapacity) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        buffer_[write & kMask] = command;
        writeIndex_.store(write + 1, std::memory_order_release);
        return true;
    }

    // Consumidor: extrae un comando. Devuelve false si no hay ninguno.
    bool pop(Command& out) {
        const std::size_t read = readIndex_.load(std::memory_order_relaxed);
        if (read == writeIndex_.load(std::memory_order_acquire)) {
            return false;
        }
        out = buffer_[read & kMask];
        readIndex_.store(read + 1, std::memory_order_release);
        return true;
    }

    // Comandos pendientes (aproximado: es una instantanea).
    std::size_t pending() const {
        const std::size_t write = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t read = readIndex_.load(std::memory_order_relaxed);
        return write - read;
    }

    // Comandos descartados por cola llena.
    std::uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

    // Devuelve la capacidad maxima sin rebasar (0 = no hay capacidad).
    std::size_t capacity() const { return kCapacity; }

    void clear() {
        readIndex_.store(writeIndex_.load(std::memory_order_relaxed), std::memory_order_release);
    }

private:
    static constexpr std::size_t kMask = kCapacity - 1;

    // Cada indice va en su propia linea de cache: el productor escribe uno y
    // el consumidor lee el otro.
    alignas(64) std::atomic<std::size_t> writeIndex_{0};
    alignas(64) std::atomic<std::size_t> readIndex_{0};
    alignas(64) std::atomic<std::uint64_t> dropped_{0};
    alignas(64) Command buffer_[kCapacity]{};
};

} // namespace bdmcs48::core
