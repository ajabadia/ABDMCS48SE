#include "TestFramework.h"

#include <vector>

#include "core/Mcs48Cpu.h"
#include "core/ProgramMemory.h"

using bdmcs48::core::CpuFault;
using bdmcs48::core::Mcs48Cpu;
using bdmcs48::core::PortBus;
using bdmcs48::core::ProgramMemory;

namespace {

class RecordingBus : public PortBus {
public:
    std::uint8_t ports[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::vector<std::uint8_t> writes;
    std::uint8_t external[16] = {};

    std::uint8_t readPort(std::uint8_t port) override { return ports[port & 0x07]; }

    void writePort(std::uint8_t port, std::uint8_t value) override {
        ports[port & 0x07] = value;
        writes.push_back(value);
    }

    std::uint8_t readExternal(std::uint8_t address) override { return external[address & 0x0f]; }

    void writeExternal(std::uint8_t address, std::uint8_t value) override {
        external[address & 0x0f] = value;
    }
};

ProgramMemory makeProgram(const std::vector<std::uint8_t>& code) {
    ProgramMemory memory;
    memory.loadBank(0, 0, code.data(), code.size());
    return memory;
}

} // namespace

TEST(Mcs48Cpu, NopCuestaUnCiclo) {
    ProgramMemory program = makeProgram({0x00});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    CHECK_EQ(cpu.cycles(), std::uint64_t{1});
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x001});
}

TEST(Mcs48Cpu, MovInmediatoCuestaDosCiclos) {
    ProgramMemory program = makeProgram({0x23, 0x5a}); // MOV A,#0x5A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    CHECK_EQ(cpu.cycles(), std::uint64_t{2});
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x5a});
}

TEST(Mcs48Cpu, AddPropagaAcento) {
    ProgramMemory program = makeProgram({0x23, 0xff, 0x03, 0x01}); // MOV A,#0xFF ; ADD A,#1
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00});
    CHECK(cpu.carry());
}

TEST(Mcs48Cpu, AritmeticaBasica) {
    ProgramMemory program = makeProgram({0x27, 0x17, 0x07, 0x37, 0x47}); // CLR A; INC A; DEC A; CPL A; SWAP A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // CLR A  -> 0
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00});
    cpu.step(); // INC A  -> 1
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x01});
    cpu.step(); // DEC A  -> 0
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00});
    cpu.step(); // CPL A  -> 0xFF
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0xff});
    cpu.step(); // SWAP A -> 0xFF
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0xff});
}

TEST(Mcs48Cpu, OutlP1EscribeEnElPuerto) {
    ProgramMemory program = makeProgram({0x23, 0x42, 0x39}); // MOV A,#0x42 ; OUTL P1,A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    CHECK_EQ(bus.ports[1], std::uint8_t{0x42});
    CHECK_EQ(bus.writes.size(), std::size_t{1});
}

TEST(Mcs48Cpu, JmpSaltaEnLaPaginaActual) {
    ProgramMemory program = makeProgram({0x04, 0x10, 0x00}); // JMP 0x010
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x010});
}

TEST(Mcs48Cpu, SelMb1MasJmpCambiaDeBanco) {
    // SEL MB1 ; JMP 0x010  ->  0x810
    ProgramMemory program = makeProgram({0xf5, 0x04, 0x10});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // SEL MB1
    CHECK_EQ(cpu.bank(), std::uint8_t{1});
    cpu.step(); // JMP 0x010
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x810});
}

TEST(Mcs48Cpu, ElSaltoCondicionalNoCambiaDePagina) {
    // 0x000: SEL MB1 ; JMP 0x000  -> instala el flip-flop y salta a 0x800
    // 0x800: JZ 0x00   (con A = 0 el salto se cumple)
    // Si el salto condicional respetase el flip-flop de banco, el PC se iria
    // a 0x000. El modelo correcto lo deja en 0x800: la pagina se conserva.
    ProgramMemory program = makeProgram({0xf5, 0x04, 0x00});
    const std::uint8_t bank1[] = {0xc6, 0x00, 0x00, 0x00};
    CHECK(program.loadBank(1, 0, bank1, sizeof(bank1)));

    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // SEL MB1
    cpu.step(); // JMP 0x000 -> 0x800
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x800});
    cpu.step(); // JZ 0x00: se queda en la pagina 8, no baja a 0x000
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x800});
}

TEST(Mcs48Cpu, DjnzDecrementaYRepite) {
    // MOV R0,#3 ; DJNZ R0,0x02 (a si mismo) ; MOV A,#0x55
    ProgramMemory program = makeProgram({0xb8, 0x03, 0xe8, 0x02, 0x23, 0x55});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // MOV R0,#3
    cpu.step(); // DJNZ R0,0x02 -> R0 = 2, salta a 0x002
    CHECK_EQ(cpu.readRegister(0), std::uint8_t{2});
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x002});
    cpu.step(); // DJNZ R0,0x00 -> R0 = 1
    cpu.step(); // DJNZ R0,0x00 -> R0 = 0, no salta
    CHECK_EQ(cpu.readRegister(0), std::uint8_t{0});
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x004});
    cpu.step(); // MOV A,#0x55
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x55});
}

TEST(Mcs48Cpu, CallYRetRestauranElContador) {
    // 0x000: CALL 0x010
    // 0x010: RET
    ProgramMemory program = makeProgram({0x14, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // CALL 0x010
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x010});
    cpu.step(); // RET
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x002});
}

TEST(Mcs48Cpu, RetrRestauraElPsw) {
    // 0x000: SEL RB1 ; 0x001-0x002: CALL 0x010 (el retorno es 0x003)
    // 0x010: SEL RB0 ; RETR
    ProgramMemory program = makeProgram({0xd5, 0x14, 0x10});
    const std::uint8_t tail[] = {0xc5, 0x93};
    CHECK(program.loadBank(0, 0x10, tail, sizeof(tail)));

    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // SEL RB1
    CHECK((cpu.psw() & Mcs48Cpu::kBankFlag) != 0);
    cpu.step(); // CALL 0x010
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x010});
    cpu.step(); // SEL RB0: se limpia el flag
    CHECK((cpu.psw() & Mcs48Cpu::kBankFlag) == 0);
    cpu.step(); // RETR: lo restaura
    CHECK((cpu.psw() & Mcs48Cpu::kBankFlag) != 0);
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x003});
}

TEST(Mcs48Cpu, LosBancosDeRegistrosSonDistintos) {
    ProgramMemory program = makeProgram({0xb8, 0x11, 0xd5, 0xb8, 0x22, 0xc5, 0xd5});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // MOV R0,#0x11 (banco 0)
    cpu.step(); // SEL RB1
    cpu.step(); // MOV R0,#0x22 (banco 1)
    cpu.step(); // SEL RB0
    CHECK_EQ(cpu.readRegister(0), std::uint8_t{0x11});
    cpu.step(); // SEL RB1
    CHECK_EQ(cpu.readRegister(0), std::uint8_t{0x22});
}

TEST(Mcs48Cpu, OpcodeNoImplementadoFallaDeFormaExplicita) {
    ProgramMemory program = makeProgram({0x25}); // EN TCNTI: temporizador no implementado
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    CHECK(cpu.faulted());
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::UnsupportedFeature));
    CHECK(!cpu.faultMessage().empty());
}

TEST(Mcs48Cpu, OpcodeIlegibleFalla) {
    // 0x01 no esta asignado en el MCS-48 (0xFF si lo esta: MOV A,R7).
    ProgramMemory program = makeProgram({0x01, 0x00, 0x00, 0x00});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    CHECK(cpu.faulted());
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::IllegalOpcode));
}

TEST(Mcs48Cpu, ElBusExternoLeeYEscribe) {
    // MOV R0,#0x03 ; MOVX A,@R0
    ProgramMemory program = makeProgram({0xb8, 0x03, 0x80});
    RecordingBus bus;
    bus.external[3] = 0x7e;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x7e});
}

TEST(Mcs48Cpu, ElFetchSecuencialConservaElBitDeBanco) {
    // SEL MB1 ; JMP 0x000 -> 0x800, y ahi hay NOPs: el PC no puede volver
    // a 0x000 por mucho que el fetch sea secuencial.
    ProgramMemory program = makeProgram({0xf5, 0x04, 0x00});
    const std::uint8_t bank1[] = {0x00, 0x00, 0x00};
    CHECK(program.loadBank(1, 0, bank1, sizeof(bank1)));
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // SEL MB1
    cpu.step(); // JMP 0x000 -> 0x800
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x800});
    cpu.step();
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x801});
    cpu.step();
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x802});
}

TEST(Mcs48Cpu, RunCyclesRespetaElPresupuesto) {
    ProgramMemory program = makeProgram({0x00, 0x00, 0x00, 0x00});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.runCycles(10);
    CHECK_EQ(cpu.cycles(), std::uint64_t{10});
}
