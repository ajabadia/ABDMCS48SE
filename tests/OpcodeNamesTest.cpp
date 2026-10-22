#include "TestFramework.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "core/OpcodeNames.h"

using bdmcs48::core::OpcodeFormat;
using bdmcs48::core::OpcodeMnemonic;
using bdmcs48::core::OpcodeName;

TEST(OpcodeNames, TablaDebeTenerUnRegistroPorOpcode) {
    constexpr std::size_t expected = 256;
    CHECK_EQ(OpcodeName::all().size(), expected);
    CHECK_EQ(OpcodeFormat::all().size(), expected);
    CHECK_EQ(OpcodeMnemonic::all().size(), expected);
}

TEST(OpcodeNames, CadaOpcodeCuentaConNombreYMnemonic) {
    for (std::uint8_t opcode = 0; opcode < 256; ++opcode) {
        CHECK(OpcodeName::find(opcode) != nullptr);
        CHECK(OpcodeMnemonic::find(opcode) != nullptr);
        CHECK(OpcodeFormat::find(opcode) != nullptr);
    }
}

TEST(OpcodeNames, LosMnemonicNoSeDuplican) {
    auto mnemonics = OpcodeMnemonic::all();
    std::sort(mnemonics.begin(), mnemonics.end(),
              [](const OpcodeMnemonic& a, const OpcodeMnemonic& b) {
                  return a.mnemonic_ < b.mnemonic_;
              });
    for (std::size_t i = 1; i < mnemonics.size(); ++i) {
        CHECK(mnemonics[i].mnemonic_ != mnemonics[i - 1].mnemonic_);
    }
}

TEST(OpcodeNames, TodoElOpcodeImplementadoSePuedeObtenerPorNombre) {
    int missing = 0;
    for (std::uint8_t opcode = 0; opcode < 256; ++opcode) {
        const OpcodeName* entry = OpcodeName::find(opcode);
        if (!entry->implemented_) {
            continue;
        }
        const char* name = entry->name();
        CHECK(OpcodeName::byName(name) != nullptr);
    }
}

TEST(OpcodeNames, TablaPuedeEnumerarsePorMnemonico) {
    auto adds = OpcodeName::byMnemonic("ADD");
    CHECK_GT(adds.size(), 0u);
    for (const auto& entry : adds) {
        CHECK_EQ(entry.mnemonic(), "ADD");
    }
}

TEST(OpcodeNames, TablaPuedeEnumerarsePorFormato) {
    auto imm = OpcodeName::byFormat("IMM");
    CHECK_GT(imm.size(), 0u);
    for (const auto& entry : imm) {
        CHECK_EQ(entry.format(), "IMM");
    }
}

TEST(OpcodeNames, LosAliasesSeDevuelvenComoCadena) {
    const OpcodeName* entry = OpcodeName::find(0x23); // MOV A,#data
    CHECK(entry != nullptr);
    CHECK(entry->toAssembler() == "mov a,#data");
    CHECK(entry->assemblerList() == "mov a,#data");
}

TEST(OpcodeNames, LosMnemonicYFormatoSonConstantes) {
    const OpcodeMnemonic* m = OpcodeMnemonic::find(0x03);
    CHECK(m != nullptr);
    CHECK(m->implemented());
    CHECK_FALSE(OpcodeMnemonic::find(0xfe) != nullptr);

    const OpcodeFormat* f = OpcodeFormat::find(0x03);
    CHECK(f != nullptr);
    CHECK_EQ(f->format(), "IMM");
}

TEST(OpcodeNames, TablaEsCerrada) {
    const OpcodeName* last = OpcodeName::find(255);
    CHECK(last != nullptr);
    CHECK_EQ(last->name(), "orl a,#data");
    CHECK_EQ(last->format(), "IMM");
    CHECK_EQ(last->mnemonic(), "ORL");
}
