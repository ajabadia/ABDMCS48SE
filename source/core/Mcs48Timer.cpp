#include "core/Mcs48Timer.h"

namespace bdmcs48::core {
namespace {

// El prescaler del temporizador divide el reloj de maquina entre 32.
constexpr std::uint8_t kPrescalerDivider = 32;

} // namespace

void Mcs48Timer::reset() {
    counter_ = 0;
    prescaler_ = 0;
    mode_ = TimerMode::Stopped;
    timerFlag_ = false;
}

void Mcs48Timer::startTimer() {
    mode_ = TimerMode::Timer;
    prescaler_ = 0; // STRT T reinicia el prescaler (MAME)
}

void Mcs48Timer::startCounter() {
    mode_ = TimerMode::EventCounter;
}

void Mcs48Timer::stop() {
    mode_ = TimerMode::Stopped;
}

void Mcs48Timer::setCounter(std::uint8_t value) {
    counter_ = value;
}

std::uint8_t Mcs48Timer::counter() const {
    return counter_;
}

bool Mcs48Timer::timerFlag() const {
    return timerFlag_;
}

void Mcs48Timer::clearTimerFlag() {
    timerFlag_ = false;
}

void Mcs48Timer::tick() {
    if (mode_ != TimerMode::Timer) {
        return;
    }
    if (++prescaler_ < kPrescalerDivider) {
        return;
    }
    prescaler_ = 0;
    increment();
}

void Mcs48Timer::externalPulse() {
    if (mode_ != TimerMode::EventCounter) {
        return;
    }
    increment();
}

void Mcs48Timer::increment() {
    const std::uint8_t previous = counter_;
    counter_ = static_cast<std::uint8_t>(counter_ + 1);
    if (previous == 0xff && counter_ == 0x00) {
        timerFlag_ = true;
    }
}

} // namespace bdmcs48::core
