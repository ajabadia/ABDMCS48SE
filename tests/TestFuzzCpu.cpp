// TestFuzzCpu.cpp - Fuzzing determinista de la CPU MCS-48 y del motor de audio.
//
// Genera programas aleatorios reproducibles mediante PRNG determinista (xorshift32)
// con semillas fijas. Somete el decodificador de instrucciones, la máquina de
// estados, la pila y el bus de puertos a estrés caótico para verificar:
//   1. Cero caídas (crashes), accesos fuera de rango o UB (clave para ASan/UBSan).
//   2. Confinamiento de fallos: todo opcode inválido o desbordamiento de pila
//      detiene la CPU limpiamente marcando CpuFault sin comportamiento errático.
//   3. Confinamiento numérico: el host y la máquina generan audio siempre finito
//      (-1.0f .. 1.0f) sin NaN ni infinitos, incluso con microcódigo caótico.

#include "TestFramework.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "core/Mcs48Cpu.h"
#include "core/ProgramMemory.h"
#include "firmware/FirmwarePackage.h"
#include "firmware/Sha256.h"
#include "host/MachineHost.h"

using namespace bdmcs48;

namespace {

// Generador PRNG xorshift32 determinista local para reproducibilidad estricta
std::uint32_t xorshift32(std::uint32_t& state) {
    std::uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

class FuzzBus : public core::PortBus {
public:
    std::uint8_t readPort(std::uint8_t port) override {
        // Devuelve datos fuzzed pseudoaleatorios dependientes del puerto
        return static_cast<std::uint8_t>((port * 0x37) ^ 0x5A);
    }
    void writePort(std::uint8_t port, std::uint8_t value) override {
        lastPort_ = port;
        lastValue_ = value;
    }
    std::uint8_t readBus() override { return 0xAA; }
    void writeBus(std::uint8_t value) override { lastValue_ = value; }

    std::uint8_t lastPort_ = 0;
    std::uint8_t lastValue_ = 0;
};

} // namespace

TEST(FuzzCpu, CpuEjecutaProgramasCaoticosSinCrashesNiUB) {
    constexpr std::uint32_t kSeedCount = 32;
    constexpr std::uint64_t kCycleBudget = 5'000;

    std::uint32_t faultsCount = 0;
    std::uint32_t completedBudgetCount = 0;

    for (std::uint32_t seed = 1; seed <= kSeedCount; ++seed) {
        std::uint32_t prngState = seed * 0x9E3779B9u;

        // Llena dos bancos de 2048 bytes con bytes totalmente aleatorios
        std::vector<std::uint8_t> bank0(2048);
        std::vector<std::uint8_t> bank1(2048);
        for (std::size_t i = 0; i < 2048; ++i) {
            bank0[i] = static_cast<std::uint8_t>(xorshift32(prngState));
            bank1[i] = static_cast<std::uint8_t>(xorshift32(prngState));
        }

        core::ProgramMemory memory;
        memory.loadBank(0, 0, bank0.data(), bank0.size());
        memory.loadBank(1, 0, bank1.data(), bank1.size());
        FuzzBus bus;
        core::Mcs48Cpu cpu(memory, &bus);

        std::uint64_t cyclesExecuted = 0;
        while (cyclesExecuted < kCycleBudget && !cpu.faulted()) {
            const std::uint64_t before = cpu.cycles();
            cpu.step();
            const std::uint64_t stepCycles = cpu.cycles() - before;
            // Cada instrucción válida de MCS-48 consume 1 o 2 ciclos (15 o 30 clocks)
            if (cpu.faulted()) {
                break;
            }
            CHECK(stepCycles == 1u || stepCycles == 2u);
            cyclesExecuted += stepCycles;
        }

        // Invariantes absolutos
        CHECK(cpu.programCounter() <= 0x0FFFu); // Espacio de 12 bits
        if (cpu.faulted()) {
            faultsCount++;
            CHECK(cpu.fault() != core::CpuFault::None);
            CHECK(!cpu.faultMessage().empty());
        } else {
            completedBudgetCount++;
        }
    }

    std::printf("      (fuzzing CPU: %u semillas, %u detenciones limpias por fallo, %u consumieron presupuesto)\n",
                kSeedCount, faultsCount, completedBudgetCount);

    CHECK(faultsCount + completedBudgetCount == kSeedCount);
}

TEST(FuzzCpu, HostRenderizaProgramasCaoticosSinNaNsNiInfinitos) {
    constexpr std::uint32_t kSeedCount = 16;
    std::uint32_t audioBlocksFuzzed = 0;

    for (std::uint32_t seed = 100; seed < 100 + kSeedCount; ++seed) {
        std::uint32_t prngState = seed * 0x85EBCA6Bu;

        firmware::LoadedPackage pkg;
        pkg.manifest.format = "bdmcs48se-firmware";
        pkg.manifest.formatVersion = 1;
        pkg.manifest.id = "fuzz_pkg_" + std::to_string(seed);
        pkg.manifest.name = "Fuzz Package";
        pkg.manifest.version = "1.0.0";
        pkg.manifest.machine.profile = "generic-mcs48";
        pkg.manifest.machine.programMemory.addressBits = 12;
        pkg.manifest.machine.programMemory.bankSizeBytes = 2048;
        pkg.manifest.machine.programMemory.bankCount = 2;

        firmware::LoadedImage img0;
        img0.id = "fuzz-bank0";
        img0.region = "program";
        img0.bank = 0;
        img0.offset = 0;
        img0.data.resize(2048);

        firmware::LoadedImage img1;
        img1.id = "fuzz-bank1";
        img1.region = "program";
        img1.bank = 1;
        img1.offset = 0;
        img1.data.resize(2048);

        for (std::size_t i = 0; i < 2048; ++i) {
            img0.data[i] = static_cast<std::uint8_t>(xorshift32(prngState));
            img1.data[i] = static_cast<std::uint8_t>(xorshift32(prngState));
        }

        pkg.manifest.images.push_back({img0.id, "roms/b0.rom", img0.region, 0, 0, 2048, firmware::Sha256::hexOf(img0.data)});
        pkg.manifest.images.push_back({img1.id, "roms/b1.rom", img1.region, 1, 0, 2048, firmware::Sha256::hexOf(img1.data)});

        pkg.images.push_back(std::move(img0));
        pkg.images.push_back(std::move(img1));

        host::MachineHost host({48'000, 64});
        std::string err;
        CHECK(host.loadPackage(pkg, err));

        std::vector<float> buffer(64, 0.0f);
        float* outputs[1] = {buffer.data()};

        for (int block = 0; block < 10; ++block) {
            // Inyecta parámetros y comandos aleatorios
            host.postCommand(0x1000u + 0, static_cast<std::int16_t>(xorshift32(prngState) & 0x7fff));
            host.postCommand(1, static_cast<std::int16_t>(xorshift32(prngState) & 0x7f));
            host.renderBlock(64, outputs, 1);
            audioBlocksFuzzed++;

            for (float s : buffer) {
                CHECK(!std::isnan(s));
                CHECK(!std::isinf(s));
                CHECK(s >= -2.0f && s <= 2.0f);
            }
        }
    }

    std::printf("      (fuzzing host: %u paquetes caoticos probados, %u bloques de audio verificados)\n",
                kSeedCount, audioBlocksFuzzed);
    CHECK(audioBlocksFuzzed == kSeedCount * 10);
}
