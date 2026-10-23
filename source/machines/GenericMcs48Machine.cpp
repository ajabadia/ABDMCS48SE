#include "machines/GenericMcs48Machine.h"

#include <algorithm>
#include <cmath>

namespace bdmcs48::machines {
namespace {

// El DAC de 8 bits sin signo se convierte a [-1, 1].
float dacToFloat(std::uint8_t value) {
    return (static_cast<float>(value) - 127.5f) / 127.5f;
}

} // namespace

GenericMcs48Machine::GenericMcs48Machine(Config config)
    : config_(config),
      cpu_(program_, this, core::Mcs48Cpu::Config{config.ramSize, config.clockHz, config.clocksPerCycle}),
      clock_(config.clockHz, config.clocksPerCycle, config.sampleRateHz) {}

void GenericMcs48Machine::setProgram(const core::ProgramMemory& program) {
    program_ = program;
}

void GenericMcs48Machine::setDataRegion(std::vector<std::uint8_t> data) {
    dataRegion_ = std::move(data);
    dataRegion_.resize(256, 0xff);
}

void GenericMcs48Machine::reset() {
    cpu_.reset();
    dacValue_ = 0;
    controlValue_ = 0;
    latches_[0] = 0;
    latches_[1] = 0;
    framesRendered_ = 0;
    clock_.reset();
}

void GenericMcs48Machine::rebuildClock() {
    const std::uint32_t effective = effectiveClockHz();
    // El reloj de la CPU cambia con el parametro, pero la maquina sigue
    // consumiendo muestras a la misma tasa: por eso se reconstruye el
    // conversor conservando la fraccion pendiente.
    clock_ = core::CoreClock(effective == 0 ? 1u : effective, config_.clocksPerCycle, config_.sampleRateHz);
    // El CPU no se reinicia al cambiar el reloj: su contador de ciclos sigue
    // creciendo. El desfase mantiene la comparacion absoluta de runBlock.
    cycleOffset_ = cpu_.cycles();
}

void GenericMcs48Machine::setParameter(std::uint32_t id, float value) {
    if (id != kParameterEngineClock) {
        return;
    }
    // El contrato de MachineInstance entrega el parametro normalizado (0..1):
    // el mismo valor que envia el host desde la cola de comandos. Proyectarlo
    // al rango real evita que una automatizacion al 100 % se quede a mitad de
    // camino (antes se recortaba a 0,25..4,0 como si ya fuese la escala).
    const double normalized = std::clamp(static_cast<double>(value), 0.0, 1.0);
    const double scale = static_cast<double>(kMinClockScale) +
                         normalized * (static_cast<double>(kMaxClockScale) -
                                       static_cast<double>(kMinClockScale));
    if (scale == clockScale_) {
        return;
    }
    clockScale_ = scale;
    rebuildClock();
}

void GenericMcs48Machine::writeInput(std::uint32_t id, float value) {
    if (id >= kLatchCount) {
        return;
    }
    const double clamped = std::clamp(static_cast<double>(value), 0.0, 1.0);
    latches_[id] = static_cast<std::uint8_t>(std::lround(clamped * 255.0));
}

void GenericMcs48Machine::runBlock(std::uint32_t frames, float* const* outputs, std::uint32_t channels) {
    if (outputs == nullptr || channels == 0) {
        return;
    }

    // Se avanza muestra a muestra: el DAC mantiene su valor entre
    // escrituras (retencion de orden cero) y el momento de la escritura
    // importa para la posicion de la muestra affected.
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        clock_.cyclesForSamples(1);
        // Se ejecuta hasta el limite ABSOLUTO de ciclos, no una instruccion
        // entera por muestra. runCycles() redondea hacia arriba hasta acabar
        // la instruccion en curso; si ese sobrante se descartase en cada
        // muestra, la CPU correria mas rapido que su reloj (hasta un 10 % con
        // engine_clock a 1.0) y toda afinacion quedaria desafinada. Comparando
        // contra el contador del CPU el sobrante se arrastra a la muestra
        // siguiente y la media es exacta.
        const std::uint64_t target = cycleOffset_ + clock_.totalCycles();
        while (cpu_.cycles() < target && !cpu_.faulted()) {
            cpu_.step();
        }
        const float sample = dacToFloat(dacValue_);
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            if (outputs[channel] != nullptr) {
                outputs[channel][frame] = sample;
            }
        }
    }
    framesRendered_ += frames;
}

core::EngineStatus GenericMcs48Machine::status() const {
    core::EngineStatus status;
    status.framesRendered = framesRendered_;
    status.cpuCycles = cpu_.cycles();
    status.programCounter = cpu_.programCounter();
    status.accumulator = cpu_.accumulator();
    status.psw = cpu_.psw();
    status.bank = cpu_.bank();
    status.faulted = cpu_.faulted() ? 1u : 0u;
    status.faultMessage = cpu_.faultMessage();
    return status;
}

std::uint8_t GenericMcs48Machine::readPort(std::uint8_t port) {
    switch (port) {
    case 1: return latches_[0];
    case 2: return latches_[1];
    default: return 0xff;
    }
}

void GenericMcs48Machine::writePort(std::uint8_t port, std::uint8_t value) {
    switch (port) {
    case 1: dacValue_ = value; break;
    case 2: controlValue_ = value; break;
    default: break;
    }
}

std::uint8_t GenericMcs48Machine::readExternal(std::uint8_t address) {
    return dataRegion_[address % dataRegion_.size()];
}

void GenericMcs48Machine::writeExternal(std::uint8_t address, std::uint8_t value) {
    dataRegion_[address % dataRegion_.size()] = value;
}

} // namespace bdmcs48::machines
