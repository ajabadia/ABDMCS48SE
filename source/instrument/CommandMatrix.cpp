#include "instrument/CommandMatrix.h"

#include <algorithm>
#include <cmath>

namespace bdmcs48::instrument {
namespace {

// MachineHost reparte los comandos como valor / 32767, tanto para parametros
// como para entradas. La matriz hace la conversion inversa.
constexpr float kCommandScale = 32'767.0f;

} // namespace

std::array<MatrixRow, CommandMatrix::kRows> CommandMatrix::defaults() {
    std::array<MatrixRow, kRows> rows{};
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].note = 60 + static_cast<int>(i);
        rows[i].enabled = i == 0;
        rows[i].target = MatrixTarget::EngineClock;
        rows[i].value = 1.0f;
        rows[i].currentValue = 1.0f;
        rows[i].probability = 1.0f;
        rows[i].delayMs = 0.0f;
        rows[i].mutation = MatrixMutation::None;
        rows[i].mutationAmount = 0.0f;
    }
    return rows;
}

CommandMatrix::CommandMatrix()
    : rows_(defaults()) {}

CommandMatrix::CommandMatrix(double sampleRateHz)
    : rows_(defaults()) {
    prepare(sampleRateHz);
}

void CommandMatrix::prepare(double sampleRateHz) {
    sampleRate_ = sampleRateHz > 0.0 ? sampleRateHz : 44'100.0;
}

void CommandMatrix::reset() {
    rows_ = defaults();
    clearSchedule();
    frame_ = 0;
    dropped_ = 0;
    randomState_ = seed_;
}

void CommandMatrix::clearSchedule() {
    scheduleCount_ = 0;
}

void CommandMatrix::setSeed(std::uint32_t seed) {
    seed_ = seed == 0 ? 1u : seed;
    randomState_ = seed_;
}

float CommandMatrix::minFor(MatrixTarget target) {
    return target == MatrixTarget::EngineClock ? kEngineClockMinScale : kLatchMin;
}

float CommandMatrix::maxFor(MatrixTarget target) {
    return target == MatrixTarget::EngineClock ? kEngineClockMaxScale : kLatchMax;
}

float CommandMatrix::clampForTarget(MatrixTarget target, float value) {
    return std::clamp(value, minFor(target), maxFor(target));
}

bool CommandMatrix::rowMatches(const MatrixRow& row, int note) {
    return row.enabled && row.note == note;
}

void CommandMatrix::noteOn(int note) {
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        auto& row = rows_[i];
        if (!rowMatches(row, note)) {
            continue;
        }

        // La probabilidad se decide al recibir la nota, no al aplicar el
        // retardo. Un valor a 0 no consume aleatoriedad, para que la semilla
        // siga siendo reproducible.
        if (row.probability <= 0.0f) {
            continue;
        }
        if (row.probability < 1.0f && nextRandom() > row.probability) {
            continue;
        }

        const double delaySamples = std::max(0.0, static_cast<double>(row.delayMs) / 1000.0) *
                                    sampleRate_;
        const auto delay = static_cast<std::uint64_t>(std::llround(delaySamples));

        if (scheduleCount_ >= schedule_.size()) {
            ++dropped_;
            continue;
        }
        schedule_[scheduleCount_++] = Scheduled{frame_ + delay, static_cast<std::uint8_t>(i)};
    }
}

void CommandMatrix::publishRows(const std::array<MatrixRow, kRows>& rows) {
    const std::uint32_t next = 1u - publishedIndex_.load(std::memory_order_relaxed);
    pendingRows_[next] = rows;
    publishedIndex_.store(next, std::memory_order_release);
    hasPending_.store(true, std::memory_order_release);
}

bool CommandMatrix::consumePublishedRows() {
    if (!hasPending_.load(std::memory_order_acquire)) {
        return false;
    }
    const std::uint32_t index = publishedIndex_.load(std::memory_order_acquire);
    rows_ = pendingRows_[index];
    hasPending_.store(false, std::memory_order_release);
    return true;
}

std::uint64_t CommandMatrix::nextDeadline() const {
    std::uint64_t earliest = 0;
    for (std::size_t i = 0; i < scheduleCount_; ++i) {
        const auto deadline = schedule_[i].deadline;
        if (earliest == 0 || deadline < earliest) {
            earliest = deadline;
        }
    }
    return earliest;
}

bool CommandMatrix::popDue(Command& out) {
    std::size_t best = scheduleCount_;
    for (std::size_t i = 0; i < scheduleCount_; ++i) {
        if (schedule_[i].deadline > frame_) {
            continue;
        }
        if (best == scheduleCount_ || schedule_[i].deadline < schedule_[best].deadline) {
            best = i;
        }
    }
    if (best == scheduleCount_) {
        return false;
    }

    const std::size_t rowIndex = schedule_[best].row;
    schedule_[best] = schedule_[scheduleCount_ - 1];
    --scheduleCount_;

    // Primero se calcula el comando con el valor vigente y despues se muta:
    // la mutacion afecta a la SIGUIENTE ejecucion, no a esta.
    out = commandFor(rows_[rowIndex]);
    mutate(rows_[rowIndex]);
    return true;
}

CommandMatrix::Command CommandMatrix::commandFor(const MatrixRow& row) {
    const float clamped = clampForTarget(row.target, row.currentValue);
    const float span = maxFor(row.target) - minFor(row.target);
    const float normalized = span > 0.0f ? (clamped - minFor(row.target)) / span : 0.0f;

    Command command;
    command.id = row.target == MatrixTarget::EngineClock ? kEngineClockCommandId
                                                          : kLatchWriteCommandId;
    command.value = static_cast<std::int32_t>(std::lround(normalized * kCommandScale));
    return command;
}

void CommandMatrix::mutate(MatrixRow& row) {
    const float min = minFor(row.target);
    const float max = maxFor(row.target);
    const float amount = std::max(0.0f, row.mutationAmount);

    switch (row.mutation) {
    case MatrixMutation::None:
        return;
    case MatrixMutation::Increment:
        row.currentValue = std::clamp(row.currentValue + amount, min, max);
        return;
    case MatrixMutation::Decrement:
        row.currentValue = std::clamp(row.currentValue - amount, min, max);
        return;
    case MatrixMutation::Random:
        row.currentValue = min + nextRandom() * (max - min);
        return;
    case MatrixMutation::Wrap:
        row.currentValue += amount;
        if (row.currentValue > max) {
            row.currentValue = min;
        }
        return;
    }
}

float CommandMatrix::nextRandom() {
    // xorshift32: barato, determinista y suficiente para probabilidad y
    // mutacion. No se usa para nada criptografico.
    std::uint32_t x = randomState_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    randomState_ = x;
    return static_cast<float>(x) / static_cast<float>(0xffffffffu);
}

} // namespace bdmcs48::instrument
