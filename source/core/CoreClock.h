// CoreClock.h - Conversion Samples <-> ciclos de CPU.
//
// El core no debe depender del reloj de audio del host: recibe bloques de
// muestras y devuelve cuantos ciclos de CPU debe ejecutar.
//
// La conversion es EXACTA y no acumula deriva. La cuenta es
//
//     ciclos = muestras * clockHz / (clocksPerCycle * sampleRateHz)
//
// y el resto de la division se guarda para la siguiente llamada. Si en
// cambio se truncase el cociente en coma flotante, 44100 muestras a 44100 Hz
// darian 0,9999999 s de CPU: un error de una diezmilesima que, repetido
// bloque a bloque, baja el tono de forma audible. Con el resto acumulado,
// 44100 muestras son exactamente 400000 ciclos a 6 MHz con 15 oscilaciones
// por ciclo, y el error es siempre menor de un ciclo.

#pragma once

#include <cstdint>

namespace bdmcs48::core {

class CoreClock {
public:
    // clockHz: frecuencia del oscilador de la CPU.
    // clocksPerCycle: oscilaciones por ciclo de maquina (15 en el MCS-48).
    // sampleRateHz: frecuencia de las muestras de audio del host.
    CoreClock(std::uint32_t clockHz, std::uint32_t clocksPerCycle, std::uint32_t sampleRateHz);

    void reset();

    // Convierte un numero de muestras en ciclos de CPU. Devuelve los ciclos
    // enteros que corresponden a esas muestras (puede ser 0 en bloques cortos).
    std::uint64_t cyclesForSamples(std::uint64_t samples);

    // Ciclos acumulados desde el ultimo reset.
    std::uint64_t totalCycles() const { return totalCycles_; }

    // Ciclos que quedan pendientes por la fraccion acumulada.
    std::uint64_t pendingCycles() const;

    std::uint32_t clockHz() const { return clockHz_; }
    std::uint32_t clocksPerCycle() const { return clocksPerCycle_; }
    std::uint32_t sampleRateHz() const { return sampleRateHz_; }

    // Ciclos de CPU por segundo (tiempo real de la maquina).
    double cpuCyclesPerSecond() const;

    // Segundos de tiempo de maquina que corresponden a un numero de muestras.
    double secondsForSamples(std::uint64_t samples) const;

private:
    std::uint32_t clockHz_ = 0;
    std::uint32_t clocksPerCycle_ = 0;
    std::uint32_t sampleRateHz_ = 0;
    std::uint64_t denominator_ = 1;  // clocksPerCycle * sampleRateHz
    std::uint64_t pending_ = 0;      // resto de la division, en el mismo denominador
    std::uint64_t totalCycles_ = 0;
};

} // namespace bdmcs48::core
