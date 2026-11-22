// TestThreadStress.cpp - Arnés de concurrencia y estrés multihilo real.
//
// Valida bajo contienda real entre múltiples std::thread:
//   1. Hilo de audio (renderBlock continuo con buffers variables).
//   2. Hilo de control/MIDI (postCommand en ráfagas y drain en service()).
//   3. Hilo de interfaz gráfica (lectura concurrente vía seqlock en snapshot()).
//   4. Hot-swap concurrente de paquetes de firmware durante el audio en vivo.
//   5. Cola lock-free SPSC CommandQueue bajo máxima presión sin pérdidas de orden.
//
// Diseñado para ejecución limpia tanto en entornos estándar como bajo
// ThreadSanitizer (TSan), AddressSanitizer (ASan) y UndefinedBehaviorSanitizer (UBSan).

#include "TestFramework.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "firmware/demo/DemoFirmware.h"
#include "firmware/demo/MonoVoiceFirmware.h"
#include "host/MachineHost.h"

using namespace bdmcs48;

TEST(ThreadStress, ConcurrenciaTripartitaAudioControlUI) {
    host::MachineHost host({48'000, 64});
    std::string error;
    CHECK(host.loadPackage(firmware::demo::demoPackage(), error));
    CHECK(error.empty());

    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> audioBlocksRendered{0};
    std::atomic<std::uint64_t> commandsPosted{0};
    std::atomic<std::uint64_t> snapshotsReadSuccess{0};
    std::atomic<std::uint64_t> snapshotRetries{0};
    std::atomic<bool> invalidAudioDetected{false};

    // 1. Hilo de Audio: ejecuta renderBlock continuo con buffers variables
    std::thread audioThread([&]() {
        std::vector<float> buffer(256, 0.0f);
        float* outputs[1] = {buffer.data()};
        const std::uint32_t bufferCycle[] = {64, 128, 256, 32};
        std::size_t cycleIndex = 0;

        while (!stop.load(std::memory_order_relaxed)) {
            const std::uint32_t frames = bufferCycle[cycleIndex % 4];
            cycleIndex++;
            host.renderBlock(frames, outputs, 1);

            for (std::uint32_t i = 0; i < frames; ++i) {
                if (std::isnan(buffer[i]) || std::isinf(buffer[i])) {
                    invalidAudioDetected.store(true, std::memory_order_relaxed);
                }
            }
            audioBlocksRendered.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // 2. Hilo de Control / MIDI: produce ráfagas de comandos y parametriza
    std::thread controlThread([&]() {
        std::uint32_t step = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            // Emula ráfagas de notas y cambios de parámetros en UI/MIDI
            for (int i = 0; i < 4; ++i) {
                host.postCommand(0x1000u + 0, static_cast<std::int16_t>((step * 31) % 32767));
                host.postCommand(0x1000u + 1, static_cast<std::int16_t>((step * 17) % 32767));
                host.postCommand(1, static_cast<std::int16_t>(step % 128));
                commandsPosted.fetch_add(3, std::memory_order_relaxed);
                step++;
            }
            host.service();
            std::this_thread::yield();
        }
    });

    // 3. Hilo de UI: lee continuamente instantáneas concurrentes vía seqlock
    std::thread uiThread([&]() {
        core::MachineSnapshot snap;
        while (!stop.load(std::memory_order_relaxed)) {
            if (host.snapshot(snap)) {
                snapshotsReadSuccess.fetch_add(1, std::memory_order_relaxed);
                if (std::isnan(snap.outputLevel) || std::isinf(snap.outputLevel)) {
                    invalidAudioDetected.store(true, std::memory_order_relaxed);
                }
            } else {
                snapshotRetries.fetch_add(1, std::memory_order_relaxed);
            }
            std::this_thread::yield();
        }
    });

    // Duración de la prueba de estrés: 250 ms (suficiente para miles de iteraciones sin demorar CI)
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    stop.store(true, std::memory_order_relaxed);

    audioThread.join();
    controlThread.join();
    uiThread.join();

    host.service();

    std::printf("      (tripartita: %llu bloques audio, %llu comandos, %llu lecturas UI ok, %llu reintentos seqlock)\n",
                static_cast<unsigned long long>(audioBlocksRendered.load()),
                static_cast<unsigned long long>(commandsPosted.load()),
                static_cast<unsigned long long>(snapshotsReadSuccess.load()),
                static_cast<unsigned long long>(snapshotRetries.load()));

    CHECK(!invalidAudioDetected.load());
    CHECK(audioBlocksRendered.load() > 0u);
    CHECK(commandsPosted.load() > 0u);
    CHECK(snapshotsReadSuccess.load() > 0u);
    CHECK_EQ(host.status().faulted, std::uint8_t{0});
}

TEST(ThreadStress, HotSwapConcurrenteBajoAudioYComandos) {
    host::MachineHost host({48'000, 128});
    std::string error;
    CHECK(host.loadPackage(firmware::demo::demoPackage(), error));
    CHECK(error.empty());

    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> hotSwapsAttempted{0};
    std::atomic<std::uint64_t> hotSwapsRetired{0};
    std::atomic<std::uint64_t> audioBlocks{0};

    // Hilo de audio
    std::thread audioThread([&]() {
        std::vector<float> buffer(128, 0.0f);
        float* outputs[1] = {buffer.data()};
        while (!stop.load(std::memory_order_relaxed)) {
            host.renderBlock(128, outputs, 1);
            audioBlocks.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    // Hilo de control: alterna firmware dinámicamente y drena máquinas retiradas
    std::thread controlThread([&]() {
        const auto pkg1 = firmware::demo::demoPackage();
        const auto pkg2 = firmware::demo::monoVoicePackage();
        int iteration = 0;
        std::string loadErr;

        while (!stop.load(std::memory_order_relaxed)) {
            const auto& pkg = (iteration % 2 == 0) ? pkg1 : pkg2;
            host.loadPackage(pkg, loadErr);
            hotSwapsAttempted.fetch_add(1, std::memory_order_relaxed);

            // Drenaje continuo de máquinas retiradas
            const std::size_t retired = host.service();
            hotSwapsRetired.fetch_add(retired, std::memory_order_relaxed);

            iteration++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true, std::memory_order_relaxed);

    audioThread.join();
    controlThread.join();

    // Drenaje final
    while (host.service() > 0) {}

    std::printf("      (hot-swap estrés: %llu bloques audio, %llu paquetes cargados, %llu retirados)\n",
                static_cast<unsigned long long>(audioBlocks.load()),
                static_cast<unsigned long long>(hotSwapsAttempted.load()),
                static_cast<unsigned long long>(hotSwapsRetired.load()));

    CHECK(audioBlocks.load() > 50u);
    CHECK(hotSwapsAttempted.load() > 5u);
    CHECK_EQ(host.status().faulted, std::uint8_t{0});
}

TEST(ThreadStress, CommandQueueConcurrenciaIntensaSPSC) {
    core::CommandQueue queue;
    std::atomic<bool> producerDone{false};
    constexpr std::uint32_t kTotalCommands = 50'000;

    std::thread producer([&]() {
        for (std::uint32_t i = 1; i <= kTotalCommands; ++i) {
            queue.push({i, static_cast<std::int16_t>(i & 0x7fff)});
            if (i % 64 == 0) {
                std::this_thread::yield();
            }
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::uint32_t lastReceivedId = 0;
    std::uint32_t receivedCount = 0;

    std::thread consumer([&]() {
        core::Command cmd;
        while (!producerDone.load(std::memory_order_acquire) || queue.pending() > 0) {
            while (queue.pop(cmd)) {
                // En un SPSC, los comandos recibidos deben llegar estrictamente en orden ascendente
                CHECK(cmd.id > lastReceivedId);
                lastReceivedId = cmd.id;
                receivedCount++;
            }
            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    std::printf("      (command queue SPSC estrés: %u recibidos de %u, %llu descartados por cola llena)\n",
                receivedCount, kTotalCommands, static_cast<unsigned long long>(queue.dropped()));

    // Ningún comando se corrompe en orden ni se pierde misteriosamente fuera de la cota de capacidad
    CHECK(receivedCount > 0u);
    CHECK_EQ(receivedCount + queue.dropped(), static_cast<std::uint64_t>(kTotalCommands));
}
