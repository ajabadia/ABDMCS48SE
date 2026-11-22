#include "TestFramework.h"

#include <fstream>
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

// ---------------------------------------------------------------------------
// Pruebas de conformidad Fase 11 y opcodes de firmware real (Juno-6 / MCS-48)
// ---------------------------------------------------------------------------

TEST(Mcs48Cpu, ClrACeroAcumulador) {
    // 0x27: CLR A
    ProgramMemory program = makeProgram({0x23, 0xaa, 0x27}); // MOV A,#0xAA ; CLR A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // MOV A,#0xAA
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0xaa});
    cpu.step(); // CLR A
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00});
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, CplAComplementaAcumulador) {
    // 0x37: CPL A
    ProgramMemory program = makeProgram({0x23, 0xaa, 0x37}); // MOV A,#0xAA ; CPL A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x55});
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, OutlP2EscribeEnPuerto2) {
    // 0x3A: OUTL P2,A
    ProgramMemory program = makeProgram({0x23, 0x7e, 0x3a}); // MOV A,#0x7E ; OUTL P2,A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    CHECK_EQ(bus.ports[2], std::uint8_t{0x7e});
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, MovdP4EscribeEnExpansor8243) {
    // 0x3C: MOVD P4,A
    ProgramMemory program = makeProgram({0x23, 0x0c, 0x3c}); // MOV A,#0x0C ; MOVD P4,A
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    CHECK_EQ(bus.ports[4], std::uint8_t{0x0c});
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, OrlARegistro) {
    // 0x4B: ORL A,R3
    // MOV A,#0x12 ; MOV R3,#0x24 ; ORL A,R3
    ProgramMemory program = makeProgram({0x23, 0x12, 0xbb, 0x24, 0x4b});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    cpu.step();
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x12 | 0x24});
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, AnlAIndirectoR1) {
    // 0x51: ANL A,@R1
    // MOV A,#0xf0 ; MOV R1,#0x20 ; MOV @R1,#0x0f ; ANL A,@R1
    ProgramMemory program = makeProgram({0x23, 0xf0, 0xb9, 0x20, 0xb1, 0x0f, 0x51});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step();
    cpu.step();
    cpu.step();
    cpu.step();
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00}); // 0xF0 & 0x0F == 0x00
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, CallPagina2YRetorno) {
    // 0x54: CALL 2 (llama a pagina 2: 0x200 + argumento)
    // 0x000: CALL 0x220 ; NOP
    // 0x220: MOV A,#0x42 ; RET
    ProgramMemory program;
    const std::uint8_t page0[] = {0x54, 0x20, 0x00};
    const std::uint8_t sub[] = {0x23, 0x42, 0x83};
    CHECK(program.loadBank(0, 0x000, page0, sizeof(page0)));
    CHECK(program.loadBank(0, 0x220, sub, sizeof(sub)));

    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // CALL 0x220
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x220});
    cpu.step(); // MOV A,#0x42
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x42});
    cpu.step(); // RET -> vuelve a 0x002
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x002});
    cpu.step(); // NOP en 0x002
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, Jb5SaltaSegunBit5) {
    // 0xB2: JB5
    // Con bit 5 activo (0x20): debe saltar a 0x10
    {
        ProgramMemory program;
        const std::uint8_t code[] = {0x23, 0x20, 0xb2, 0x10, 0x00};
        const std::uint8_t target[] = {0x23, 0x99};
        CHECK(program.loadBank(0, 0, code, sizeof(code)));
        CHECK(program.loadBank(0, 0x10, target, sizeof(target)));
        RecordingBus bus;
        Mcs48Cpu cpu(program, &bus);
        cpu.step(); // MOV A,#0x20
        cpu.step(); // JB5 0x10
        CHECK_EQ(cpu.programCounter(), std::uint16_t{0x10});
        cpu.step(); // MOV A,#0x99
        CHECK_EQ(cpu.accumulator(), std::uint8_t{0x99});
        CHECK(!cpu.faulted());
    }
    // Con bit 5 inactivo (0x00): no salta, cae en el NOP
    {
        ProgramMemory program = makeProgram({0x23, 0x00, 0xb2, 0x10, 0x00});
        RecordingBus bus;
        Mcs48Cpu cpu(program, &bus);
        cpu.step(); // MOV A,#0x00
        cpu.step(); // JB5 0x10
        CHECK_EQ(cpu.programCounter(), std::uint16_t{0x04}); // siguiente instruccion
        CHECK(!cpu.faulted());
    }
}

TEST(Mcs48Cpu, Jt0NoSaltaEnV1) {
    // 0x36: JT0 (sin pin de test conectado en v1: nunca salta)
    ProgramMemory program = makeProgram({0x36, 0x20, 0x00});
    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // JT0 0x20
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x02}); // cae en NOP
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, JmppSaltaSegunTabla) {
    // 0xB3: JMPP @A (salto indirecto en la misma pagina)
    // 0x000: MOV A,#0x06 ; JMPP @A ; NOP
    // 0x006: byte de destino = 0x30
    // 0x030: MOV A,#0x77
    ProgramMemory program;
    const std::uint8_t code[] = {0x23, 0x06, 0xb3, 0x00, 0x00, 0x00, 0x30};
    const std::uint8_t dest[] = {0x23, 0x77};
    CHECK(program.loadBank(0, 0x000, code, sizeof(code)));
    CHECK(program.loadBank(0, 0x030, dest, sizeof(dest)));

    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);
    cpu.step(); // MOV A,#0x06
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x06});
    cpu.step(); // JMPP @A -> lee memoria en 0x006 (0x30) y salta a 0x030
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x030});
    cpu.step(); // MOV A,#0x77
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x77});
    CHECK(!cpu.faulted());
}

TEST(Mcs48Cpu, Juno6SecuenciaInitLimpiaRam) {
    // Secuencia real desensamblada de Juno-6 (upd8049c-238):
    // 0x0000: 0x27       (CLR A)
    // 0x0001: 0x04, 0xA0 (JMP 0x00A0)
    // ...
    // 0x00A0: 0xB8, 0x7F (MOV R0,#0x7F)
    // 0x00A2: 0xA0       (MOV @R0,A)
    // 0x00A3: 0xE8, 0xA2 (DJNZ R0,0x00A2)
    // 0x00A5: 0x00       (NOP)
    ProgramMemory program;
    const std::uint8_t resetCode[] = {0x27, 0x04, 0xa0};
    const std::uint8_t initCode[] = {0xb8, 0x7f, 0xa0, 0xe8, 0xa2, 0x00};
    CHECK(program.loadBank(0, 0x0000, resetCode, sizeof(resetCode)));
    CHECK(program.loadBank(0, 0x00a0, initCode, sizeof(initCode)));

    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);

    // Pre-llenar la RAM con valores no nulos
    for (std::uint8_t addr = 0; addr < 128; ++addr) {
        cpu.writeRam(addr, 0xee);
    }

    // Ejecutar hasta salir del bucle de inicializacion y limpieza de RAM (0x00A5)
    while (cpu.programCounter() != 0x00a5 && !cpu.faulted()) {
        cpu.step();
    }

    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x00a5});
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00});
    CHECK_EQ(cpu.readRegister(0), std::uint8_t{0x00});
    // Comprobar que las direcciones 0x01..0x7F se limpiaron a 0
    for (std::uint8_t addr = 1; addr <= 0x7f; ++addr) {
        CHECK_EQ(cpu.readRam(addr), std::uint8_t{0x00});
    }
}

TEST(Mcs48Cpu, Juno6RomRealArranqueSinFallo) {
    const std::string romPath = "D:/desarrollos/ABDSynths/ABDJuno6ROMRE/juno6/upd8049c-238.ic56";
    std::ifstream stream(romPath, std::ios::binary);
    if (!stream.is_open()) {
        return; // ROM privada no presente en este entorno; se omite limpiamente
    }
    std::vector<std::uint8_t> rom(2048);
    stream.read(reinterpret_cast<char*>(rom.data()), 2048);
    if (stream.gcount() != 2048) {
        return;
    }

    ProgramMemory program;
    CHECK(program.loadBank(0, 0, rom.data(), rom.size()));

    RecordingBus bus;
    Mcs48Cpu cpu(program, &bus);

    // Pre-llenar RAM con 0xCC para verificar limpieza
    for (std::uint8_t addr = 0; addr < 128; ++addr) {
        cpu.writeRam(addr, 0xcc);
    }

    // Ejecutar hasta que termine la secuencia de reset y borrado de RAM (0x00A5)
    while (cpu.programCounter() != 0x00a5 && !cpu.faulted()) {
        cpu.step();
    }

    CHECK(!cpu.faulted());
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x00a5});
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x00});
    CHECK_EQ(cpu.readRegister(0), std::uint8_t{0x00});
    // Toda la RAM 0x01..0x7F debe haber sido limpiada a 0x00 por el firmware real
    for (std::uint8_t addr = 1; addr <= 0x7f; ++addr) {
        CHECK_EQ(cpu.readRam(addr), std::uint8_t{0x00});
    }

    // Continuar ejecutando la inicializacion de perifericos de Juno-6 hasta EN I (0x00F2).
    // Ningun opcode debe fallar por instruccion ilegal; la CPU se detiene limpiamente al
    // llegar a EN I (0x05) con CpuFault::UnsupportedFeature (politica arquitectonica v1).
    while (!cpu.faulted()) {
        cpu.step();
    }
    CHECK(cpu.fault() == CpuFault::UnsupportedFeature);
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x00f3});
}
