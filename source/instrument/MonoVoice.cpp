#include "instrument/MonoVoice.h"

#include <algorithm>
#include <cmath>

namespace bdmcs48::instrument {
namespace {

double clampNote(double note) {
    return std::clamp(note, 0.0, 127.0);
}

} // namespace

void MonoVoice::prepare(double sampleRateHz, const VoiceConfig& config) {
    sampleRate_ = sampleRateHz > 0.0 ? sampleRateHz : 44'100.0;
    config_ = config;
    refreshTimes();
    reset();
}

void MonoVoice::refreshTimes() {
    const auto stepFor = [this](double milliseconds) {
        const double samples = (milliseconds / 1000.0) * sampleRate_;
        return samples <= 1.0 ? 1.0f : static_cast<float>(1.0 / samples);
    };
    attackStep_ = stepFor(config_.attackMs);
    releaseStep_ = stepFor(config_.releaseMs);

    if (config_.glideMs <= 0.0) {
        glideStepCoeff_ = 1.0;
    } else {
        const double tau = (config_.glideMs / 1000.0) / 4.6;
        const double samples = tau * sampleRate_;
        glideStepCoeff_ = samples <= 1.0 ? 1.0 : (1.0 - std::exp(-1.0 / samples));
    }
}

void MonoVoice::reset() {
    held_.fill(-1);
    heldCount_ = 0;
    currentNote_ = -1;
    currentPitch_ = 69.0;
    targetPitch_ = 69.0;
    isGliding_ = false;
    velocity_ = 0.0f;
    gain_ = 0.0f;
    stage_ = Stage::Idle;
    refreshTimes();
}

double MonoVoice::frequencyForNote(double note, const VoiceConfig& config) {
    return config.referenceHz * std::pow(2.0, (clampNote(note) - config.referenceNote) / 12.0);
}

double MonoVoice::clockScaleForNote(double note) const {
    const double frequency = frequencyForNote(note, config_);
    if (config_.baseFrequencyHz <= 0.0) {
        return 1.0;
    }
    return frequency / config_.baseFrequencyHz;
}

float MonoVoice::clockParameter() const {
    if (currentNote_ < 0) {
        return 0.0f;
    }
    const double span = config_.maxClockScale - config_.minClockScale;
    if (span <= 0.0) {
        return 0.0f;
    }
    const double noteToUse = isGliding_ ? currentPitch_ : static_cast<double>(currentNote_);
    const double scale = std::clamp(clockScaleForNote(noteToUse),
                                    config_.minClockScale, config_.maxClockScale);
    return static_cast<float>((scale - config_.minClockScale) / span);
}

std::int32_t MonoVoice::clockCommandValue() const {
    const float normalized = std::clamp(clockParameter(), 0.0f, 1.0f);
    // redondeo, no truncado: el host divide por 32767 y una truncacion
    // desviaria la afinacion hasta un semitono en las notas graves.
    return static_cast<std::int32_t>(std::lround(static_cast<double>(normalized) * 32767.0));
}

void MonoVoice::startAttack() {
    if (config_.attackMs <= 0.0 || gain_ >= 1.0f) {
        gain_ = 1.0f;
        stage_ = Stage::Hold;
        return;
    }
    attackStep_ = static_cast<float>(1.0 / std::max(1.0, (config_.attackMs / 1000.0) * sampleRate_));
    stage_ = Stage::Attack;
}

void MonoVoice::startRelease() {
    if (gain_ <= 0.0f) {
        gain_ = 0.0f;
        stage_ = Stage::Idle;
        currentNote_ = -1;
        return;
    }
    releaseStep_ = static_cast<float>(gain_ / std::max(1.0, (config_.releaseMs / 1000.0) * sampleRate_));
    stage_ = Stage::Release;
}

void MonoVoice::setReleaseMs(double milliseconds) {
    const double clamped = std::max(0.0, milliseconds);
    if (std::abs(clamped - config_.releaseMs) < 1e-9) {
        // El host repite el parametro en cada bloque. Recalcular la pendiente
        // durante una liberacion en curso la alargaria indefinidamente
        // (ganancia actual / muestras restantes, bloque tras bloque).
        return;
    }
    config_.releaseMs = clamped;
    refreshTimes();
    if (stage_ == Stage::Release) {
        releaseStep_ = static_cast<float>(gain_ / std::max(1.0, (config_.releaseMs / 1000.0) * sampleRate_));
    }
}

void MonoVoice::setAttackMs(double milliseconds) {
    const double clamped = std::max(0.1, milliseconds);
    if (std::abs(clamped - config_.attackMs) < 1e-9) {
        return;
    }
    config_.attackMs = clamped;
    refreshTimes();
    if (stage_ == Stage::Attack) {
        attackStep_ = static_cast<float>(1.0 / std::max(1.0, (config_.attackMs / 1000.0) * sampleRate_));
    }
}

void MonoVoice::setReferenceHz(double hz) {
    const double clamped = std::clamp(hz, 200.0, 1000.0);
    if (std::abs(clamped - config_.referenceHz) < 1e-9) {
        return;
    }
    config_.referenceHz = clamped;
}

void MonoVoice::setVelocitySens(float sens) {
    velocitySens_ = std::clamp(sens, 0.0f, 1.0f);
}

void MonoVoice::setGlideMs(double milliseconds) {
    const double clamped = std::clamp(milliseconds, 0.0, 2000.0);
    if (std::abs(clamped - config_.glideMs) < 1e-9) {
        return;
    }
    config_.glideMs = clamped;
    refreshTimes();
}

void MonoVoice::setVelocityCurve(VelocityCurve curve) {
    config_.velocityCurve = curve;
}

int MonoVoice::lastHeldNote() const {
    return heldCount_ == 0 ? -1 : held_[heldCount_ - 1];
}

void MonoVoice::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) {
        return;
    }
    // Repetir una tecla ya pulsada la mueve al final (ultima nota).
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i] == note) {
            for (std::size_t j = i; j + 1 < heldCount_; ++j) {
                held_[j] = held_[j + 1];
            }
            --heldCount_;
            break;
        }
    }

    const bool wasLegato = (stage_ == Stage::Hold || stage_ == Stage::Attack) && (heldCount_ > 0);

    if (heldCount_ < kMaxHeldNotes) {
        held_[heldCount_++] = note;
    } else {
        // Sin hueco libre: se descarta la nota mas antigua.
        for (std::size_t i = 1; i < heldCount_; ++i) {
            held_[i - 1] = held_[i];
        }
        held_[heldCount_ - 1] = note;
    }

    currentNote_ = note;
    targetPitch_ = static_cast<double>(note);

    if (wasLegato && config_.glideMs > 0.0) {
        isGliding_ = true;
    } else {
        currentPitch_ = targetPitch_;
        isGliding_ = false;
    }

    const float rawVel = std::clamp(velocity, 0.0f, 1.0f);
    float curvedVel = rawVel;
    switch (config_.velocityCurve) {
    case VelocityCurve::Linear:
        curvedVel = rawVel;
        break;
    case VelocityCurve::Exponential:
        curvedVel = rawVel * rawVel;
        break;
    case VelocityCurve::Logarithmic:
        curvedVel = std::sqrt(rawVel);
        break;
    case VelocityCurve::Stepped:
        if (rawVel < 0.25f) curvedVel = 0.25f;
        else if (rawVel < 0.50f) curvedVel = 0.50f;
        else if (rawVel < 0.75f) curvedVel = 0.75f;
        else curvedVel = 1.0f;
        break;
    }
    velocity_ = std::clamp((1.0f - velocitySens_) + velocitySens_ * curvedVel, 0.0f, 1.0f);

    if (wasLegato) {
        // Legato: la altura cambia sin volver a disparar la envolvente.
        stage_ = gain_ >= 1.0f ? Stage::Hold : Stage::Attack;
        if (stage_ == Stage::Hold) {
            gain_ = 1.0f;
        }
    } else {
        startAttack();
    }
}

void MonoVoice::noteOff(int note) {
    if (note < 0 || note > 127) {
        return;
    }
    for (std::size_t i = 0; i < heldCount_; ++i) {
        if (held_[i] == note) {
            for (std::size_t j = i; j + 1 < heldCount_; ++j) {
                held_[j] = held_[j + 1];
            }
            --heldCount_;
            break;
        }
    }

    if (note != currentNote_) {
        return;
    }
    if (heldCount_ > 0) {
        currentNote_ = lastHeldNote();
        targetPitch_ = static_cast<double>(currentNote_);
        if (config_.glideMs > 0.0 && (stage_ == Stage::Hold || stage_ == Stage::Attack)) {
            isGliding_ = true;
        } else {
            currentPitch_ = targetPitch_;
            isGliding_ = false;
        }
        if (stage_ == Stage::Release || stage_ == Stage::Idle) {
            startAttack();
        }
        return;
    }
    startRelease();
}

void MonoVoice::allNotesOff() {
    heldCount_ = 0;
    isGliding_ = false;
    if (stage_ != Stage::Idle) {
        startRelease();
    }
}

float MonoVoice::nextGain() {
    if (isGliding_) {
        currentPitch_ += glideStepCoeff_ * (targetPitch_ - currentPitch_);
        if (std::abs(currentPitch_ - targetPitch_) <= 0.01) {
            currentPitch_ = targetPitch_;
            isGliding_ = false;
        }
    }

    switch (stage_) {
    case Stage::Idle:
        gain_ = 0.0f;
        currentNote_ = -1;
        break;
    case Stage::Attack:
        gain_ += attackStep_;
        if (gain_ >= 1.0f) {
            gain_ = 1.0f;
            stage_ = Stage::Hold;
        }
        break;
    case Stage::Hold:
        gain_ = 1.0f;
        break;
    case Stage::Release:
        gain_ -= releaseStep_;
        if (gain_ <= 0.0f) {
            gain_ = 0.0f;
            stage_ = Stage::Idle;
            currentNote_ = -1;
        }
        break;
    }
    return gain_;
}

} // namespace bdmcs48::instrument
