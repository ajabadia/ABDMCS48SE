#include "firmware/Sha256.h"

#include <cstring>

namespace bdmcs48::firmware {
namespace {

constexpr std::uint32_t kRoundConstants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u,
};

inline std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
}

} // namespace

void Sha256::reset() {
    state_[0] = 0x6a09e667u;
    state_[1] = 0xbb67ae85u;
    state_[2] = 0x3c6ef372u;
    state_[3] = 0xa54ff53au;
    state_[4] = 0x510e527fu;
    state_[5] = 0x9b05688cu;
    state_[6] = 0x1f83d9abu;
    state_[7] = 0x5be0cd19u;
    bitCount_ = 0;
    bufferSize_ = 0;
}

void Sha256::transform(const std::uint8_t block[64]) {
    std::uint32_t w[64];
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<std::uint32_t>(block[i * 4 + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];

    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
        const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    bitCount_ += static_cast<std::uint64_t>(size) * 8;

    while (size > 0) {
        const std::size_t chunk = std::min(size, std::size_t{64} - bufferSize_);
        std::memcpy(buffer_ + bufferSize_, bytes, chunk);
        bufferSize_ += chunk;
        bytes += chunk;
        size -= chunk;

        if (bufferSize_ == 64) {
            transform(buffer_);
            bufferSize_ = 0;
        }
    }
}

std::string Sha256::hexDigest() const {
    // Se opera sobre una copia: el digest no debe alterar el estado.
    Sha256 copy = *this;

    const std::uint64_t bitCount = copy.bitCount_;
    const std::uint8_t padding = 0x80;
    copy.update(&padding, 1);
    copy.bitCount_ = bitCount; // el relleno no cuenta como datos

    const std::uint8_t zero = 0x00;
    while (copy.bufferSize_ != 56) {
        copy.update(&zero, 1);
        copy.bitCount_ = bitCount;
    }

    std::uint8_t lengthBytes[8];
    for (std::size_t i = 0; i < 8; ++i) {
        lengthBytes[i] = static_cast<std::uint8_t>((bitCount >> (56 - i * 8)) & 0xffu);
    }
    copy.update(lengthBytes, 8);

    static const char* kHex = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (std::size_t i = 0; i < 8; ++i) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result.push_back(kHex[(copy.state_[i] >> shift) & 0x0fu]);
        }
    }
    return result;
}

std::string Sha256::hexOf(const void* data, std::size_t size) {
    Sha256 hash;
    hash.update(data, size);
    return hash.hexDigest();
}

std::string Sha256::hexOf(const std::vector<std::uint8_t>& data) {
    return hexOf(data.data(), data.size());
}

bool Sha256::matches(const void* data, std::size_t size, const std::string& expected) {
    if (expected.size() != 64) {
        return false;
    }
    const std::string actual = hexOf(data, size);
    // Comparacion sin dependencias del tiempo: no es un secreto, pero es
    // gratuita y evita que alguien la copie mal mas adelante.
    unsigned char difference = 0;
    for (std::size_t i = 0; i < 64; ++i) {
        difference |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    return difference == 0;
}

bool Sha256::matches(const std::vector<std::uint8_t>& data, const std::string& expected) {
    return matches(data.data(), data.size(), expected);
}

} // namespace bdmcs48::firmware
