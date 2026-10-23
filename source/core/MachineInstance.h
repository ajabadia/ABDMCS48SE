// MachineInstance.h - Contrato de una maquina de sonido.
// Es la frontera que el host conoce. El core no sabe que hay una CPU dentro:
// un perfil (por ejemplo la maquina MCS-48 generica) implementa este
// contrato y el host la puede sustituir en caliente.
//
// El estado de la maquina se consulta mediante status() const: lo llama el
// hilo de control y no debe modificar nada. El hilo de audio solo publica
// instantaneas vía el canal de instantaneas.
//
// El ciclo de vida en caliente (que maquina queda retirada y cuando se
// destruye) NO es asunto de la maquina: lo lleva MachineExchange, que es
// quien conserva la propiedad de las instancias sustituidas.

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

    // Estado para diagnostico. Es const porque el host lo consulta desde el
    // hilo de control sin tocar el estado de la maquina.
    virtual EngineStatus status() const = 0;
};

} // namespace bdmcs48::core
