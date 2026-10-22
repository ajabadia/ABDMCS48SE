#include "core/MachineExchange.h"

namespace bdmcs48::core {

MachineExchange::MachineExchange(std::size_t retireCapacity)
    : retired_(std::make_unique<MachineInstance*[]>(retireCapacity == 0 ? kDefaultRetireCapacity : retireCapacity)),
      retireCapacity_(retireCapacity == 0 ? kDefaultRetireCapacity : retireCapacity) {
    for (std::size_t i = 0; i < retireCapacity_; ++i) {
        retired_[i] = nullptr;
    }
}

MachineExchange::~MachineExchange() {
    // Solo es seguro destruir el intercambio cuando el hilo de audio ya ha
    // terminado: si queda una maquina pendiente se destruye aqui.
    MachineInstance* pending = pending_.exchange(nullptr, std::memory_order_acq_rel);
    delete pending;
    for (std::size_t i = 0; i < retireCapacity_; ++i) {
        delete retired_[i];
        retired_[i] = nullptr;
    }
}

bool MachineExchange::publish(std::unique_ptr<MachineInstance> candidate) {
    if (!candidate) {
        rejectedCount_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    MachineInstance* expected = nullptr;
    if (!pending_.compare_exchange_strong(expected, candidate.get(), std::memory_order_acq_rel,
                                          std::memory_order_acquire)) {
        // Ya hay una maquina pendiente: se rechaza la nueva y el
        // unique_ptr la destruye aqui (hilo de control, no de audio).
        rejectedCount_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // La propiedad pasa al intercambio: el hilo de audio la tomara.
    (void)candidate.release();
    return true;
}

MachineInstance* MachineExchange::swapAtBlockBoundary(MachineInstance* current) {
    MachineInstance* candidate = pending_.exchange(nullptr, std::memory_order_acq_rel);
    if (candidate == nullptr) {
        return current;
    }

    // La primera instalacion no retira nada: no debe ocupar un hueco de la
    // cola de retiro ni contarse como maquina pendiente de destruir.
    if (current != nullptr) {
        // Si la cola de retiro esta llena, el cambio se difiere: la maquina
        // nueva vuelve a quedar pendiente y la activa sigue sonando.
        const std::size_t write = retireWrite_.load(std::memory_order_relaxed);
        const std::size_t read = retireRead_.load(std::memory_order_acquire);
        if (write - read >= retireCapacity_) {
            pending_.store(candidate, std::memory_order_release);
            return current;
        }

        retired_[write % retireCapacity_] = current;
        retireWrite_.store(write + 1, std::memory_order_release);
    }

    swapCount_.fetch_add(1, std::memory_order_relaxed);
    return candidate;
}

std::size_t MachineExchange::drainRetired() {
    std::size_t destroyed = 0;
    const std::size_t read = retireRead_.load(std::memory_order_relaxed);
    const std::size_t write = retireWrite_.load(std::memory_order_acquire);
    while (read != write) {
        MachineInstance* instance = retired_[read % retireCapacity_];
        retired_[read % retireCapacity_] = nullptr;
        delete instance;
        ++destroyed;
        retireRead_.store(read + 1, std::memory_order_release);
        break; // una pasada por llamada: el hilo de control no debe bloquear
    }
    return destroyed;
}

std::size_t MachineExchange::retiredPending() const {
    const std::size_t write = retireWrite_.load(std::memory_order_relaxed);
    const std::size_t read = retireRead_.load(std::memory_order_relaxed);
    return write - read;
}

} // namespace bdmcs48::core
