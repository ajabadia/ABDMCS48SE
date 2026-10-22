// Sha256.h - SHA-256 (FIPS 180-4) para verificar las imagenes del paquete.
// El hash demuestra que los bytes son los que el manifiesto declara: NO
// demuestra autenticidad, procedencia, licencia ni permiso de redistribucion.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace bdmcs48::firmware {
class Sha256 {
public:
    Sha256() { reset(); }
    void reset();
    void update(const void* data, std::size_t size);
    void update(const std::string& text) { update(text.data(), text.size()); }
    std::string hexDigest() const;
    static std::string hexOf(const void* data, std::size_t size);
    static std::string hexOf(const char* text);
    static std::string hexOf(const std::string& text);
    static std::string hexOf(const std::vector<std::uint8_t>& data);
    static bool matches(const void* data, std::size_t size, const std::string& expected);
    static bool matches(const std::vector<std::uint8_t>& data, const std::string& expected);
private:
    void transform(const std::uint8_t block[64]);
    std::uint32_t state_[8]{};
    std::uint64_t bitCount_ = 0;
    std::uint8_t buffer_[64]{};
    std::size_t bufferSize_ = 0;
};
} // namespace bdmcs48::firmware
