// MachineInstance.h - Contrato de una maquina de sonido.
// Es la frontera que el host conoce. El core no sabe que hay una CPU dentro:
// un perfil (por ejemplo la maquina MCS-48 generica) implementa este
// contrato y el host la puede sustituir en caliente.
//
// El estado de la maquina se consulta mediante status(), que es un metodo
// no-const: lo llama el hilo de control y no el de audio. El hilo de audio
// solo publica instantaneas vía el canal de instantaneas.
//
// Parte del protocolo de intercambio en caliente:
//   * retiredPending(): cantidad de maquinas retiradas que aun no se
//     destruyen. Lo lee el hilo de control para saber si el intercambio se
//     puede completar.
//   * drainRetired(): destruye las maquinas retiradas. Lo ejecuta el hilo
//     de control despues de que el hilo de audio ya no las necesita.
//   * status() const: la version constante para lectura de diagnostico que
//     no modifica el estado de la maquina.

#pragma once

#include <cstdint>
#include <string>

namespace bdmcs48::core {

struct EngineStatus {
    std::uint64_t framesRendered = 0;
    std::uint64_t cpuCycles = 0;
    std::uint16_t programCounter = 0;
    std::uint8_t accumulator = 0;
    std::uint8_t psw = 0;
    std::uint8_t bank = 0;        // flip-flop A11 del MCS-48
    std::uint8_t faulted = 0;
    std::string faultMessage;
};

class MachineInstance {
public:
    virtual ~MachineInstance() = default;

    // Identificador del perfil ("generic_mcs48" por ejemplo).
    virtual std::string profileId() const = 0;

    // Devuelve la maquina al estado inicial.
    virtual void reset() = 0;

    // Parametro continuo (0..1) identificado por id.
    virtual void setParameter(std::uint32_t id, float value) = 0;

    // Entrada externa (latch, teclado, trigger) identificada por id.
    virtual void writeInput(std::uint32_t id, float value) = 0;

    // Renderiza `frames` muestras en `outputs` (una fila por canal).
    virtual void runBlock(std::uint32_t frames, float* const* outputs, std::uint32_t channels) = 0;

    // Estado para diagnostico.
    virtual EngineStatus status() = 0;

    // Interior del protocolo de intercambio en caliente. La maquina actua
    // informa cuantas instancias retiradas aun no se destruyeron y permite
    // que el hilo de control las recorra antes de finalizar.
    virtual std::uint64_t retiredPending() const = 0;
    virtual void drainRetired() = 0;
};

} // namespace bdmcs48::core
