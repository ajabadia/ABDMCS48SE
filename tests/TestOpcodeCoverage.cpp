// TestOpcodeCoverage.cpp - Matriz de cobertura de los 256 opcodes.
//
// No basta con una lista escrita a mano: el test ejecuta cada opcode y
// comprueba en que grupo cae. Si alguien deja de implementar una familia
// (o implementa una que debia fallar) el recuento y la lista cambian y el
// test lo dice. Las listas esperadas salen de la ISA del MCS-48:
//
//   * 0x05, 0x15, 0x16, 0x25, 0x35, 0x75, 0x86: interrupciones (EN I,
//     DIS I, JTF, EN/DIS TCNTI, ENT0 CLK, JNI). El contador del temporizador
//     (0x42, 0x45, 0x55, 0x62, 0x65) si esta implementado.
//   * el resto de huecos son bytes que la ISA no asigna.
//
// Cada opcode se ejecuta con un nivel de pila ya empujado (CALL previo) para
// que RET y RETR no se confundan con el caso de pila vacia, que tiene su
// propio test en TestMcs48Faults.

#include "TestFramework.h"

#include <cstdint>
#include <string>
#include <vector>

#include "core/Mcs48Cpu.h"
#include "core/ProgramMemory.h"

namespace {

using bdmcs48::core::CpuFault;
using bdmcs48::core::Mcs48Cpu;
using bdmcs48::core::PortBus;
using bdmcs48::core::ProgramMemory;

class QuietBus : public PortBus {};

// Ejecuta `opcode` en la direccion 0x002 despues de un CALL que deja un
// nivel en la pila. Devuelve el fallo con el que termina la CPU.
CpuFault faultOf(std::uint8_t opcode) {
    ProgramMemory memory;
    std::vector<std::uint8_t> program(8, 0x00);
    program[0] = 0x14; // CALL 0x002
    program[1] = 0x02;
    program[2] = opcode;
    if (!memory.loadBank(0, 0, program.data(), program.size())) {
        return CpuFault::IllegalOpcode;
    }

    QuietBus bus;
    Mcs48Cpu cpu(memory, &bus);
    cpu.step(); // CALL: un nivel de pila
    cpu.step(); // el opcode bajo prueba
    return cpu.fault();
}

std::string hexList(const std::vector<std::uint8_t>& opcodes) {
    static const char* kHex = "0123456789ABCDEF";
    std::string text;
    for (std::size_t i = 0; i < opcodes.size(); ++i) {
        if (i != 0) {
            text += ' ';
        }
        text += "0x";
        text += kHex[(opcodes[i] >> 4) & 0x0f];
        text += kHex[opcodes[i] & 0x0f];
    }
    return text;
}

struct Survey {
    std::vector<std::uint8_t> executes;
    std::vector<std::uint8_t> unsupported;
    std::vector<std::uint8_t> illegal;
};

Survey surveyOpcodes() {
    Survey survey;
    for (int value = 0; value < 256; ++value) {
        const auto opcode = static_cast<std::uint8_t>(value);
        switch (faultOf(opcode)) {
        case CpuFault::None:
            survey.executes.push_back(opcode);
            break;
        case CpuFault::UnsupportedFeature:
            survey.unsupported.push_back(opcode);
            break;
        case CpuFault::IllegalOpcode:
        case CpuFault::StackOverflow:
        case CpuFault::StackUnderflow:
            survey.illegal.push_back(opcode);
            break;
        }
    }
    return survey;
}

} // namespace

TEST(OpcodeCoverage, LosTresGruposCubrenLos256Opcodes) {
    const Survey survey = surveyOpcodes();
    CHECK_EQ(survey.executes.size() + survey.unsupported.size() + survey.illegal.size(),
             std::size_t{256});
    CHECK_EQ(survey.executes.size(), std::size_t{223});
    CHECK_EQ(survey.unsupported.size(), std::size_t{7});
    CHECK_EQ(survey.illegal.size(), std::size_t{26});
}

TEST(OpcodeCoverage, SoloLasInterrupcionesFallanComoNoSoportadas) {
    const Survey survey = surveyOpcodes();
    CHECK_EQ(hexList(survey.unsupported),
             std::string("0x05 0x15 0x16 0x25 0x35 0x75 0x86"));
}

TEST(OpcodeCoverage, LosHuecosDeLaIsaSonOpcodeIlegal) {
    // Lista contrastada con la tabla de opcodes de MAME
    // (docs/mame/mcs48.cpp): son los bytes que el 8048/8049 no asigna.
    const Survey survey = surveyOpcodes();
    CHECK_EQ(hexList(survey.illegal),
             std::string("0x01 0x06 0x0B 0x22 0x33 0x38 0x3B 0x63 0x66 0x73 0x82 0x87 "
                         "0x8B 0x9B 0xA2 0xA6 0xB7 0xC0 0xC1 0xC2 0xC3 0xD6 0xE0 0xE1 "
                         "0xE2 0xF3"));
}

TEST(OpcodeCoverage, CplCNoEsMovPsw) {
    // 0xA7 es CPL C. Alguna version del core lo trataba como MOV PSW,A, que
    // es 0xD7: aqui se fija la semantica correcta.
    ProgramMemory memory;
    const std::uint8_t program[] = {0xa7, 0x00};
    CHECK(memory.loadBank(0, 0, program, sizeof(program)));

    QuietBus bus;
    Mcs48Cpu cpu(memory, &bus);
    CHECK_EQ(cpu.carry(), false);
    cpu.step();
    CHECK_EQ(cpu.carry(), true);
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
    // El acumulador manda sobre el PSW solo en 0xD7; aqui no se toca.
    CHECK_EQ(cpu.psw() & 0xf0, std::uint8_t{0x80});
}
