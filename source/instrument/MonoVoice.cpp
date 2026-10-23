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
}

void MonoVoice::reset() {
    held_.fill(-1);
    heldCount_ = 0;
    currentNote_ = -1;
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
    const double scale = std::clamp(clockScaleForNote(currentNote_),
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
    velocity_ = std::clamp(velocity, 0.0f, 1.0f);
    if (stage_ == Stage::Hold || stage_ == Stage::Attack) {
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
        if (stage_ == Stage::Release || stage_ == Stage::Idle) {
            startAttack();
        }
        return;
    }
    currentNote_ = -1;
    startRelease();
}

void MonoVoice::allNotesOff() {
    heldCount_ = 0;
    if (stage_ != Stage::Idle) {
        currentNote_ = -1;
        startRelease();
    }
}

float MonoVoice::nextGain() {
    switch (stage_) {
    case Stage::Idle:
        gain_ = 0.0f;
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
        }
        break;
    }
    return gain_;
}

} // namespace bdmcs48::instrument
