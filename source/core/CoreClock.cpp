#include "core/CoreClock.h"

namespace bdmcs48::core {

CoreClock::CoreClock(std::uint32_t clockHz, std::uint32_t clocksPerCycle, std::uint32_t sampleRateHz)
    : clockHz_(clockHz == 0 ? 1u : clockHz),
      clocksPerCycle_(clocksPerCycle == 0 ? 1u : clocksPerCycle),
      sampleRateHz_(sampleRateHz == 0 ? 1u : sampleRateHz) {
    denominator_ = static_cast<std::uint64_t>(clocksPerCycle_) * static_cast<std::uint64_t>(sampleRateHz_);
    if (denominator_ == 0) {
        denominator_ = 1;
    }
    reset();
}

void CoreClock::reset() {
    pending_ = 0;
    totalCycles_ = 0;
}

std::uint64_t CoreClock::cyclesForSamples(std::uint64_t samples) {
    // El numerador se acumula en el mismo denominador: al dividir se
    // obtiene el cociente entero y el resto se guarda para la proxima
    // llamada. No hay truncado y, por tanto, no hay deriva.
    pending_ += samples * static_cast<std::uint64_t>(clockHz_);
    const std::uint64_t whole = pending_ / denominator_;
    pending_ %= denominator_;
    totalCycles_ += whole;
    return whole;
}

std::uint64_t CoreClock::pendingCycles() const {
    return pending_ / denominator_;
}

double CoreClock::cpuCyclesPerSecond() const {
    return static_cast<double>(clockHz_) / static_cast<double>(clocksPerCycle_);
}

double CoreClock::secondsForSamples(std::uint64_t samples) const {
    return static_cast<double>(samples) / static_cast<double>(sampleRateHz_);
}

} // namespace bdmcs48::core
