// ProgramMemory.h - Memoria de programa del MCS-48.
//
// El espacio de programa del MCS-48 tiene 12 bits de direccion (4 KiB). El bit
// A11 NO es un "selector de banco" global: es una linea de direccion que la
// CPU aplica en el momento de un JMP/CALL, segun el valor del flip-flop de
// banco que fija SEL MB0/SB1. Por eso la memoria se modela como un array
// plano de 4 KiB y el "banco" es simplemente el bit 11 de la direccion.
//
// Fuentes: Intel MCS-48 User's Manual y MAME (src/devices/cpu/mcs48).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bdmcs48::core {

class ProgramMemory {
public:
    static constexpr std::size_t kAddressBits = 12;
    static constexpr std::size_t kSize = 1u << kAddressBits;   // 4096 bytes
    static constexpr std::size_t kBankSize = kSize / 2;        // 2048 bytes
    static constexpr std::size_t kBankCount = 2;

    ProgramMemory() = default;

    void clear();

    // Carga `size` bytes en el banco indicado (0 -> 0x000-0x7FF,
    // 1 -> 0x800-0xFFF), dentro de la posicion `offset` del banco.
    // Devuelve false si el banco no existe o si los bytes no caben.
    bool loadBank(std::size_t bank, std::size_t offset, const std::uint8_t* data, std::size_t size);

    // Lectura por direccion de 12 bits. Las direcciones por encima de 4 KiB
    // se enmascaran (el bus de programa del MCS-48 tiene 12 lineas).
    std::uint8_t read(std::uint16_t address) const;

    // Direccion de 12 bits -> indice de banco (bit 11).
    static std::size_t bankOf(std::uint16_t address);

    bool isBankLoaded(std::size_t bank) const;
    std::size_t loadedBytes() const;

    const std::uint8_t* data() const { return bytes_.data(); }
    std::uint8_t* data() { return bytes_.data(); }

private:
    std::array<std::uint8_t, kSize> bytes_{};
    std::array<std::size_t, kBankCount> loaded_{0, 0};
};

} // namespace bdmcs48::core
