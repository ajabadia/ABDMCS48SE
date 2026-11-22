#include "core/Diagnostics.h"

#include <cstring>

namespace bdmcs48::core {
namespace {

std::uint32_t floatToBits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float bitsToFloat(std::uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace

void SnapshotChannel::encode(const MachineSnapshot& snapshot, std::uint64_t* words) const {
    words[0] = snapshot.frameCounter;
    words[1] = snapshot.cpuCycles;
    words[2] = static_cast<std::uint64_t>(snapshot.programCounter) |
               (static_cast<std::uint64_t>(snapshot.accumulator) << 16) |
               (static_cast<std::uint64_t>(snapshot.psw) << 24) |
               (static_cast<std::uint64_t>(snapshot.bank) << 32) |
               (static_cast<std::uint64_t>(snapshot.faulted) << 40);
    words[3] = static_cast<std::uint64_t>(floatToBits(snapshot.outputLevel)) |
               (static_cast<std::uint64_t>(floatToBits(snapshot.peakLevel)) << 32);
    words[4] = static_cast<std::uint64_t>(floatToBits(snapshot.rmsLevel));
    std::uint64_t waveWord = 0;
    std::memcpy(&waveWord, snapshot.waveform, sizeof(snapshot.waveform));
    words[5] = waveWord;

    // El mensaje viaja en 6 palabras (words[6..11])
    const std::size_t messageWords = (kMessageChars + 7) / 8;
    std::memset(words + 6, 0, messageWords * sizeof(std::uint64_t));
    const std::size_t length = std::strlen(snapshot.message);
    const std::size_t clamped = length < kMessageChars ? length : kMessageChars;
    for (std::size_t i = 0; i < clamped; ++i) {
        const std::size_t word = i / 8;
        const std::size_t byte = i % 8;
        words[6 + word] |= static_cast<std::uint64_t>(static_cast<unsigned char>(snapshot.message[i])) << (byte * 8);
    }
}

void SnapshotChannel::decode(const std::uint64_t* words, MachineSnapshot& out) const {
    out.frameCounter = words[0];
    out.cpuCycles = words[1];
    out.programCounter = static_cast<std::uint32_t>(words[2] & 0xffffu);
    out.accumulator = static_cast<std::uint8_t>((words[2] >> 16) & 0xffu);
    out.psw = static_cast<std::uint8_t>((words[2] >> 24) & 0xffu);
    out.bank = static_cast<std::uint8_t>((words[2] >> 32) & 0xffu);
    out.faulted = static_cast<std::uint8_t>((words[2] >> 40) & 0xffu);
    out.outputLevel = bitsToFloat(static_cast<std::uint32_t>(words[3] & 0xffffffffu));
    out.peakLevel = bitsToFloat(static_cast<std::uint32_t>(words[3] >> 32));
    out.rmsLevel = bitsToFloat(static_cast<std::uint32_t>(words[4] & 0xffffffffu));
    const std::uint64_t waveWord = words[5];
    std::memcpy(out.waveform, &waveWord, sizeof(out.waveform));

    std::memset(out.message, 0, sizeof(out.message));
    const std::size_t messageWords = (kMessageChars + 7) / 8;
    std::size_t idx = 0;
    for (std::size_t w = 0; w < messageWords; ++w) {
        for (std::size_t byte = 0; byte < 8; ++byte) {
            const char character = static_cast<char>((words[6 + w] >> (byte * 8)) & 0xffu);
            if (character == '\0' || idx >= sizeof(out.message) - 1) {
                out.message[idx] = '\0';
                return;
            }
            out.message[idx++] = character;
        }
    }
}

void SnapshotChannel::publish(const MachineSnapshot& snapshot) {
    std::uint64_t words[kWords] = {};

    const std::uint64_t sequence = sequence_.load(std::memory_order_relaxed);
    // Impar: hay una escritura en curso.
    sequence_.store(sequence + 1, std::memory_order_release);

    encode(snapshot, words);
    for (std::size_t i = 0; i < kWords; ++i) {
        words_[i].store(words[i], std::memory_order_relaxed);
    }

    // Par: la instantanea es coherente.
    sequence_.store(sequence + 2, std::memory_order_release);
    published_.fetch_add(1, std::memory_order_relaxed);
}

bool SnapshotChannel::read(MachineSnapshot& out) const {
    std::uint64_t words[kWords] = {};

    for (int attempt = 0; attempt < 8; ++attempt) {
        const std::uint64_t before = sequence_.load(std::memory_order_acquire);
        if ((before & 1u) != 0) {
            continue; // el escritor esta a mitad
        }
        for (std::size_t i = 0; i < kWords; ++i) {
            words[i] = words_[i].load(std::memory_order_relaxed);
        }
        const std::uint64_t after = sequence_.load(std::memory_order_acquire);
        if (before == after) {
            decode(words, out);
            return true;
        }
    }
    return false;
}

} // namespace bdmcs48::core
