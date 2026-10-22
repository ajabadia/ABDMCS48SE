// GenericMcs48Machine.h - Perfil de maquina MCS-48 generico.
//
// Es un perfil de REFERENCIA, no una replica de ningun equipo concreto:
//   * P1 escribe -> DAC de 8 bits (salida de audio).
//   * P1 lee    -> latch de entrada 0.
//   * P2 escribe -> registro de control legible por el firmware.
//   * P2 lee    -> latch de entrada 1.
//   * Bus externo (MOVX) -> region de datos del paquete (wavetables).
//
// El perfil no sabe nada del host: recibe bloques de muestras y devuelve
// audio. El reloj lo lleva la propia maquina, de modo que el parametro
// "engine_clock" cambia la velocidad sin tocar el host.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/CoreClock.h"
#include "core/MachineInstance.h"
#include "core/Mcs48Cpu.h"
#include "core/ProgramMemory.h"

namespace bdmcs48::machines {

class GenericMcs48Machine final : public core::MachineInstance, public core::PortBus {
public:

    struct Config {
        std::uint32_t sampleRateHz;
        std::uint32_t clockHz;
        std::uint32_t clocksPerCycle;
        std::size_t ramSize;
    };

    // Los valores por defecto se pasan en el constructor para que el
    // inicializador de miembros no sepa de la clase antes de su fin.
    explicit GenericMcs48Machine(std::uint32_t sampleRateHz = 44'100,
                                 std::uint32_t clockHz = 6'000'000,
                                 std::uint32_t clocksPerCycle = 15,
                                 std::size_t ramSize = 128);

    // Identificadores de parametro y entrada del perfil.
    static constexpr std::uint32_t kParameterEngineClock = 1;
    static constexpr std::uint32_t kInputLatch0 = 0;
    static constexpr std::uint32_t kInputLatch1 = 1;
    static constexpr std::size_t kLatchCount = 2;

    explicit GenericMcs48Machine(Config config = Config());

    // Instala el programa (banco 0 y 1) y la region de datos.
    void setProgram(const core::ProgramMemory& program);
    void setDataRegion(std::vector<std::uint8_t> data);

    // core::MachineInstance
    std::string profileId() const override { return "generic-mcs48"; }
    void reset() override;
    void setParameter(std::uint32_t id, float value) override;
    void writeInput(std::uint32_t id, float value) override;
    void runBlock(std::uint32_t frames, float* const* outputs, std::uint32_t channels) override;
    core::EngineStatus status() const;

    // core::PortBus
    std::uint8_t readPort(std::uint8_t port) override;
    void writePort(std::uint8_t port, std::uint8_t value) override;
    std::uint8_t readExternal(std::uint8_t address) override;
    void writeExternal(std::uint8_t address, std::uint8_t value) override;

    // Ultimo valor escrito en el DAC (0..255) y control.
    std::uint8_t dacValue() const { return dacValue_; }
    std::uint8_t controlValue() const { return controlValue_; }

    const core::Mcs48Cpu& cpu() const { return cpu_; }
    double clockScale() const { return clockScale_; }

private:
    void rebuildClock();

    Config config_;
    core::ProgramMemory program_;
    std::vector<std::uint8_t> dataRegion_;
    core::Mcs48Cpu cpu_;
    core::CoreClock clock_;

    std::uint8_t dacValue_ = 0;
    std::uint8_t controlValue_ = 0;
    std::uint8_t latches_[kLatchCount] = {0, 0};
    double clockScale_ = 1.0;
    std::uint64_t framesRendered_ = 0;
};

} // namespace bdmcs48::machines
