// Diagnostics.h - Diagnostico sin carrera entre el hilo de audio y la UI.
//
// El hilo de audio publica un estado (instantanea) y el hilo de la UI lo
// lee. La instantanea viaja en un canal con seqlock: el escritor marca la
// secuencia en impar mientras escribe y en par cuando termina; el lector
// comprueba que la secuencia no haya cambiado durante la copia.
//
// El payload son palabras ATOMICAS, no bytes planos: un seqlock con datos no
// atomicos es una carrera de datos segun el modelo de memoria de C++, y el
// compilador tiene derecho a "optimizar" la copia de forma que devuelva un
// estado incoherente. Con palabras atomicas la lectura es siempre legal y
// la coherencia la garantiza la secuencia.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace bdmcs48::core {

struct MachineSnapshot {
    std::uint64_t frameCounter = 0;
    std::uint64_t cpuCycles = 0;
    std::uint32_t programCounter = 0;
    std::uint8_t accumulator = 0;
    std::uint8_t psw = 0;
    std::uint8_t bank = 0;
    std::uint8_t faulted = 0;
    float outputLevel = 0.0f;
    std::string message;
};

class SnapshotChannel {
public:
    static constexpr std::size_t kWords = 12;
    static constexpr std::size_t kMessageChars = 40;

    // Hilo de audio.
    void publish(const MachineSnapshot& snapshot);

    // Hilo de UI. Devuelve false si no se pudo obtener una copia coherente
    // (el escritor esta justo en mitad de una publicacion).
    bool read(MachineSnapshot& out) const;

    std::uint64_t publishedCount() const { return published_.load(std::memory_order_relaxed); }

private:
    void encode(const MachineSnapshot& snapshot, std::uint64_t* words) const;
    void decode(const std::uint64_t* words, MachineSnapshot& out) const;

    mutable std::atomic<std::uint64_t> sequence_{0};
    mutable std::atomic<std::uint64_t> words_[kWords]{};
    std::atomic<std::uint64_t> published_{0};
};

// ---------------------------------------------------------------------------
// Bus de registros: del hilo de control al cliente WebView.
// ---------------------------------------------------------------------------
struct LogRecord {
    std::uint64_t timestampUs = 0;
    std::uint32_t id = 0;
    std::int32_t value = 0;
    char text[48]{};
};

class LogBus {
public:
    static constexpr std::size_t kCapacity = 128;

    bool push(const LogRecord& record) {
        const std::size_t write = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t read = readIndex_.load(std::memory_order_acquire);
        if (write - read >= kCapacity) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        buffer_[write % kCapacity] = record;
        writeIndex_.store(write + 1, std::memory_order_release);
        return true;
    }

    bool pop(LogRecord& out) {
        const std::size_t read = readIndex_.load(std::memory_order_relaxed);
        if (read == writeIndex_.load(std::memory_order_acquire)) {
            return false;
        }
        out = buffer_[read % kCapacity];
        readIndex_.store(read + 1, std::memory_order_release);
        return true;
    }

    std::size_t pending() const {
        const std::size_t write = writeIndex_.load(std::memory_order_relaxed);
        const std::size_t read = readIndex_.load(std::memory_order_relaxed);
        return write - read;
    }

    std::uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    alignas(64) std::atomic<std::size_t> writeIndex_{0};
    alignas(64) std::atomic<std::size_t> readIndex_{0};
    alignas(64) std::atomic<std::uint64_t> dropped_{0};
    alignas(64) LogRecord buffer_[kCapacity]{};
};

} // namespace bdmcs48::core
