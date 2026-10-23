// Mcs48Cpu.h - Subconjunto fiel del MCS-48 (Intel 8048/8049/8039/8035).
//
// La implementacion sigue la semántica de MAME (src/devices/cpu/mcs48),
// que a su vez refleja el manual de Intel:
//
//  * El PC tiene 12 bits. El bit 11 (A11) NO es un selector de banco global:
//    lo aporta el flip-flop de banco que fija SEL MB0/SEL MB1 y SOLO se aplica
//    en el JMP/CALL siguiente. El fetch secuencial conserva el bit 11.
//  * Los saltos condicionales (JZ, JNZ, JC, JBn...) NO tocan A11: se quedan
//    en la pagina de 256 bytes actual (PC bits 8-11).
//  * CALL guarda el PC de 12 bits y el PSW; RET restaura el PC, RETR restaura
//    ademas el PSW.
//  * 1 ciclo de maquina = 15 oscilaciones del reloj.
//
// El temporizador/contador (T, prescaler de 32, STRT/STOP, MOV A,T y
// MOV T,A) esta modelado en Mcs48Timer y avanza con los ciclos de maquina.
//
// Los opcodes no implementados (expansores, interrupciones y el salto al
// flag del temporizador) producen un fallo explicito: la CPU se detiene en
// lugar de comportarse como si hiciese algo.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Mcs48Timer.h"
#include "core/ProgramMemory.h"

namespace bdmcs48::core {

// Bus de puertos y datos externos. Lo implementa el perfil de maquina, de
// modo que el core no conoce ningun hardware concreto.
class PortBus {
public:
    virtual ~PortBus() = default;

    // Puertos 1..7 (P1, P2, P4..P7).
    virtual std::uint8_t readPort(std::uint8_t port) { (void)port; return 0xff; }
    virtual void writePort(std::uint8_t port, std::uint8_t value) { (void)port; (void)value; }

    // Bus de datos externo (MOVX A,@Ri / MOVX @Ri,A).
    virtual std::uint8_t readExternal(std::uint8_t address) { (void)address; return 0xff; }
    virtual void writeExternal(std::uint8_t address, std::uint8_t value) {
        (void)address;
        (void)value;
    }

    // Bus bidireccional (INS A,BUS / OUTL BUS,A).
    virtual std::uint8_t readBus() { return 0xff; }
    virtual void writeBus(std::uint8_t value) { (void)value; }
};

enum class CpuFault {
    None,
    IllegalOpcode,
    UnsupportedFeature,
    StackOverflow,
    StackUnderflow,
};

struct Mcs48State {
    std::uint8_t accumulator = 0;
    std::uint8_t psw = 0;
    std::uint16_t pc = 0;
    std::uint8_t bank = 0;      // flip-flop A11: 0 o 1
    std::uint64_t cycles = 0;
};

class Mcs48Cpu {
public:
    struct Config {
        std::size_t ramSize;
        std::uint32_t clockHz;
        std::uint32_t clocksPerCycle;
    };

    // Recibe el config por parametro: los valores por defecto van en el
    // constructor, separados del tipo para que el inicializador de miembros
    // no tropiece con el fin de la clase en C++20.
    Mcs48Cpu(ProgramMemory& program, PortBus* bus,
              std::size_t ramSize = 128, std::uint32_t clockHz = 6'000'000,
              std::uint32_t clocksPerCycle = 15);

    // Banderas del PSW (valores de MAME).
    static constexpr std::uint8_t kCarryFlag = 0x80;
    static constexpr std::uint8_t kAuxCarryFlag = 0x40;
    static constexpr std::uint8_t kF0Flag = 0x20;
    static constexpr std::uint8_t kBankFlag = 0x10;  // SEL RB0/SEL RB1

    // Constructor explícito con el struct Config.
    Mcs48Cpu(ProgramMemory& program, PortBus* bus, Config config) : program_(program), bus_(bus), config_(config) {}

    void reset();

    // Ejecuta instrucciones hasta consumir al menos `cycles` ciclos de maquina.
    void runCycles(std::uint64_t cycles);

    // Ejecuta una instruccion.
    void step();

    const Mcs48State& state() const { return state_; }
    std::uint8_t accumulator() const { return state_.accumulator; }
    std::uint8_t psw() const { return state_.psw; }
    std::uint16_t programCounter() const { return state_.pc; }
    std::uint8_t bank() const { return state_.bank; }
    std::uint64_t cycles() const { return state_.cycles; }
    bool carry() const { return (state_.psw & kCarryFlag) != 0; }

    std::uint8_t readRam(std::uint8_t address) const;
    void writeRam(std::uint8_t address, std::uint8_t value);

    // R0..R7 del banco de registros activo (SEL RB0 / SEL RB1).
    std::uint8_t readRegister(std::uint8_t index) const;
    void writeRegister(std::uint8_t index, std::uint8_t value);

    bool faulted() const { return fault_ != CpuFault::None; }
    CpuFault fault() const { return fault_; }
    const std::string& faultMessage() const { return faultMessage_; }

    std::uint32_t clockHz() const { return config_.clockHz; }
    std::uint32_t clocksPerCycle() const { return config_.clocksPerCycle; }
    std::size_t ramSize() const { return ram_.size(); }

    // Temporizador/contador. El perfil de maquina inyecta los flancos de T1
    // con timer().externalPulse() y el host puede leer el estado.
    Mcs48Timer& timer() { return timer_; }
    const Mcs48Timer& timer() const { return timer_; }

private:
    std::uint8_t fetch();
    std::uint8_t fetchArgument();
    void execute(std::uint8_t opcode);
    void raiseFault(CpuFault fault, std::string message);

    void executeJmp(std::uint16_t address);
    void executeJcc(bool condition);
    void executeCall(std::uint16_t address);
    void pushPcPsw();
    void pullPc();
    void pullPcPsw();
    void updateRegisterBank();

    void executeAdd(std::uint8_t value);
    void executeAddc(std::uint8_t value);
    void executeAnl(std::uint8_t value);
    void executeOrl(std::uint8_t value);
    void executeXrl(std::uint8_t value);

    std::uint8_t& registerRef(std::uint8_t index);
    std::uint8_t registerValue(std::uint8_t index) const;

    ProgramMemory& program_;
    PortBus* bus_ = nullptr;
    Config config_{};
    std::vector<std::uint8_t> ram_;
    Mcs48State state_{};
    Mcs48Timer timer_;
    // Niveles de pila ocupados. El hardware real solo tiene un puntero de 3
    // bits que da la vuelta, asi que hace falta este contador para distinguir
    // "pila llena" de "pila vacia" y poder avisar del error de firmware.
    std::uint8_t stackDepth_ = 0;
    CpuFault fault_ = CpuFault::None;
    std::string faultMessage_;
};

} // namespace bdmcs48::core
