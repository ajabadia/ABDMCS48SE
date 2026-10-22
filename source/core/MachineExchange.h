// MachineExchange.h - Cambio de maquina en caliente sin destruction en el
// hilo de audio.
//
// Por que no se usa std::atomic<std::shared_ptr<Machine>>:
//   1. La especializacion no tiene garantia de lock-free: puede bloquear.
//   2. Copiar y destruir un shared_ptr en el hilo de audio puede ejecutar el
//      destructor del objeto en ese hilo, que es justo lo que no se quiere.
//   3. El control de vida de la maquina retirada no queda explicito.
//
// Aqui el protocolo es explicito y verificable:
//   * El hilo de CONTROL prepara la maquina nueva y la publica como puntero
//     crudo. Solo hay una pendiente a la vez: el puntero atomico hace de flag.
//   * El hilo de AUDIO, en el limite de bloque, cambia el puntero activo. No
//     destruye nada: retira la maquina anterior a una cola de tamanho fijo.
//   * El hilo de CONTROL drena esa cola y destruye las maquinas retiradas.
//
// Consecuencia: el hilo de audio no reserva memoria, no bloquea y no ejecuta
// ningun destructor. El precio es explicito: si la cola de retiro se llena,
// el cambio se difiere al siguiente bloque.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/MachineInstance.h"

namespace bdmcs48::core {

class MachineExchange {
public:
    explicit MachineExchange(std::size_t retireCapacity = 8);
    ~MachineExchange();

    MachineExchange(const MachineExchange&) = delete;
    MachineExchange& operator=(const MachineExchange&) = delete;

    // Hilo de control: publica una maquina ya construida. Devuelve false si
    // ya habia una pendiente (se rechaza la nueva) o si el puntero es nulo.
    // La maquina se destruye aqui si la publicacion se rechaza.
    bool publish(std::unique_ptr<MachineInstance> candidate);

    bool hasPending() const { return pending_.load(std::memory_order_acquire) != nullptr; }

    // Hilo de audio: en el limite de bloque, si hay maquina pendiente, la
    // instala y retira la actual. Devuelve la maquina que debe seguir viva.
    // Nunca destruye ni reserva memoria.
    MachineInstance* swapAtBlockBoundary(MachineInstance* current);

    // Hilo de control: destruye las maquinas retiradas. Devuelve cuantas.
    std::size_t drainRetired();

    std::uint64_t swapCount() const { return swapCount_.load(std::memory_order_relaxed); }
    std::uint64_t rejectedCount() const { return rejectedCount_.load(std::memory_order_relaxed); }
    std::size_t retiredPending() const;

private:
    static constexpr std::size_t kDefaultRetireCapacity = 8;

    // Maquina pendiente de instalar (propiedad del hilo de control hasta que
    // el hilo de audio la toma).
    std::atomic<MachineInstance*> pending_{nullptr};

    // Cola de retiro: la empuja el hilo de audio, la vacia el hilo de control.
    std::unique_ptr<MachineInstance*[]> retired_;
    std::size_t retireCapacity_ = 0;
    alignas(64) std::atomic<std::size_t> retireWrite_{0};
    alignas(64) std::atomic<std::size_t> retireRead_{0};

    std::atomic<std::uint64_t> swapCount_{0};
    std::atomic<std::uint64_t> rejectedCount_{0};
};

} // namespace bdmcs48::core
