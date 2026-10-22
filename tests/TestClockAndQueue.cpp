#include "TestFramework.h"

#include "core/CommandQueue.h"
#include "core/CoreClock.h"

using bdmcs48::core::Command;
using bdmcs48::core::CommandQueue;
using bdmcs48::core::CoreClock;

TEST(CoreClock, UnSegundoDeAudioSon400000Ciclos) {
    // 6 MHz / 15 oscilaciones por ciclo = 400000 ciclos de maquina por
    // segundo, a 44100 Hz de audio.
    CoreClock clock(6'000'000, 15, 44'100);
    CHECK_EQ(clock.cyclesForSamples(44'100), std::uint64_t{400'000});
    CHECK_EQ(clock.totalCycles(), std::uint64_t{400'000});
}

TEST(CoreClock, NoHayDerivaAlAcumularBloques) {
    // El error de truncado se acumularia si el cociente se truncase en
    // cada llamada. Con el resto acumulado, el total es exacto.
    CoreClock clock(6'000'000, 15, 44'100);
    std::uint64_t total = 0;
    for (int i = 0; i < 44'100; ++i) {
        total += clock.cyclesForSamples(1);
    }
    CHECK_EQ(total, std::uint64_t{400'000});
    CHECK_EQ(clock.totalCycles(), std::uint64_t{400'000});
}

TEST(CoreClock, OtraFrecuenciaDeAudio) {
    CoreClock clock(6'000'000, 15, 48'000);
    CHECK_EQ(clock.cyclesForSamples(48'000), std::uint64_t{400'000});
    CHECK_EQ(clock.cyclesForSamples(96'000), std::uint64_t{800'000});
}

TEST(CoreClock, RelojDePrueba) {
    // 1 MHz / 1 oscilacion = 1 ciclo por microsegundo.
    CoreClock clock(1'000'000, 1, 1'000'000);
    CHECK_EQ(clock.cyclesForSamples(1'000'000), std::uint64_t{1'000'000});
    CHECK_NEAR(clock.cpuCyclesPerSecond(), 1'000'000.0, 0.5);
    CHECK_NEAR(clock.secondsForSamples(44'100), 0.0441, 1e-9);
}

TEST(CoreClock, ReiniciarDejaElEstadoACero) {
    CoreClock clock(6'000'000, 15, 44'100);
    clock.cyclesForSamples(1000);
    clock.reset();
    CHECK_EQ(clock.totalCycles(), std::uint64_t{0});
    CHECK_EQ(clock.cyclesForSamples(44'100), std::uint64_t{400'000});
}

TEST(CommandQueue, OrdenFifo) {
    CommandQueue queue;
    for (std::uint32_t i = 0; i < 8; ++i) {
        Command command;
        command.id = i;
        command.value = static_cast<std::int32_t>(i * 10);
        CHECK(queue.push(command));
    }
    CHECK_EQ(queue.pending(), std::size_t{8});
    for (std::uint32_t i = 0; i < 8; ++i) {
        Command command;
        CHECK(queue.pop(command));
        CHECK_EQ(command.id, i);
        CHECK_EQ(command.value, static_cast<std::int32_t>(i * 10));
    }
    Command command;
    CHECK(!queue.pop(command));
    CHECK_EQ(queue.pending(), std::size_t{0});
}

TEST(CommandQueue, ColaLlenaDescartaYLoCuenta) {
    CommandQueue queue;
    for (std::size_t i = 0; i < CommandQueue::kCapacity; ++i) {
        Command command;
        command.id = static_cast<std::uint32_t>(i);
        CHECK(queue.push(command));
    }
    Command overflow;
    overflow.id = 9999;
    CHECK(!queue.push(overflow));
    CHECK_EQ(queue.dropped(), std::uint64_t{1});
    CHECK_EQ(queue.pending(), CommandQueue::kCapacity);
}

TEST(CommandQueue, VaciarDescartaLoPendiente) {
    CommandQueue queue;
    Command command;
    command.id = 1;
    CHECK(queue.push(command));
    queue.clear();
    CHECK_EQ(queue.pending(), std::size_t{0});
    CHECK(!queue.pop(command));
}
