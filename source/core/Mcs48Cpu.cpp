#include "core/Mcs48Cpu.h"

#include <cstdio>

namespace bdmcs48::core {
namespace {

// Bases de los bancos de registros (MAME usa 24 = 0x18 para el banco 1).
constexpr std::uint8_t kRegisterBank0 = 0x00;
constexpr std::uint8_t kRegisterBank1 = 0x18;

// La pila ocupa 8 niveles de 2 bytes en 0x08-0x17.
constexpr std::uint8_t kStackBase = 0x08;
constexpr std::uint8_t kStackLevels = 8;

constexpr std::uint16_t kAddressMask = 0x0fff;
constexpr std::uint16_t kSequentialMask = 0x07ff;
constexpr std::uint16_t kBankBit = 0x0800;
constexpr std::uint16_t kPageMask = 0x0f00;

std::string toHex(std::uint8_t value) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "0x%02X", value);
    return std::string(buffer);
}

} // namespace

Mcs48Cpu::Mcs48Cpu(ProgramMemory& program, PortBus* bus,
                     std::size_t ramSize, std::uint32_t clockHz,
                     std::uint32_t clocksPerCycle)
    : program_(program), bus_(bus),
      config_{ramSize, clockHz, clocksPerCycle} {
    if (config_.ramSize == 0 || config_.ramSize > 256) {
        config_.ramSize = 128;
    }
    ram_.assign(config_.ramSize, 0);
    reset();
}

void Mcs48Cpu::reset() {
    ram_.assign(config_.ramSize, 0);
    state_ = Mcs48State{};
    timer_.reset();
    stackDepth_ = 0;
    fault_ = CpuFault::None;
    faultMessage_.clear();
}

void Mcs48Cpu::raiseFault(CpuFault fault, std::string message) {
    if (fault_ != CpuFault::None) {
        return; // se conserva el primer fallo: es el que explica la parada
    }
    fault_ = fault;
    faultMessage_ = std::move(message);
}

std::uint8_t Mcs48Cpu::fetch() {
    const std::uint16_t address = state_.pc;
    state_.pc = static_cast<std::uint16_t>(((state_.pc + 1) & kSequentialMask) |
                                          (state_.pc & kBankBit));
    return program_.read(address);
}

std::uint8_t Mcs48Cpu::fetchArgument() {
    return fetch();
}

std::uint8_t& Mcs48Cpu::registerRef(std::uint8_t index) {
    const std::uint8_t base = (state_.psw & kBankFlag) != 0 ? kRegisterBank1 : kRegisterBank0;
    const std::size_t address = static_cast<std::size_t>(base) + (index & 0x07);
    return ram_[address % ram_.size()];
}

std::uint8_t Mcs48Cpu::registerValue(std::uint8_t index) const {
    return const_cast<Mcs48Cpu*>(this)->registerRef(index);
}

std::uint8_t Mcs48Cpu::readRam(std::uint8_t address) const {
    return ram_[static_cast<std::size_t>(address) % ram_.size()];
}

void Mcs48Cpu::writeRam(std::uint8_t address, std::uint8_t value) {
    ram_[static_cast<std::size_t>(address) % ram_.size()] = value;
}

std::uint8_t Mcs48Cpu::readRegister(std::uint8_t index) const {
    return registerValue(index);
}

void Mcs48Cpu::writeRegister(std::uint8_t index, std::uint8_t value) {
    registerRef(index) = value;
}

void Mcs48Cpu::updateRegisterBank() {
    // El banco activo se deduce del PSW en cada acceso; no hay puntero cacheado.
}

void Mcs48Cpu::runCycles(std::uint64_t cycles) {
    const std::uint64_t target = state_.cycles + cycles;
    while (state_.cycles < target && fault_ == CpuFault::None) {
        step();
    }
}

void Mcs48Cpu::step() {
    if (fault_ != CpuFault::None) {
        return;
    }
    const std::uint8_t opcode = fetch();
    execute(opcode);
}

void Mcs48Cpu::executeJmp(std::uint16_t address) {
    // El bit A11 lo aporta el flip-flop de banco (SEL MB0/MB1) SOLO aqui.
    const std::uint16_t bankBit = state_.bank != 0 ? kBankBit : 0;
    state_.pc = static_cast<std::uint16_t>(address | bankBit);
}

void Mcs48Cpu::executeJcc(bool condition) {
    // Los saltos condicionales conservan la pagina actual (PC bits 8-11):
    // NO cambian de banco aunque el flip-flop A11 valga 1.
    const std::uint16_t page = static_cast<std::uint16_t>(state_.pc & kPageMask);
    const std::uint8_t offset = fetchArgument();
    if (condition) {
        state_.pc = static_cast<std::uint16_t>(page | offset);
    }
}

void Mcs48Cpu::pushPcPsw() {
    if (stackDepth_ >= kStackLevels) {
        // El hardware real no lo detecta: el puntero de 3 bits da la vuelta y
        // sobrescribe el nivel mas antiguo. Un CALL con la pila llena es un
        // error de firmware y aqui se detiene la CPU.
        raiseFault(CpuFault::StackOverflow, "CALL con los 8 niveles de pila ocupados");
        return;
    }
    const std::uint8_t sp = static_cast<std::uint8_t>(state_.psw & 0x07);
    writeRam(static_cast<std::uint8_t>(kStackBase + 2 * sp), static_cast<std::uint8_t>(state_.pc & 0xff));
    writeRam(static_cast<std::uint8_t>(kStackBase + 2 * sp + 1),
             static_cast<std::uint8_t>(((state_.pc >> 8) & 0x0f) | (state_.psw & 0xf0)));
    state_.psw = static_cast<std::uint8_t>((state_.psw & 0xf0) | ((sp + 1) & 0x07));
    ++stackDepth_;
}

void Mcs48Cpu::pullPc() {
    if (stackDepth_ == 0) {
        raiseFault(CpuFault::StackUnderflow, "RET sin ningun retorno pendiente en la pila");
        return;
    }
    const std::uint8_t sp = static_cast<std::uint8_t>((state_.psw - 1) & 0x07);
    std::uint16_t pc = readRam(static_cast<std::uint8_t>(kStackBase + 2 * sp));
    pc = static_cast<std::uint16_t>(pc | (readRam(static_cast<std::uint8_t>(kStackBase + 2 * sp + 1)) << 8));
    state_.pc = static_cast<std::uint16_t>(pc & kAddressMask);
    state_.psw = static_cast<std::uint8_t>((state_.psw & 0xf0) | sp);
    --stackDepth_;
}

void Mcs48Cpu::pullPcPsw() {
    if (stackDepth_ == 0) {
        raiseFault(CpuFault::StackUnderflow, "RETR sin ningun retorno pendiente en la pila");
        return;
    }
    const std::uint8_t sp = static_cast<std::uint8_t>((state_.psw - 1) & 0x07);
    std::uint16_t pc = readRam(static_cast<std::uint8_t>(kStackBase + 2 * sp));
    pc = static_cast<std::uint16_t>(pc | (readRam(static_cast<std::uint8_t>(kStackBase + 2 * sp + 1)) << 8));
    state_.psw = static_cast<std::uint8_t>(((pc >> 8) & 0xf0) | sp);
    state_.pc = static_cast<std::uint16_t>(pc & kAddressMask);
    --stackDepth_;
}

void Mcs48Cpu::executeCall(std::uint16_t address) {
    pushPcPsw();
    executeJmp(address);
}

void Mcs48Cpu::executeAdd(std::uint8_t value) {
    const std::uint16_t temp = static_cast<std::uint16_t>(state_.accumulator + value);
    const std::uint16_t temp4 = static_cast<std::uint16_t>((state_.accumulator & 0x0f) + (value & 0x0f));
    state_.psw = static_cast<std::uint8_t>(state_.psw & ~(kCarryFlag | kAuxCarryFlag));
    state_.psw = static_cast<std::uint8_t>(state_.psw | ((temp4 << 2) & kAuxCarryFlag));
    state_.psw = static_cast<std::uint8_t>(state_.psw | ((temp >> 1) & kCarryFlag));
    state_.accumulator = static_cast<std::uint8_t>(temp & 0xff);
}

void Mcs48Cpu::executeAddc(std::uint8_t value) {
    const std::uint8_t carryIn = static_cast<std::uint8_t>((state_.psw & kCarryFlag) >> 7);
    const std::uint16_t temp = static_cast<std::uint16_t>(state_.accumulator + value + carryIn);
    const std::uint16_t temp4 = static_cast<std::uint16_t>((state_.accumulator & 0x0f) + (value & 0x0f) + carryIn);
    state_.psw = static_cast<std::uint8_t>(state_.psw & ~(kCarryFlag | kAuxCarryFlag));
    state_.psw = static_cast<std::uint8_t>(state_.psw | ((temp4 << 2) & kAuxCarryFlag));
    state_.psw = static_cast<std::uint8_t>(state_.psw | ((temp >> 1) & kCarryFlag));
    state_.accumulator = static_cast<std::uint8_t>(temp & 0xff);
}

void Mcs48Cpu::executeAnl(std::uint8_t value) {
    state_.accumulator = static_cast<std::uint8_t>(state_.accumulator & value);
}

void Mcs48Cpu::executeOrl(std::uint8_t value) {
    state_.accumulator = static_cast<std::uint8_t>(state_.accumulator | value);
}

void Mcs48Cpu::executeXrl(std::uint8_t value) {
    state_.accumulator = static_cast<std::uint8_t>(state_.accumulator ^ value);
}

void Mcs48Cpu::execute(std::uint8_t opcode) {
    // Cada ciclo de maquina hace avanzar el prescaler del temporizador.
    const auto burn = [this](std::uint64_t cycles) {
        state_.cycles += cycles;
        for (std::uint64_t i = 0; i < cycles; ++i) {
            timer_.tick();
        }
    };

    // --- Familias contiguas Rn --------------------------------------------
    if (opcode >= 0x18 && opcode <= 0x1f) { // INC R0..R7
        writeRegister(static_cast<std::uint8_t>(opcode - 0x18),
                      static_cast<std::uint8_t>(readRegister(static_cast<std::uint8_t>(opcode - 0x18)) + 1));
        burn(1);
        return;
    }
    if (opcode >= 0x28 && opcode <= 0x2f) { // XCH A,R0..R7
        std::uint8_t& reg = registerRef(static_cast<std::uint8_t>(opcode - 0x28));
        const std::uint8_t tmp = state_.accumulator;
        state_.accumulator = reg;
        reg = tmp;
        burn(1);
        return;
    }
    if (opcode >= 0x48 && opcode <= 0x4f) { // ORL A,R0..R7
        executeOrl(readRegister(static_cast<std::uint8_t>(opcode - 0x48)));
        burn(1);
        return;
    }
    if (opcode >= 0x58 && opcode <= 0x5f) { // ANL A,R0..R7
        executeAnl(readRegister(static_cast<std::uint8_t>(opcode - 0x58)));
        burn(1);
        return;
    }
    if (opcode >= 0x68 && opcode <= 0x6f) { // ADD A,R0..R7
        executeAdd(readRegister(static_cast<std::uint8_t>(opcode - 0x68)));
        burn(1);
        return;
    }
    if (opcode >= 0x78 && opcode <= 0x7f) { // ADC A,R0..R7
        executeAddc(readRegister(static_cast<std::uint8_t>(opcode - 0x78)));
        burn(1);
        return;
    }
    if (opcode >= 0xa8 && opcode <= 0xaf) { // MOV Rn,A
        writeRegister(static_cast<std::uint8_t>(opcode - 0xa8), state_.accumulator);
        burn(1);
        return;
    }
    if (opcode >= 0xb8 && opcode <= 0xbf) { // MOV Rn,#data
        writeRegister(static_cast<std::uint8_t>(opcode - 0xb8), fetchArgument());
        burn(2);
        return;
    }
    if (opcode >= 0xc8 && opcode <= 0xcf) { // DEC R0..R7
        writeRegister(static_cast<std::uint8_t>(opcode - 0xc8),
                      static_cast<std::uint8_t>(readRegister(static_cast<std::uint8_t>(opcode - 0xc8)) - 1));
        burn(1);
        return;
    }
    if (opcode >= 0xd8 && opcode <= 0xdf) { // XRL A,R0..R7
        executeXrl(readRegister(static_cast<std::uint8_t>(opcode - 0xd8)));
        burn(1);
        return;
    }
    if (opcode >= 0xe8 && opcode <= 0xef) { // DJNZ R0..R7
        const std::uint8_t index = static_cast<std::uint8_t>(opcode - 0xe8);
        writeRegister(index, static_cast<std::uint8_t>(readRegister(index) - 1));
        executeJcc(readRegister(index) != 0);
        burn(2);
        return;
    }
    if (opcode >= 0xf8 && opcode <= 0xff) { // MOV A,R0..R7
        state_.accumulator = readRegister(static_cast<std::uint8_t>(opcode - 0xf8));
        burn(1);
        return;
    }

    switch (opcode) {
    // --- NOP y control ----------------------------------------------------
    case 0x00: // NOP
        burn(1);
        break;
    case 0x05: // EN I
    case 0x15: // DIS I
        // Sin modelo de interrupciones, habilitarlas o deshabilitarlas no
        // puede simularse: se avisa en lugar de fingir que se ha hecho.
        raiseFault(CpuFault::UnsupportedFeature,
                   "las interrupciones del MCS-48 no estan implementadas (opcode " +
                       toHex(opcode) + ")");
        break;

    // --- Acumulador y registros ------------------------------------------
    case 0x07: // DEC A
        state_.accumulator = static_cast<std::uint8_t>(state_.accumulator - 1);
        burn(1);
        break;
    case 0x17: // INC A
        state_.accumulator = static_cast<std::uint8_t>(state_.accumulator + 1);
        burn(1);
        break;
    case 0x27: // CLR A
        state_.accumulator = 0;
        burn(1);
        break;
    case 0x37: // CPL A
        state_.accumulator = static_cast<std::uint8_t>(state_.accumulator ^ 0xff);
        burn(1);
        break;
    case 0x47: // SWAP A
        state_.accumulator = static_cast<std::uint8_t>((state_.accumulator >> 4) |
                                                       (state_.accumulator << 4));
        burn(1);
        break;
    case 0x57: // DA A
        if ((state_.accumulator & 0x0f) > 0x09 || (state_.psw & kAuxCarryFlag) != 0) {
            if (state_.accumulator > 0xf9) {
                state_.psw = static_cast<std::uint8_t>(state_.psw | kCarryFlag);
            }
            state_.accumulator = static_cast<std::uint8_t>(state_.accumulator + 0x06);
        }
        if ((state_.accumulator & 0xf0) > 0x90 || (state_.psw & kCarryFlag) != 0) {
            state_.accumulator = static_cast<std::uint8_t>(state_.accumulator + 0x60);
            state_.psw = static_cast<std::uint8_t>(state_.psw | kCarryFlag);
        }
        burn(1);
        break;
    case 0x67: // RRC A
    {
        const std::uint8_t newCarry = static_cast<std::uint8_t>((state_.accumulator << 7) & kCarryFlag);
        state_.accumulator = static_cast<std::uint8_t>((state_.accumulator >> 1) |
                                                       (state_.psw & kCarryFlag));
        state_.psw = static_cast<std::uint8_t>((state_.psw & ~kCarryFlag) | newCarry);
        burn(1);
        break;
    }
    case 0x77: // RR A
        state_.accumulator = static_cast<std::uint8_t>((state_.accumulator >> 1) | (state_.accumulator << 7));
        burn(1);
        break;
    case 0xe7: // RL A
        state_.accumulator = static_cast<std::uint8_t>((state_.accumulator << 1) | (state_.accumulator >> 7));
        burn(1);
        break;
    case 0xf7: // RLC A
    {
        const std::uint8_t newCarry = static_cast<std::uint8_t>(state_.accumulator & kCarryFlag);
        state_.accumulator = static_cast<std::uint8_t>((state_.accumulator << 1) | (state_.psw >> 7));
        state_.psw = static_cast<std::uint8_t>((state_.psw & ~kCarryFlag) | newCarry);
        burn(1);
        break;
    }
    case 0x42: // MOV A,T
        state_.accumulator = timer_.counter();
        burn(1);
        break;
    case 0x62: // MOV T,A
        timer_.setCounter(state_.accumulator);
        burn(1);
        break;
    case 0x45: // STRT CNT
        timer_.startCounter();
        burn(1);
        break;
    case 0x55: // STRT T
        timer_.startTimer();
        burn(1);
        break;
    case 0x65: // STOP TCNT
        timer_.stop();
        burn(1);
        break;
    case 0xc7: // MOV A,PSW
        state_.accumulator = static_cast<std::uint8_t>(state_.psw | 0x08);
        burn(1);
        break;
    case 0xd7: // MOV PSW,A (0xA7 NO es MOV PSW,A: es CPL C)
        state_.psw = static_cast<std::uint8_t>(state_.accumulator & ~0x08);
        updateRegisterBank();
        burn(1);
        break;
    case 0xa7: // CPL C
        state_.psw = static_cast<std::uint8_t>(state_.psw ^ kCarryFlag);
        burn(1);
        break;
    case 0x85: // CLR F0
        state_.psw = static_cast<std::uint8_t>(state_.psw & ~kF0Flag);
        burn(1);
        break;
    case 0x95: // CPL F0
        state_.psw = static_cast<std::uint8_t>(state_.psw ^ kF0Flag);
        burn(1);
        break;
    case 0x97: // CLR C
        state_.psw = static_cast<std::uint8_t>(state_.psw & ~kCarryFlag);
        burn(1);
        break;
    case 0xa5: // CLR F1 (sin pin F1 en v1)
    case 0xb5: // CPL F1
        burn(1);
        break;

    // --- Registros indirectos --------------------------------------------
    case 0x10: // INC @R0
    case 0x11: // INC @R1
    {
        const std::uint8_t index = static_cast<std::uint8_t>(opcode & 0x01);
        const std::uint8_t address = readRegister(index);
        writeRam(address, static_cast<std::uint8_t>(readRam(address) + 1));
        burn(1);
        break;
    }
    case 0x20: // XCH A,@R0
    case 0x21: // XCH A,@R1
    {
        const std::uint8_t address = readRegister(static_cast<std::uint8_t>(opcode & 0x01));
        const std::uint8_t tmp = state_.accumulator;
        state_.accumulator = readRam(address);
        writeRam(address, tmp);
        burn(1);
        break;
    }
    case 0x30: // XCHD A,@R0
    case 0x31: // XCHD A,@R1
    {
        const std::uint8_t address = readRegister(static_cast<std::uint8_t>(opcode & 0x01));
        const std::uint8_t oldRam = readRam(address);
        writeRam(address, static_cast<std::uint8_t>((oldRam & 0xf0) | (state_.accumulator & 0x0f)));
        state_.accumulator = static_cast<std::uint8_t>((state_.accumulator & 0xf0) | (oldRam & 0x0f));
        burn(1);
        break;
    }
    case 0x40: // ORL A,@R0
    case 0x41: // ORL A,@R1
        executeOrl(readRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01))));
        burn(1);
        break;
    case 0x50: // ANL A,@R0
    case 0x51: // ANL A,@R1
        executeAnl(readRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01))));
        burn(1);
        break;
    case 0x60: // ADD A,@R0
    case 0x61: // ADD A,@R1
        executeAdd(readRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01))));
        burn(1);
        break;
    case 0x70: // ADC A,@R0
    case 0x71: // ADC A,@R1
        executeAddc(readRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01))));
        burn(1);
        break;
    case 0xa0: // MOV @R0,A
    case 0xa1: // MOV @R1,A
        writeRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01)), state_.accumulator);
        burn(1);
        break;
    case 0xb0: // MOV @R0,#data
    case 0xb1: // MOV @R1,#data
        writeRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01)), fetchArgument());
        burn(2);
        break;
    case 0xd0: // XRL A,@R0
    case 0xd1: // XRL A,@R1
        executeXrl(readRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01))));
        burn(1);
        break;
    case 0xf0: // MOV A,@R0
    case 0xf1: // MOV A,@R1
        state_.accumulator = readRam(readRegister(static_cast<std::uint8_t>(opcode & 0x01)));
        burn(1);
        break;
    case 0x80: // MOVX A,@R0
    case 0x81: // MOVX A,@R1
        state_.accumulator = bus_ != nullptr
                                 ? bus_->readExternal(readRegister(static_cast<std::uint8_t>(opcode & 0x01)))
                                 : 0xff;
        burn(2);
        break;
    case 0x90: // MOVX @R0,A
    case 0x91: // MOVX @R1,A
        if (bus_ != nullptr) {
            bus_->writeExternal(readRegister(static_cast<std::uint8_t>(opcode & 0x01)), state_.accumulator);
        }
        burn(2);
        break;

    // --- Aritmetica inmediata ---------------------------------------------
    case 0x03: // ADD A,#data
        executeAdd(fetchArgument());
        burn(2);
        break;
    case 0x13: // ADC A,#data
        executeAddc(fetchArgument());
        burn(2);
        break;
    case 0x23: // MOV A,#data
        state_.accumulator = fetchArgument();
        burn(2);
        break;
    case 0x43: // ORL A,#data
        executeOrl(fetchArgument());
        burn(2);
        break;
    case 0x53: // ANL A,#data
        executeAnl(fetchArgument());
        burn(2);
        break;
    case 0xd3: // XRL A,#data
        executeXrl(fetchArgument());
        burn(2);
        break;

    // --- Puertos ----------------------------------------------------------
    case 0x02: // OUTL BUS,A
        if (bus_ != nullptr) {
            bus_->writeBus(state_.accumulator);
        }
        burn(2);
        break;
    case 0x08: // INS A,BUS
        state_.accumulator = bus_ != nullptr ? bus_->readBus() : 0xff;
        burn(2);
        break;
    case 0x09: // IN A,P1
        state_.accumulator = bus_ != nullptr ? bus_->readPort(1) : 0xff;
        burn(2);
        break;
    case 0x0a: // IN A,P2
        state_.accumulator = bus_ != nullptr ? bus_->readPort(2) : 0xff;
        burn(2);
        break;
    case 0x0c: // MOV A,P4
    case 0x0d: // MOV A,P5
    case 0x0e: // MOV A,P6
    case 0x0f: // MOV A,P7
        state_.accumulator = bus_ != nullptr ? bus_->readPort(static_cast<std::uint8_t>(opcode - 0x08)) : 0xff;
        burn(2);
        break;
    case 0x39: // OUTL P1,A
        if (bus_ != nullptr) {
            bus_->writePort(1, state_.accumulator);
        }
        burn(2);
        break;
    case 0x3a: // OUTL P2,A
        if (bus_ != nullptr) {
            bus_->writePort(2, state_.accumulator);
        }
        burn(2);
        break;
    case 0x3c: // MOV P4,A
    case 0x3d: // MOV P5,A
    case 0x3e: // MOV P6,A
    case 0x3f: // MOV P7,A
        if (bus_ != nullptr) {
            bus_->writePort(static_cast<std::uint8_t>(opcode - 0x38), state_.accumulator);
        }
        burn(2);
        break;
    case 0x88: // ORL BUS,#data
        if (bus_ != nullptr) {
            bus_->writeBus(static_cast<std::uint8_t>(bus_->readBus() | fetchArgument()));
        } else {
            (void)fetchArgument();
        }
        burn(2);
        break;
    case 0x89: // ORL P1,#data
        if (bus_ != nullptr) {
            bus_->writePort(1, static_cast<std::uint8_t>(bus_->readPort(1) | fetchArgument()));
        } else {
            (void)fetchArgument();
        }
        burn(2);
        break;
    case 0x8a: // ORL P2,#data
        if (bus_ != nullptr) {
            bus_->writePort(2, static_cast<std::uint8_t>(bus_->readPort(2) | fetchArgument()));
        } else {
            (void)fetchArgument();
        }
        burn(2);
        break;
    case 0x8c: // ORLD P4,A
    case 0x8d: // ORLD P5,A
    case 0x8e: // ORLD P6,A
    case 0x8f: // ORLD P7,A
        if (bus_ != nullptr) {
            const std::uint8_t port = static_cast<std::uint8_t>(opcode - 0x88);
            bus_->writePort(port, static_cast<std::uint8_t>(bus_->readPort(port) | state_.accumulator));
        }
        burn(2);
        break;
    case 0x98: // ANL BUS,#data
        if (bus_ != nullptr) {
            bus_->writeBus(static_cast<std::uint8_t>(bus_->readBus() & fetchArgument()));
        } else {
            (void)fetchArgument();
        }
        burn(2);
        break;
    case 0x99: // ANL P1,#data
        if (bus_ != nullptr) {
            bus_->writePort(1, static_cast<std::uint8_t>(bus_->readPort(1) & fetchArgument()));
        } else {
            (void)fetchArgument();
        }
        burn(2);
        break;
    case 0x9a: // ANL P2,#data
        if (bus_ != nullptr) {
            bus_->writePort(2, static_cast<std::uint8_t>(bus_->readPort(2) & fetchArgument()));
        } else {
            (void)fetchArgument();
        }
        burn(2);
        break;
    case 0x9c: // ANLD P4,A
    case 0x9d: // ANLD P5,A
    case 0x9e: // ANLD P6,A
    case 0x9f: // ANLD P7,A
        if (bus_ != nullptr) {
            const std::uint8_t port = static_cast<std::uint8_t>(opcode - 0x98);
            bus_->writePort(port, static_cast<std::uint8_t>(bus_->readPort(port) & state_.accumulator));
        }
        burn(2);
        break;

    // --- Saltos ------------------------------------------------------------
    case 0x04: // JMP 0
    case 0x24: // JMP 1
    case 0x44: // JMP 2
    case 0x64: // JMP 3
    case 0x84: // JMP 4
    case 0xa4: // JMP 5
    case 0xc4: // JMP 6
    case 0xe4: // JMP 7
        executeJmp(static_cast<std::uint16_t>(((opcode - 0x04) / 0x20) * 0x100 + fetchArgument()));
        burn(2);
        break;
    case 0x14: // CALL 0
    case 0x34: // CALL 1
    case 0x54: // CALL 2
    case 0x74: // CALL 3
    case 0x94: // CALL 4
    case 0xb4: // CALL 5
    case 0xd4: // CALL 6
    case 0xf4: // CALL 7
        executeCall(static_cast<std::uint16_t>(((opcode - 0x14) / 0x20) * 0x100 + fetchArgument()));
        burn(2);
        break;
    case 0x83: // RET (no restaura el PSW)
        pullPc();
        burn(2);
        break;
    case 0x93: // RETR (restaura tambien el PSW)
        pullPcPsw();
        burn(2);
        break;
    case 0x12: // JB0
    case 0x32: // JB1
    case 0x52: // JB2
    case 0x72: // JB3
    case 0x92: // JB4
    case 0xb2: // JB5
    case 0xd2: // JB6
    case 0xf2: // JB7
    {
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << ((opcode - 0x12) / 0x20));
        executeJcc((state_.accumulator & mask) != 0);
        burn(2);
        break;
    }
    case 0xc6: // JZ
        executeJcc(state_.accumulator == 0);
        burn(2);
        break;
    case 0x96: // JNZ
        executeJcc(state_.accumulator != 0);
        burn(2);
        break;
    case 0xf6: // JC
        executeJcc((state_.psw & kCarryFlag) != 0);
        burn(2);
        break;
    case 0xe6: // JNC
        executeJcc((state_.psw & kCarryFlag) == 0);
        burn(2);
        break;
    case 0xb6: // JF0
        executeJcc((state_.psw & kF0Flag) != 0);
        burn(2);
        break;
    case 0x76: // JF1 (sin pin F1 en v1: nunca se cumple)
    case 0x36: // JT 0 (sin pin de test en v1: nunca se cumple)
    case 0x56: // JT 1
        executeJcc(false);
        burn(2);
        break;
    case 0x26: // JNT 0 (sin pin de test en v1: el pin esta a 0, se cumple)
    case 0x46: // JNT 1
        executeJcc(true);
        burn(2);
        break;
    case 0xa3: // MOVP A,@A
        state_.accumulator = program_.read(static_cast<std::uint16_t>((state_.pc & kPageMask) | state_.accumulator));
        burn(2);
        break;
    case 0xe3: // MOVP3 A,@A
        state_.accumulator = program_.read(static_cast<std::uint16_t>(0x300 | state_.accumulator));
        burn(2);
        break;
    case 0xb3: // JMPP @A
    {
        std::uint16_t page = static_cast<std::uint16_t>(state_.pc & kPageMask);
        const std::uint8_t target = program_.read(static_cast<std::uint16_t>(page | state_.accumulator));
        page = static_cast<std::uint16_t>(page & 0x0f00);
        state_.pc = static_cast<std::uint16_t>(page | target);
        burn(2);
        break;
    }

    // --- Banco de registros y banco de programa ---------------------------
    case 0xc5: // SEL RB0
        state_.psw = static_cast<std::uint8_t>(state_.psw & ~kBankFlag);
        updateRegisterBank();
        burn(1);
        break;
    case 0xd5: // SEL RB1
        state_.psw = static_cast<std::uint8_t>(state_.psw | kBankFlag);
        updateRegisterBank();
        burn(1);
        break;
    case 0xe5: // SEL MB0
        state_.bank = 0;
        burn(1);
        break;
    case 0xf5: // SEL MB1
        state_.bank = 1;
        burn(1);
        break;

    // --- Temporizador: contador modelado, interrupciones no ---------------
    case 0x25: // EN TCNTI
    case 0x35: // DIS TCNTI
    case 0x75: // ENT0 CLK
    case 0x86: // JNI
    case 0x16: // JTF (saltaria segun el flag TF de Mcs48Timer)
        // El contador y su flag existen, pero despachar una interrupcion o
        // saltar por el flag exige el modelo de interrupciones, que no esta
        // implementado. Fallar aqui evita que el firmware crea que ha
        // consultado el flag cuando no lo ha hecho.
        raiseFault(CpuFault::UnsupportedFeature,
                   "las interrupciones del temporizador no estan implementadas (opcode " +
                       toHex(opcode) + ")");
        break;

    default:
        raiseFault(CpuFault::IllegalOpcode,
                   "opcode " + toHex(opcode) + " no implementado en el subconjunto MCS-48");
        break;
    }
}

} // namespace bdmcs48::core
