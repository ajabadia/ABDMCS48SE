// TestToyRom.cpp - El subconjunto MCS-48 contra un vector ROM propio.
//
// El vector es un programa pequeno escrito a mano que ejercita las familias
// que implementa Mcs48Cpu: MOV A,#data, OUTL P1,A, JMP de pagina, CALL/RET,
// CLR A, ADD A,#data y SEL MB0/SEL MB1. Los valores esperados (acumulador,
// puerto, PC y ciclos) estan derivados del manual del MCS-48, no de ejecutar
// esta implementacion: si la CPU cambia de semantica, el test falla.
//
// El vector es sintetico y vive en este fichero: no hay ninguna ROM de
// terceros ni material con copyright.

#include "TestFramework.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/Mcs48Cpu.h"
#include "core/ProgramMemory.h"

namespace {

using bdmcs48::core::CpuFault;
using bdmcs48::core::Mcs48Cpu;
using bdmcs48::core::PortBus;
using bdmcs48::core::ProgramMemory;

// Bus que recuerda lo que la CPU escribe, para poder comparar las salidas.
class RecordingBus : public PortBus {
public:
    std::uint8_t ports[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::uint8_t external[16] = {0};
    std::uint8_t busValue = 0xff;
    std::vector<std::uint8_t> portWrites;

    std::uint8_t readPort(std::uint8_t port) override { return ports[port & 0x07]; }

    void writePort(std::uint8_t port, std::uint8_t value) override {
        ports[port & 0x07] = value;
        portWrites.push_back(value);
    }

    std::uint8_t readExternal(std::uint8_t address) override { return external[address & 0x0f]; }

    void writeExternal(std::uint8_t address, std::uint8_t value) override {
        external[address & 0x0f] = value;
    }

    std::uint8_t readBus() override { return busValue; }
    void writeBus(std::uint8_t value) override { busValue = value; }
};

// ---------------------------------------------------------------------------
// Vector A: llamada a subrutina y retorno.
//
// El modelo de ciclos de este core cuenta un ciclo por ciclo de maquina
// (15 oscilaciones) y las instrucciones de dos bytes consumen dos: esa es
// la convencion que ya fija TestMcs48Cpu (MovInmediatoCuestaDosCiclos).
//
//   0x000  23 42   MOV A,#0x42      2 ciclos
//   0x002  39      OUTL P1,A        2 ciclos  -> P1 = 0x42
//   0x003  04 10   JMP 0x010        2 ciclos
//   0x005  ...     relleno
//   0x010  14 20   CALL 0x020       2 ciclos
//   0x012  04 00   JMP 0x000        2 ciclos
//   0x020  27      CLR A            1 ciclo   -> A = 0
//   0x021  03 05   ADD A,#0x05      2 ciclos  -> A = 5, C = 0
//   0x023  83      RET              2 ciclos
// ---------------------------------------------------------------------------
constexpr std::size_t kVectorACallAddress = 0x010;
constexpr std::size_t kVectorASubroutineAddress = 0x020;
constexpr std::size_t kVectorAJumpBackAddress = 0x012;

std::vector<std::uint8_t> makeVectorA() {
    std::vector<std::uint8_t> rom(kVectorASubroutineAddress + 4, 0x00);
    rom[0x000] = 0x23; // MOV A,#0x42
    rom[0x001] = 0x42;
    rom[0x002] = 0x39; // OUTL P1,A
    rom[0x003] = 0x04; // JMP 0x010
    rom[0x004] = 0x10;
    rom[0x010] = 0x14; // CALL 0x020
    rom[0x011] = 0x20;
    rom[0x012] = 0x04; // JMP 0x000
    rom[0x013] = 0x00;
    rom[0x020] = 0x27; // CLR A
    rom[0x021] = 0x03; // ADD A,#0x05
    rom[0x022] = 0x05;
    rom[0x023] = 0x83; // RET
    return rom;
}

// ---------------------------------------------------------------------------
// Vector B: el bit A11 lo aporta SOLO el flip-flop SEL MB, y solo en JMP/CALL.
//
//   0x000  F5      SEL MB1          1 ciclo   -> bank = 1
//   0x001  04 10   JMP 0x010        2 ciclos  -> PC = 0x810
//   0x003  00      NOP              (no se ejecuta)
//   0x810  E5      SEL MB0          1 ciclo   -> bank = 0
//   0x811  04 20   JMP 0x020        2 ciclos  -> PC = 0x020
//   0x813  00      NOP              (no se ejecuta)
// ---------------------------------------------------------------------------
std::vector<std::uint8_t> makeVectorB() {
    std::vector<std::uint8_t> rom(ProgramMemory::kSize, 0x00);
    rom[0x000] = 0xf5; // SEL MB1
    rom[0x001] = 0x04; // JMP 0x010
    rom[0x002] = 0x10;
    rom[0x810] = 0xe5; // SEL MB0
    rom[0x811] = 0x04; // JMP 0x020
    rom[0x812] = 0x20;
    return rom;
}

// Carga una imagen completa de 4 KiB repartiendola entre los dos bancos:
// loadBank solo admite una imagen por banco y de 2 KiB como maximo.
void load(ProgramMemory& memory, const std::vector<std::uint8_t>& rom) {
    const std::size_t first = rom.size() < ProgramMemory::kBankSize ? rom.size() : ProgramMemory::kBankSize;
    CHECK(memory.loadBank(0, 0, rom.data(), first));
    if (rom.size() > first) {
        CHECK(memory.loadBank(1, 0, rom.data() + first, rom.size() - first));
    }
}

} // namespace

TEST(ToyRom, LaRomSeCargaEnElBancoCero) {
    ProgramMemory memory;
    const std::vector<std::uint8_t> rom = makeVectorA();
    load(memory, rom);

    CHECK_EQ(memory.loadedBytes(), rom.size());
    CHECK(memory.isBankLoaded(0));
    CHECK(!memory.isBankLoaded(1));
    CHECK_EQ(memory.read(0x000), std::uint8_t{0x23});
    CHECK_EQ(memory.read(0x023), std::uint8_t{0x83});
}

TEST(ToyRom, VectorAEjecutaHastaLaLlamada) {
    ProgramMemory memory;
    const std::vector<std::uint8_t> rom = makeVectorA();
    load(memory, rom);

    RecordingBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step(); // MOV A,#0x42
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x42});
    CHECK_EQ(cpu.cycles(), std::uint64_t{2});

    cpu.step(); // OUTL P1,A
    CHECK_EQ(bus.ports[1], std::uint8_t{0x42});
    CHECK_EQ(cpu.cycles(), std::uint64_t{4});

    cpu.step(); // JMP 0x010
    CHECK_EQ(cpu.programCounter(), static_cast<std::uint16_t>(kVectorACallAddress));
    CHECK_EQ(cpu.cycles(), std::uint64_t{6});
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
}

TEST(ToyRom, VectorALlamaEjecutaYVuelve) {
    ProgramMemory memory;
    const std::vector<std::uint8_t> rom = makeVectorA();
    load(memory, rom);

    RecordingBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.runCycles(6); // hasta 0x010
    CHECK_EQ(cpu.programCounter(), static_cast<std::uint16_t>(kVectorACallAddress));

    cpu.step(); // CALL 0x020
    CHECK_EQ(cpu.programCounter(), static_cast<std::uint16_t>(kVectorASubroutineAddress));
    CHECK_EQ(cpu.cycles(), std::uint64_t{8});
    CHECK_EQ(cpu.psw() & 0x07, std::uint8_t{1}); // la pila tiene un nivel
    CHECK_EQ(cpu.readRam(0x08), std::uint8_t{0x12}); // PC de retorno, byte bajo

    cpu.step(); // CLR A
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0});

    cpu.step(); // ADD A,#0x05
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x05});
    CHECK_EQ(cpu.carry(), false);

    cpu.step(); // RET
    CHECK_EQ(cpu.programCounter(), static_cast<std::uint16_t>(kVectorAJumpBackAddress));
    CHECK_EQ(cpu.psw() & 0x07, std::uint8_t{0}); // la pila vuelve a estar vacia

    cpu.step(); // JMP 0x000
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x000});
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x05});
    CHECK_EQ(cpu.cycles(), std::uint64_t{15});
    CHECK_EQ(bus.portWrites.size(), std::size_t{1});
    CHECK_EQ(bus.ports[1], std::uint8_t{0x42});
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
}

TEST(ToyRom, VectorBElBitA11LoSoloAportaSelMb) {
    ProgramMemory memory;
    const std::vector<std::uint8_t> rom = makeVectorB();
    load(memory, rom);

    RecordingBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step(); // SEL MB1
    CHECK_EQ(cpu.bank(), std::uint8_t{1});
    // El fetch secuencial NO cambia de banco por si mismo.
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x001});

    cpu.step(); // JMP 0x010 con el flip-flop a 1
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x810});
    CHECK_EQ(cpu.cycles(), std::uint64_t{3});

    cpu.step(); // SEL MB0
    CHECK_EQ(cpu.bank(), std::uint8_t{0});

    cpu.step(); // JMP 0x020 con el flip-flop a 0
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x020});
    CHECK_EQ(cpu.cycles(), std::uint64_t{6});
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
}

// ---------------------------------------------------------------------------
// Vector C: un salto condicional NO cambia de pagina ni aplica A11.
//
//   0x000  34 00   CALL 0x100       2 ciclos  -> PC = 0x100
//   0x002  00      NOP              (destino del RET)
//   0x100  F5      SEL MB1          1 ciclo   -> bank = 1
//   0x101  C6 40   JZ 0x040         2 ciclos  -> PC = 0x140 (pagina 1)
//   0x140  83      RET              2 ciclos  -> PC = 0x002
// ---------------------------------------------------------------------------
TEST(ToyRom, VectorCElSaltoCondicionalConservaLaPagina) {
    ProgramMemory memory;
    std::vector<std::uint8_t> rom(ProgramMemory::kSize, 0x00);
    rom[0x000] = 0x34; // CALL 0x100 (CALL de la pagina 1)
    rom[0x001] = 0x00;
    rom[0x100] = 0xf5; // SEL MB1
    rom[0x101] = 0xc6; // JZ 0x040
    rom[0x102] = 0x40;
    rom[0x140] = 0x83; // RET

    load(memory, rom);
    RecordingBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step(); // CALL 0x100
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x100});
    CHECK_EQ(cpu.cycles(), std::uint64_t{2});

    cpu.step(); // SEL MB1
    CHECK_EQ(cpu.bank(), std::uint8_t{1});

    cpu.step(); // JZ 0x040 (A es 0 tras el reset: se cumple)
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x140});
    CHECK_EQ(cpu.cycles(), std::uint64_t{5});

    cpu.step(); // RET
    CHECK_EQ(cpu.programCounter(), std::uint16_t{0x002});
    CHECK_EQ(cpu.cycles(), std::uint64_t{7});
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
}

TEST(ToyRom, UnOpcodeNoImplementadoDetieneLaCpu) {
    ProgramMemory memory;
    std::vector<std::uint8_t> rom(ProgramMemory::kSize, 0x00);
    rom[0x000] = 0x01; // opcode sin definir en el subconjunto
    load(memory, rom);

    RecordingBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::IllegalOpcode));
    CHECK(cpu.faulted());
    const std::uint64_t cyclesAtFault = cpu.cycles();

    // Un fallo es terminal: no se ejecuta nada mas ni se consumen ciclos.
    cpu.step();
    cpu.runCycles(16);
    CHECK_EQ(cpu.cycles(), cyclesAtFault);
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::IllegalOpcode));
}

TEST(ToyRom, ElTemporizadorSeArrancaYSeLee) {
    ProgramMemory memory;
    std::vector<std::uint8_t> rom(ProgramMemory::kSize, 0x00);
    rom[0x000] = 0x55; // STRT T
    rom[0x001] = 0x23; // MOV A,#0x10
    rom[0x002] = 0x10;
    rom[0x003] = 0x62; // MOV T,A
    rom[0x004] = 0x42; // MOV A,T
    load(memory, rom);

    RecordingBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step(); // STRT T
    CHECK_EQ(static_cast<int>(cpu.timer().mode()), static_cast<int>(bdmcs48::core::TimerMode::Timer));

    cpu.step(); // MOV A,#0x10
    cpu.step(); // MOV T,A
    cpu.step(); // MOV A,T
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0x10});
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x10});
    CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::None));
}
