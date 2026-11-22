#include "machines/GenericMcs48Machine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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
      clock_(config.clockHz, config.clocksPerCycle, config.sampleRateHz) {
    rebuildDacLut();
}

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
    dacQuantizeBits_ = 8;
    freeze_ = false;
    clockScale_ = 1.0;
    clock_.reset();
    rebuildClock();
}

void GenericMcs48Machine::setDacTolerance(float tolerance) {
    const float clamped = std::clamp(tolerance, 0.0f, 1.0f);
    if (std::abs(clamped - dacTolerance_) < 1e-6f) {
        return;
    }
    dacTolerance_ = clamped;
    rebuildDacLut();
}

void GenericMcs48Machine::rebuildDacLut() {
    // Tolerancias deterministas de resistencias R-2R tipicas vintage
    constexpr float kBitMismatches[8] = {
        +0.025f, -0.020f, +0.035f, -0.030f,
        +0.040f, -0.028f, +0.018f, -0.035f
    };

    const float tol = std::clamp(dacTolerance_, 0.0f, 1.0f);

    for (int i = 0; i < 256; ++i) {
        const float ideal = (static_cast<float>(i) - 127.5f) / 127.5f;
        if (tol <= 0.0f) {
            dacLut_[i] = ideal;
            continue;
        }

        float r2rSum = 0.0f;
        for (int b = 0; b < 8; ++b) {
            if ((i >> b) & 1) {
                const float bitWeight = static_cast<float>(1 << b) * (1.0f + kBitMismatches[b] * tol);
                r2rSum += bitWeight;
            }
        }
        const float r2rNormalized = (r2rSum - 127.5f) / 127.5f;
        const float saturated = std::tanh(1.2f * r2rNormalized) / std::tanh(1.2f);
        dacLut_[i] = (1.0f - tol) * ideal + tol * saturated;
    }
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
    if (id == kParameterDacTolerance) {
        setDacTolerance(std::clamp(value, 0.0f, 1.0f));
        return;
    }
    if (id == kParameterDacQuantize) {
        // value normalizado 0.0..1.0 mapeado a 1..8 bits
        const int bits = std::clamp(static_cast<int>(std::lround(1.0 + std::clamp(value, 0.0f, 1.0f) * 7.0)), 1, 8);
        dacQuantizeBits_ = static_cast<std::uint8_t>(bits);
        return;
    }
    if (id == kParameterFreeze) {
        const bool frozen = (value >= 0.5f);
        if (freeze_ && !frozen) {
            // Al descongelar, resincronizar cycleOffset_ para evitar saltos masivos de CPU
            cycleOffset_ = cpu_.cycles();
        }
        freeze_ = frozen;
        return;
    }
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
        if (!freeze_) {
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
        }
        std::uint8_t effectiveDac = dacValue_;
        if (dacQuantizeBits_ < 8) {
            const std::uint8_t shift = static_cast<std::uint8_t>(8 - dacQuantizeBits_);
            effectiveDac = static_cast<std::uint8_t>((effectiveDac >> shift) << shift);
        }
        const float sample = dacLut_[effectiveDac];
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
    std::strncpy(status.faultMessage, cpu_.faultMessageCStr(), sizeof(status.faultMessage) - 1);
    status.faultMessage[sizeof(status.faultMessage) - 1] = '\0';
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
