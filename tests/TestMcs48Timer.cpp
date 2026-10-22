// TestMcs48Timer.cpp - Temporizador/contador del 8048.
//
// Cubre el contador de 8 bits, el prescaler de 32 ciclos, el flag de
// desbordamiento, los modos timer y contador externo y los opcodes que los
// manejan. La semantica esperada sale del manual del MCS-48 y de la tabla de
// MAME en docs/mame/mcs48.cpp.

#include "TestFramework.h"

#include <cstdint>

#include "core/Mcs48Cpu.h"
#include "core/Mcs48Timer.h"
#include "core/ProgramMemory.h"
#include "machines/GenericMcs48Machine.h"

namespace {

using bdmcs48::core::CpuFault;
using bdmcs48::core::Mcs48Cpu;
using bdmcs48::core::Mcs48Timer;
using bdmcs48::core::PortBus;
using bdmcs48::core::ProgramMemory;
using bdmcs48::core::TimerMode;

class TimerTestBus : public PortBus {};

ProgramMemory programOf(const std::uint8_t* bytes, std::size_t size) {
    ProgramMemory memory;
    CHECK(memory.loadBank(0, 0, bytes, size));
    return memory;
}

} // namespace

// ---------------------------------------------------------------------------
// El contador por separado
// ---------------------------------------------------------------------------

TEST(Mcs48Timer, ElPrescalerDivideEntre32) {
    Mcs48Timer timer;
    timer.startTimer();
    timer.setCounter(0xfe);

    for (int i = 0; i < 31; ++i) {
        timer.tick();
    }
    CHECK_EQ(timer.counter(), std::uint8_t{0xfe});

    timer.tick(); // ciclo 32: primer incremento
    CHECK_EQ(timer.counter(), std::uint8_t{0xff});
    CHECK(!timer.timerFlag());

    for (int i = 0; i < 32; ++i) { // segundo bloque de 32: 0xFF -> 0x00
        timer.tick();
    }
    CHECK_EQ(timer.counter(), std::uint8_t{0x00});
    CHECK(timer.timerFlag());
}

TEST(Mcs48Timer, ElFlagSoloSeEnciendeAlDesbordar) {
    Mcs48Timer timer;
    timer.startTimer();
    timer.setCounter(0x10);

    for (int i = 0; i < 32; ++i) {
        timer.tick();
    }
    CHECK_EQ(timer.counter(), std::uint8_t{0x11});
    CHECK(!timer.timerFlag());

    timer.clearTimerFlag();
    timer.setCounter(0xff);
    for (int i = 0; i < 32; ++i) {
        timer.tick();
    }
    CHECK_EQ(timer.counter(), std::uint8_t{0x00});
    CHECK(timer.timerFlag());
}

TEST(Mcs48Timer, StrTReiniciaElPrescaler) {
    Mcs48Timer timer;
    timer.startTimer();
    for (int i = 0; i < 20; ++i) {
        timer.tick();
    }
    // Con 20 ciclos acumulados, un STRT T deja el prescaler a cero: hacen
    // falta 32 ciclos nuevos para el primer incremento.
    timer.startTimer();
    for (int i = 0; i < 31; ++i) {
        timer.tick();
    }
    CHECK_EQ(timer.counter(), std::uint8_t{0});
    timer.tick();
    CHECK_EQ(timer.counter(), std::uint8_t{1});
}

TEST(Mcs48Timer, ElModoContadorSoloAvanzaConPulsos) {
    Mcs48Timer timer;
    timer.startCounter();
    timer.setCounter(0x10);

    for (int i = 0; i < 64; ++i) {
        timer.tick();
    }
    CHECK_EQ(timer.counter(), std::uint8_t{0x10});
    CHECK_EQ(static_cast<int>(timer.mode()), static_cast<int>(TimerMode::EventCounter));

    timer.externalPulse();
    CHECK_EQ(timer.counter(), std::uint8_t{0x11});
    CHECK(!timer.timerFlag());
}

TEST(Mcs48Timer, ParadoIgnoraCiclosYPulsos) {
    Mcs48Timer timer;
    timer.setCounter(0x10);

    for (int i = 0; i < 64; ++i) {
        timer.tick();
    }
    timer.externalPulse();
    CHECK_EQ(timer.counter(), std::uint8_t{0x10});
    CHECK_EQ(static_cast<int>(timer.mode()), static_cast<int>(TimerMode::Stopped));
}

TEST(Mcs48Timer, ElPulsoExternoNoCuentaEnModoTimer) {
    Mcs48Timer timer;
    timer.startTimer();
    timer.setCounter(0x10);
    timer.externalPulse();
    CHECK_EQ(timer.counter(), std::uint8_t{0x10});
}

TEST(Mcs48Timer, ElPerfilDeMaquinaInyectaPulsosDeT1) {
    using bdmcs48::machines::GenericMcs48Machine;

    GenericMcs48Machine machine(GenericMcs48Machine::Config{48'000, 6'000'000, 15, 128});

    ProgramMemory program;
    const std::uint8_t rom[] = {0x45, 0x00}; // STRT CNT
    CHECK(program.loadBank(0, 0, rom, sizeof(rom)));
    machine.setProgram(program);
    machine.reset();

    std::vector<float> buffer(1, 0.0f);
    float* outputs[1] = {buffer.data()};
    machine.runBlock(1, outputs, 1); // ejecuta STRT CNT
    CHECK_EQ(static_cast<int>(machine.timer().mode()),
             static_cast<int>(TimerMode::EventCounter));

    machine.externalTimerPulse();
    CHECK_EQ(machine.timer().counter(), std::uint8_t{1});

    // En modo timer el pulso de T1 no cuenta: solo los ciclos.
    const std::uint8_t romTimer[] = {0x55, 0x00}; // STRT T
    CHECK(program.loadBank(0, 0, romTimer, sizeof(romTimer)));
    machine.setProgram(program);
    machine.reset();
    machine.runBlock(1, outputs, 1);
    CHECK_EQ(static_cast<int>(machine.timer().mode()), static_cast<int>(TimerMode::Timer));
    const std::uint8_t before = machine.timer().counter();
    machine.externalTimerPulse();
    CHECK_EQ(machine.timer().counter(), before);
}

TEST(Mcs48Timer, ResetVuelveAlEstadoInicial) {
    Mcs48Timer timer;
    timer.startTimer();
    timer.setCounter(0xff);
    for (int i = 0; i < 32; ++i) {
        timer.tick();
    }
    timer.reset();
    CHECK_EQ(timer.counter(), std::uint8_t{0});
    CHECK(!timer.timerFlag());
    CHECK_EQ(static_cast<int>(timer.mode()), static_cast<int>(TimerMode::Stopped));
}

// ---------------------------------------------------------------------------
// Los opcodes, a traves de la CPU
// ---------------------------------------------------------------------------

TEST(Mcs48Timer, MovATLeeElContador) {
    const std::uint8_t program[] = {0x42, 0x00}; // MOV A,T
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.timer().setCounter(0x42);
    cpu.step();
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x42});
    CHECK_EQ(cpu.cycles(), std::uint64_t{1});
}

TEST(Mcs48Timer, MovTAEscribeElContador) {
    const std::uint8_t program[] = {0x23, 0x77, 0x62, 0x42, 0x00};
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step(); // MOV A,#0x77
    cpu.step(); // MOV T,A
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0x77});

    cpu.step(); // MOV A,T devuelve el mismo valor
    CHECK_EQ(cpu.accumulator(), std::uint8_t{0x77});
}

TEST(Mcs48Timer, StrTArrancaElContadorDeCiclos) {
    const std::uint8_t program[] = {0x55, 0x00, 0x00}; // STRT T ; NOP
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.timer().setCounter(0x00);
    cpu.step(); // STRT T
    CHECK_EQ(static_cast<int>(cpu.timer().mode()), static_cast<int>(TimerMode::Timer));

    // 31 NOP mas (32 ciclos contando el STRT T) -> primer incremento.
    for (int i = 0; i < 32; ++i) {
        cpu.step();
    }
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{1});
}

TEST(Mcs48Timer, StrCntArrancaElContadorDeEventos) {
    const std::uint8_t program[] = {0x45, 0x00}; // STRT CNT
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.timer().mode()), static_cast<int>(TimerMode::EventCounter));

    // Los ciclos no cuentan en este modo; los pulsos externos si.
    cpu.runCycles(128);
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0});
    cpu.timer().externalPulse();
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{1});
}

TEST(Mcs48Timer, StopTcntDetieneSinBorrarElContador) {
    const std::uint8_t program[] = {0x55, 0x65, 0x00}; // STRT T ; STOP TCNT
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.timer().setCounter(0x20);
    cpu.step(); // STRT T (reinicia el prescaler)
    cpu.step(); // STOP TCNT
    CHECK_EQ(static_cast<int>(cpu.timer().mode()), static_cast<int>(TimerMode::Stopped));
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0x20});

    cpu.runCycles(128);
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0x20});
}

TEST(Mcs48Timer, ElDesbordamientoEnciendeElFlagObservable) {
    const std::uint8_t program[] = {0x55, 0x00, 0x00};
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.timer().setCounter(0xff);
    cpu.step(); // STRT T
    for (int i = 0; i < 32; ++i) {
        cpu.step();
    }
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0x00});
    CHECK(cpu.timer().timerFlag());

    cpu.timer().clearTimerFlag();
    CHECK(!cpu.timer().timerFlag());
}

TEST(Mcs48Timer, LasInterrupcionesSiguenFallando) {
    // El contador esta modelado, pero EN I, DIS I, EN TCNTI, DIS TCNTI,
    // ENT0 CLK, JNI y JTF exigen el modelo de interrupciones.
    const std::uint8_t opcodes[] = {0x05, 0x15, 0x16, 0x25, 0x35, 0x75, 0x86};
    for (std::uint8_t opcode : opcodes) {
        const std::uint8_t program[] = {opcode, 0x00};
        ProgramMemory memory = programOf(program, sizeof(program));
        TimerTestBus bus;
        Mcs48Cpu cpu(memory, &bus);

        cpu.step();
        CHECK_EQ(static_cast<int>(cpu.fault()), static_cast<int>(CpuFault::UnsupportedFeature));
        CHECK(cpu.faultMessage().find("interrupciones") != std::string::npos);
    }
}

TEST(Mcs48Timer, ResetLimpiaElContador) {
    const std::uint8_t program[] = {0x55, 0x00};
    ProgramMemory memory = programOf(program, sizeof(program));
    TimerTestBus bus;
    Mcs48Cpu cpu(memory, &bus);

    cpu.timer().setCounter(0x33);
    cpu.step();
    CHECK_EQ(static_cast<int>(cpu.timer().mode()), static_cast<int>(TimerMode::Timer));
    cpu.reset();
    CHECK_EQ(cpu.timer().counter(), std::uint8_t{0});
    CHECK_EQ(static_cast<int>(cpu.timer().mode()), static_cast<int>(TimerMode::Stopped));
    CHECK_EQ(cpu.cycles(), std::uint64_t{0});
}
