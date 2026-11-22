#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bdmcs48::core {

/**
 * Informacion canonica de instruccion de la arquitectura MCS-48 (8048 / 8049 / 8039).
 * Derivada de la especificacion de Intel y de docs/mame/mcs48dsm.cpp.
 */
struct OpcodeInfo {
    std::uint8_t opcode = 0;
    const char* name = "";
    const char* mnemonic = "";
    std::uint8_t length = 1;
    bool isBranch = false;
    bool isCall = false;
    bool isConditional = false;
    bool isImplemented = true;
    bool isIllegal = false;
};

class OpcodeTable {
public:
    static const OpcodeInfo& get(std::uint8_t opcode) noexcept;
    static std::uint8_t lengthOf(std::uint8_t opcode) noexcept;
    static bool isTwoBytes(std::uint8_t opcode) noexcept;
    static bool isIllegal(std::uint8_t opcode) noexcept;
    static const char* nameOf(std::uint8_t opcode) noexcept;
    static const char* mnemonicOf(std::uint8_t opcode) noexcept;
    static std::vector<OpcodeInfo> all() noexcept;
};

} // namespace bdmcs48::core
