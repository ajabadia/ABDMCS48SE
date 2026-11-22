// TestOpcodeTable.cpp - La tabla de OpcodeNames contra la CPU que la ejecuta.
//
// La tabla describe nombre, longitud y clasificacion de los 256 opcodes. Aqui
// no se compara con una lista escrita a mano: se ejecuta cada opcode en la CPU
// real y se exige que la tabla diga lo mismo. Esta parte corre en CI sin
// material externo; el contraste con firmware real (listado del Juno-6) vive en
// `tests/TestJuno6DecodingConformance.cpp` y necesita el listado privado.

#include "TestFramework.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Mcs48Cpu.h"
#include "core/OpcodeNames.h"
#include "core/ProgramMemory.h"

namespace {

using bdmcs48::core::CpuFault;
using bdmcs48::core::Mcs48Cpu;
using bdmcs48::core::OpcodeInfo;
using bdmcs48::core::OpcodeTable;
using bdmcs48::core::PortBus;
using bdmcs48::core::ProgramMemory;

constexpr std::uint16_t kOpcodeAddress = 0x002;

class QuietBus : public PortBus {};

struct Run {
    CpuFault fault = CpuFault::None;
    std::uint16_t pc = 0;
};

// Ejecuta el opcode en 0x002 dejando antes un nivel de pila (CALL), igual que
// TestOpcodeCoverage: asi RET/RETR no fallan por pila vacia.
Run run(std::uint8_t opcode, std::uint8_t argument) {
    ProgramMemory memory;
    std::vector<std::uint8_t> program(8, 0x00);
    program[0] = 0x14; // CALL 0x002
    program[1] = 0x02;
    program[2] = opcode;
    program[3] = argument;
    CHECK(memory.loadBank(0, 0, program.data(), program.size()));

    QuietBus bus;
    Mcs48Cpu cpu(memory, &bus);
    cpu.step();
    cpu.step();

    Run result;
    result.fault = cpu.fault();
    result.pc = cpu.programCounter();
    return result;
}

} // namespace

TEST(OpcodeTable, FilasYAccesoresEnOrden) {
    // El contador es int: con std::uint8_t la condicion `opcode < 256` seria
    // siempre cierta y el bucle no terminaria nunca.
    const std::vector<OpcodeInfo> rows = OpcodeTable::all();
    CHECK_EQ(rows.size(), std::size_t{256});
    for (int value = 0; value < 256; ++value) {
        const auto opcode = static_cast<std::uint8_t>(value);
        const OpcodeInfo& row = OpcodeTable::get(opcode);
        CHECK_EQ(static_cast<std::size_t>(row.opcode), static_cast<std::size_t>(value));
        CHECK(std::string(row.name).size() > 0);
        CHECK(std::string(row.mnemonic).size() > 0);
        CHECK(row.length == 1 || row.length == 2);

        // La API de consulta que usan el desensamblador y las herramientas
        // debe devolver exactamente la misma fila.
        CHECK(std::string(OpcodeTable::nameOf(opcode)) == std::string(row.name));
        CHECK(std::string(OpcodeTable::mnemonicOf(opcode)) == std::string(row.mnemonic));
        CHECK_EQ(OpcodeTable::lengthOf(opcode), row.length);
        CHECK_EQ(OpcodeTable::isTwoBytes(opcode), row.length == 2);
        CHECK_EQ(OpcodeTable::isIllegal(opcode), row.isIllegal);

        // `isIllegal` es un hueco de la ISA que el hardware no asigna;
        // `!isImplemented` es una instruccion real que el core aun no modela.
        // Una fila no puede ser las dos cosas.
        if (row.isIllegal) {
            CHECK(!row.isImplemented);
        }
    }
}

TEST(OpcodeTable, LaClasificacionCoincideConLaCpu) {
    int implemented = 0;
    int unsupported = 0;
    int illegal = 0;
    for (int value = 0; value < 256; ++value) {
        const auto opcode = static_cast<std::uint8_t>(value);
        const OpcodeInfo& row = OpcodeTable::get(opcode);
        const Run result = run(opcode, 0x00);
        if (row.isIllegal) {
            ++illegal;
            CHECK_EQ(static_cast<int>(result.fault), static_cast<int>(CpuFault::IllegalOpcode));
            continue;
        }
        if (!row.isImplemented) {
            ++unsupported;
            CHECK_EQ(static_cast<int>(result.fault),
                     static_cast<int>(CpuFault::UnsupportedFeature));
            continue;
        }
        ++implemented;
        CHECK_EQ(static_cast<int>(result.fault), static_cast<int>(CpuFault::None));
    }
    CHECK_EQ(implemented, 223);
    CHECK_EQ(unsupported, 7);
    CHECK_EQ(illegal, 26);
}

TEST(OpcodeTable, LaLongitudCoincideConElAvanceDePc) {
    // Sin ambiguedad solo se puede medir en las instrucciones que no desvian el
    // flujo: ahi el PC avanzado es exactamente la longitud. Los saltos y
    // llamadas validan su longitud contra el listado golden del Juno-6.
    for (int value = 0; value < 256; ++value) {
        const auto opcode = static_cast<std::uint8_t>(value);
        const OpcodeInfo& row = OpcodeTable::get(opcode);
        if (row.isIllegal || !row.isImplemented || row.isBranch || row.isCall) {
            continue;
        }
        const Run result = run(opcode, 0x00);
        CHECK_EQ(static_cast<int>(result.fault), static_cast<int>(CpuFault::None));
        CHECK_EQ(static_cast<int>(result.pc), static_cast<int>(kOpcodeAddress + row.length));
    }
}

TEST(OpcodeTable, LosSaltosCondicionalesConsumenSuArgumento) {
    // Si la tabla declarase 1 byte para un salto de 2, el argumento se
    // ejecutaria como instruccion. Con 0x77 el PC no puede acabar en 0x003 ni
    // saltando (0x077) ni cayendo al opcode siguiente (0x004).
    for (int value = 0; value < 256; ++value) {
        const auto opcode = static_cast<std::uint8_t>(value);
        const OpcodeInfo& row = OpcodeTable::get(opcode);
        if (!row.isConditional || !row.isImplemented) {
            continue;
        }
        CHECK_EQ(row.length, std::uint8_t{2});
        const Run result = run(opcode, 0x77);
        CHECK_EQ(static_cast<int>(result.fault), static_cast<int>(CpuFault::None));
        CHECK(result.pc != kOpcodeAddress + 1);
    }
}

