// TestMcs48Faults.cpp - Fallos explicitos de la CPU.
//
// El core no adivina: cuando el firmware hace algo que el subconjunto no
// implementa, o que el hardware real no admitiria (pila de 8 niveles
// desbordada, RET sin retorno pendiente), la CPU se detiene con un fallo
// concreto en lugar de seguir con un estado inventado.

#include "TestFramework.h"

#include <cstdint>

#include "core/Mcs48Cpu.h"
#include "core/ProgramMemory.h"

namespace {

using bdmcs48::core::CpuFault;
using bdmcs48::core::Mcs48Cpu;
using bdmcs48::core::PortBus;
using bdmcs48::core::ProgramMemory;

class SilentBus : public PortBus {};

ProgramMemory programOf(const std::uint8_t* bytes, std::size_t size) {
    ProgramMemory memory;
    CHECK(memory.loadBank(0, 0, bytes, size));
    return memory;
}

} // namespace

TEST(Mcs48Faults, UnOpcodeIlegalDetieneLaCpu) {
    const std::uint8_t program[] = {0x01, 0x00};
    ProgramMemory memory = programOf(program, sizeof(program));
    SilentBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::IllegalOpcode));
    CHECK(!cpu.faultMessage().empty());

    // Un fallo es terminal: ni el contador ni el reloj avanzan mas.
    const std::uint16_t pcAtFault = cpu.programCounter();
    const std::uint64_t cyclesAtFault = cpu.cycles();
    cpu.step();
    cpu.runCycles(32);
    CHECK_EQ(cpu.programCounter(), pcAtFault);
    CHECK_EQ(cpu.cycles(), cyclesAtFault);
}

TEST(Mcs48Faults, LasInterrupcionesFallaDeFormaExplicita) {
    // El contador del temporizador si esta modelado; lo que no existe es el
    // despacho de interrupciones, que empieza por EN TCNTI.
    const std::uint8_t program[] = {0x25, 0x00}; // EN TCNTI
    ProgramMemory memory = programOf(program, sizeof(program));
    SilentBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::UnsupportedFeature));
    CHECK(cpu.faultMessage().find("interrupciones") != std::string::npos);
}

TEST(Mcs48Faults, LaPilaDeOchoNivelesNoSeDesbordaEnSilencio) {
    // 0x000: CALL 0x000 (recursion sin fin). Los primeros 8 CALL llenan la
    // pila; el noveno es un error de firmware que el hardware real no
    // detecta (el puntero de 3 bits da la vuelta). Aqui se avisa.
    const std::uint8_t program[] = {0x14, 0x00, 0x00};
    ProgramMemory memory = programOf(program, sizeof(program));
    SilentBus bus;
    Mcs48Cpu cpu(memory, &bus);

    for (int level = 0; level < 8; ++level) {
        cpu.step();
        CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
    }
    CHECK_EQ(cpu.psw() & 0x07, std::uint8_t{0}); // 8 niveles = puntero otra vez a 0

    cpu.step(); // noveno CALL
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::StackOverflow));
}

TEST(Mcs48Faults, RetSinRetornoPendienteFalla) {
    const std::uint8_t program[] = {0x83, 0x00}; // RET
    ProgramMemory memory = programOf(program, sizeof(program));
    SilentBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::StackUnderflow));
}

TEST(Mcs48Faults, RetrSinRetornoPendienteFalla) {
    const std::uint8_t program[] = {0x93, 0x00}; // RETR
    ProgramMemory memory = programOf(program, sizeof(program));
    SilentBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::StackUnderflow));
}

TEST(Mcs48Faults, ResetLimpiaElFallo) {
    const std::uint8_t program[] = {0x01, 0x00};
    ProgramMemory memory = programOf(program, sizeof(program));
    SilentBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK(cpu.faulted());
    cpu.reset();
    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0});
    CHECK_EQ(cpu.cycles(), std::uint64_t{0});
}
