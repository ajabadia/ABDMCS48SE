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
    words[3] = floatToBits(snapshot.outputLevel);

    // El mensaje travels en 5 palabras de 8 caracteres.
    const std::size_t messageWords = (kMessageChars + 7) / 8;
    std::memset(words + 4, 0, messageWords * sizeof(std::uint64_t));
    const std::size_t length = snapshot.message.size() < kMessageChars ? snapshot.message.size() : kMessageChars;
    for (std::size_t i = 0; i < length; ++i) {
        const std::size_t word = i / 8;
        const std::size_t byte = i % 8;
        words[4 + word] |= static_cast<std::uint64_t>(static_cast<unsigned char>(snapshot.message[i])) << (byte * 8);
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
    out.outputLevel = bitsToFloat(static_cast<std::uint32_t>(words[3]));

    out.message.clear();
    const std::size_t messageWords = (kMessageChars + 7) / 8;
    for (std::size_t w = 0; w < messageWords; ++w) {
        for (std::size_t byte = 0; byte < 8; ++byte) {
            const char character = static_cast<char>((words[4 + w] >> (byte * 8)) & 0xffu);
            if (character == '\0') {
                return;
            }
            out.message.push_back(character);
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
