// TestAudioInvariants.cpp - Estrés headless e invariantes del hilo de audio.
//
// Cubre lo que pide la Fase 5 y no estaba en TestClockAndQueue:
//   * desbordamiento y colectores (LogBus, SnapshotChannel, CommandQueue)
//   * salida definida sin maquina, con firmware invalido y tras un fallo de CPU
//   * spam de comandos y cambios de reloj mientras se renderiza
//   * reset determinista de la maquina
//   * ausencia de reservas de memoria en renderBlock (contador de operator new)
//
// La politica que estos tests vigilan esta en docs/AUDIO_THREAD_POLICY.md.

#include "TestFramework.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "firmware/demo/DemoFirmware.h"
#include "host/MachineHost.h"
#include "machines/GenericMcs48Machine.h"
#include "machines/MachineFactory.h"

namespace {

namespace fs = std::filesystem;

using bdmcs48::core::CommandQueue;
using bdmcs48::core::EngineStatus;
using bdmcs48::core::LogBus;
using bdmcs48::core::LogRecord;
using bdmcs48::core::MachineSnapshot;
using bdmcs48::core::SnapshotChannel;
using bdmcs48::host::MachineHost;

// --- Contador de reservas de memoria -------------------------------------
//
// Se sustituye el operator new global del ejecutable de tests y se cuenta
// solo dentro de la ventana marcada. Sirve para demostrar que el callback no
// reserva memoria en regimen estable; no mide el coste ni el resto del host.
std::atomic<bool> gCountAllocations{false};
std::atomic<std::uint64_t> gAllocationCount{0};

bool allFinite(const std::vector<float>& samples) {
    for (float sample : samples) {
        if (!std::isfinite(sample)) {
            return false;
        }
    }
    return true;
}

bool allEqual(const std::vector<float>& samples) {
    for (float sample : samples) {
        if (sample != samples.front()) {
            return false;
        }
    }
    return !samples.empty();
}

std::vector<float> render(MachineHost& host, std::uint32_t frames, std::uint32_t blockSize) {
    std::vector<float> samples;
    samples.reserve(frames);
    std::vector<float> buffer(blockSize, 0.0f);
    std::uint32_t rendered = 0;
    while (rendered < frames) {
        const std::uint32_t count = frames - rendered < blockSize ? frames - rendered : blockSize;
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        float* outputs[1] = {buffer.data()};
        host.renderBlock(count, outputs, 1);
        host.service();
        samples.insert(samples.end(), buffer.begin(), buffer.begin() + count);
        rendered += count;
    }
    return samples;
}

// Paquete de demostracion en disco, en un directorio temporal propio.
struct TempPackage {
    fs::path root;

    TempPackage() {
        static std::atomic<unsigned> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = fs::temp_directory_path() /
               ("bdmcs48se-invariants-" + std::to_string(stamp) + "-" +
                std::to_string(counter.fetch_add(1)));
        std::error_code code;
        fs::remove_all(root, code);
    }

    ~TempPackage() {
        std::error_code code;
        fs::remove_all(root, code);
    }

    bool write(std::string& error) const {
        return bdmcs48::firmware::demo::writeDemoPackage(root.string(), error);
    }
};

// Invierte un byte de la imagen indicada para romper el SHA-256 del manifiesto.
bool corruptImage(const fs::path& root, const std::string& imageId, std::string& error) {
    const fs::path path = root / "roms" / (imageId + ".rom");
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        error = "no se pudo abrir " + path.string();
        return false;
    }
    char byte = 0;
    file.read(&byte, 1);
    if (!file) {
        error = "no se pudo leer " + path.string();
        return false;
    }
    byte = static_cast<char>(~static_cast<unsigned char>(byte));
    file.seekp(0);
    file.write(&byte, 1);
    file.flush();
    return static_cast<bool>(file);
}

} // namespace

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || defined(BDMCS48_NO_GLOBAL_NEW_OVERRIDE)
#define BDMCS48_SANITIZER_ACTIVE 1
#elif defined(__has_feature)
  #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer)
    #define BDMCS48_SANITIZER_ACTIVE 1
  #endif
#endif

#if !defined(BDMCS48_SANITIZER_ACTIVE)
void* operator new(std::size_t size) {
    if (gCountAllocations.load(std::memory_order_relaxed)) {
        gAllocationCount.fetch_add(1, std::memory_order_relaxed);
    }
    void* pointer = std::malloc(size == 0 ? 1 : size);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
#endif

// --- Colas y colectores ---------------------------------------------------

TEST(InvariantesAudio, LogBusDesbordaSinBloquearYCuenta) {
    LogBus bus;
    for (std::size_t i = 0; i < LogBus::kCapacity; ++i) {
        LogRecord record;
        record.id = static_cast<std::uint32_t>(i);
        CHECK(bus.push(record));
    }
    CHECK_EQ(bus.dropped(), std::uint64_t{0});

    LogRecord extra;
    extra.id = 9999;
    CHECK(!bus.push(extra));
    CHECK_EQ(bus.dropped(), std::uint64_t{1});
    CHECK_EQ(bus.pending(), LogBus::kCapacity);

    // FIFO: lo primero que entra es lo primero que sale.
    LogRecord out;
    CHECK(bus.pop(out));
    CHECK_EQ(out.id, std::uint32_t{0});
    CHECK_EQ(bus.pending(), LogBus::kCapacity - 1);

    // Al liberar un hueco, el productor vuelve a entrar.
    CHECK(bus.push(extra));
    CHECK_EQ(bus.dropped(), std::uint64_t{1});
}

TEST(InvariantesAudio, LaInstantaneaCoalesceYDevuelveLaUltima) {
    SnapshotChannel channel;
    for (std::uint64_t i = 1; i <= 100; ++i) {
        MachineSnapshot snapshot;
        snapshot.frameCounter = i;
        snapshot.cpuCycles = i * 10;
        snapshot.programCounter = static_cast<std::uint32_t>(i);
        std::snprintf(snapshot.message, sizeof(snapshot.message), "n%llu", static_cast<unsigned long long>(i));
        channel.publish(snapshot);
    }
    CHECK_EQ(channel.publishedCount(), std::uint64_t{100});

    // No hay cola: la UI ve el ultimo estado, no los 100 intermedios.
    MachineSnapshot out;
    CHECK(channel.read(out));
    CHECK_EQ(out.frameCounter, std::uint64_t{100});
    CHECK_EQ(out.cpuCycles, std::uint64_t{1000});
    CHECK_EQ(out.programCounter, std::uint32_t{100});
    CHECK_EQ(std::string_view(out.message), std::string_view("n100"));
}

TEST(InvariantesAudio, LaColaDeComandosDesbordaYSeRecupera) {
    CommandQueue queue;
    bdmcs48::core::Command command;
    for (std::size_t i = 0; i < CommandQueue::kCapacity + 10; ++i) {
        command.id = static_cast<std::uint32_t>(i);
        (void)queue.push(command);
    }
    CHECK_EQ(queue.dropped(), std::uint64_t{10});

    // El consumidor drena todo lo aceptado y la cola vuelve a admitir.
    bdmcs48::core::Command out;
    std::size_t popped = 0;
    while (queue.pop(out)) {
        ++popped;
    }
    CHECK_EQ(popped, CommandQueue::kCapacity);
    CHECK_EQ(queue.pending(), std::size_t{0});
    CHECK(queue.push(command));
}

// --- Salida definida ------------------------------------------------------

TEST(InvariantesAudio, SinMaquinaLaSalidaEsSilencioYNoBasura) {
    MachineHost host({44'100, 64});
    // Se ensucia el buffer: el host debe escribir silencio, no dejarlo como
    // llegue (el buffer de un DAW puede traer restos del ciclo anterior).
    std::vector<float> buffer(64, 0.5f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1);
    host.service();

    for (float sample : buffer) {
        CHECK_EQ(sample, 0.0f);
    }
    CHECK_EQ(host.framesRendered(), std::uint64_t{0});
}

TEST(InvariantesAudio, UnPaqueteInexistenteConservaLaMaquinaAnterior) {
    MachineHost host({44'100, 64});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));
    CHECK(error.empty());

    const std::vector<float> before = render(host, 512, 64);
    CHECK(allFinite(before));

    std::string missingError;
    CHECK(!host.loadPackage((fs::temp_directory_path() / "bdmcs48se-no-existe").string(),
                            missingError));
    CHECK(!missingError.empty());

    const std::vector<float> after = render(host, 512, 64);
    CHECK(allFinite(after));
    // La maquina anterior sigue sonando: no se ha quedado en silencio por el
    // intento fallido.
    bool silent = true;
    for (float sample : after) {
        if (sample != 0.0f) {
            silent = false;
            break;
        }
    }
    CHECK(!silent);
    CHECK_EQ(host.status().faulted, std::uint8_t{0});
}

TEST(InvariantesAudio, UnaRomCorruptaSeRechazaYNoTocaElAudio) {
    TempPackage package;
    std::string error;
    CHECK(package.write(error));
    CHECK(error.empty());

    MachineHost host({44'100, 64});
    CHECK(host.loadPackage(package.root.string(), error));
    CHECK(error.empty());
    const std::vector<float> before = render(host, 256, 64);

    const std::string imageId = bdmcs48::firmware::demo::demoImages().front().id;
    CHECK(corruptImage(package.root, imageId, error));

    std::string corruptError;
    CHECK(!host.loadPackage(package.root.string(), corruptError));
    CHECK(!corruptError.empty());
    // El mensaje tiene que identificar el problema, no ser un "false" mudo.
    CHECK(corruptError.find("sha256") != std::string::npos ||
          corruptError.find("SHA-256") != std::string::npos);

    const std::vector<float> after = render(host, 256, 64);
    CHECK_EQ(before.size(), after.size());
    CHECK(allFinite(after));
}

TEST(InvariantesAudio, UnPerfilDesconocidoSeRechaza) {
    bdmcs48::firmware::LoadedPackage package = bdmcs48::firmware::demo::demoPackage();
    package.manifest.machine.profile = "perfil-que-no-existe";

    MachineHost host({44'100, 64});
    std::string error;
    CHECK(!host.loadPackage(package, error));
    CHECK(error.find("desconocido") != std::string::npos);
    CHECK(error.find("perfil-que-no-existe") != std::string::npos);

    // Sin maquina valida la salida sigue siendo silencio definido.
    std::vector<float> buffer(64, 0.25f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(buffer.front(), 0.0f);
}

TEST(InvariantesAudio, UnFirmwareConOpcodeIlegalDaSalidaConstanteYFinito) {
    bdmcs48::firmware::LoadedPackage package = bdmcs48::firmware::demo::demoPackage();
    CHECK(!package.images.empty());
    CHECK(package.images.front().region == std::string("program"));
    // 0x01 es un hueco de la ISA NMOS: la CPU se detiene con IllegalOpcode.
    package.images.front().data[0] = 0x01;

    MachineHost host({44'100, 64});
    std::string error;
    CHECK(host.loadPackage(package, error));
    CHECK(error.empty());

    const std::vector<float> samples = render(host, 256, 64);
    CHECK(allFinite(samples));
    CHECK(allEqual(samples));

    const bdmcs48::core::EngineStatus status = host.status();
    CHECK_EQ(status.faulted, std::uint8_t{1});
    CHECK(status.faultMessage[0] != '\0');

    // La CPU queda parada: vuelve a renderizar y no avanza ni cambia la salida.
    const std::uint16_t stoppedPc = status.programCounter;
    const std::vector<float> later = render(host, 128, 64);
    CHECK(allEqual(later));
    CHECK_EQ(host.status().programCounter, stoppedPc);
    CHECK_EQ(host.status().faulted, std::uint8_t{1});
}

// --- Estrés ---------------------------------------------------------------

TEST(InvariantesAudio, ElSpamDeComandosNoRompeElAudio) {
    MachineHost host({44'100, 64});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));

    constexpr std::uint32_t kBlocks = 200;
    std::vector<float> all;
    all.reserve(kBlocks * 64);

    for (std::uint32_t block = 0; block < kBlocks; ++block) {
        // Muchos mas comandos que capacidad: el exceso se descarta y se cuenta.
        for (std::uint32_t i = 0; i < CommandQueue::kCapacity + 64; ++i) {
            host.postCommand(i % 2u, static_cast<std::int32_t>(i % 32768u));
        }
        std::vector<float> buffer(64, 0.0f);
        float* outputs[1] = {buffer.data()};
        host.renderBlock(64, outputs, 1);
        host.service();
        all.insert(all.end(), buffer.begin(), buffer.end());
    }

    CHECK(allFinite(all));
    CHECK(host.droppedCommands() > 0);
    CHECK_EQ(host.framesRendered(), std::uint64_t{kBlocks * 64u});
    CHECK_EQ(host.status().faulted, std::uint8_t{0});
    CHECK_EQ(host.retiredPending(), std::size_t{0});
}

TEST(InvariantesAudio, LosCambiosDeRelojRepetidosMantienenLaSalida) {
    MachineHost host({44'100, 64});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));

    constexpr std::uint32_t kBlocks = 200;
    std::vector<float> all;
    all.reserve(kBlocks * 64);
    for (std::uint32_t block = 0; block < kBlocks; ++block) {
        // Extremos del rango de engine_clock en cada bloque: 0.0625x y 8x.
        host.postCommand(0x1000u + bdmcs48::machines::GenericMcs48Machine::kParameterEngineClock,
                         (block % 2u) == 0 ? 0 : 32767);
        std::vector<float> buffer(64, 0.0f);
        float* outputs[1] = {buffer.data()};
        host.renderBlock(64, outputs, 1);
        host.service();
        all.insert(all.end(), buffer.begin(), buffer.end());
    }

    CHECK(allFinite(all));
    CHECK_EQ(host.framesRendered(), std::uint64_t{kBlocks * 64u});
    CHECK_EQ(host.status().faulted, std::uint8_t{0});
    CHECK(host.status().cpuCycles > 0);
}

TEST(InvariantesAudio, ResetDevuelveElEstadoInicialDeterminista) {
    const bdmcs48::firmware::LoadedPackage package = bdmcs48::firmware::demo::demoPackage();

    std::string error;
    auto reference = bdmcs48::machines::createMachine(package.manifest, package, error);
    CHECK(reference != nullptr);
    CHECK(error.empty());

    auto recycled = bdmcs48::machines::createMachine(package.manifest, package, error);
    CHECK(recycled != nullptr);

    std::vector<float> warmup(1024, 0.0f);
    float* warmupOut[1] = {warmup.data()};
    recycled->runBlock(1024, warmupOut, 1);
    CHECK(recycled->status().framesRendered == 1024);

    recycled->reset();

    std::vector<float> referenceSamples(512, 0.0f);
    std::vector<float> recycledSamples(512, 0.0f);
    float* referenceOut[1] = {referenceSamples.data()};
    float* recycledOut[1] = {recycledSamples.data()};
    reference->runBlock(512, referenceOut, 1);
    recycled->runBlock(512, recycledOut, 1);

    CHECK(recycledSamples == referenceSamples);
    CHECK_EQ(recycled->status().framesRendered,
             reference->status().framesRendered);
    CHECK_EQ(recycled->status().cpuCycles, reference->status().cpuCycles);
}

// --- Invariante de reservas ----------------------------------------------

TEST(InvariantesAudio, ElCallbackNoReservaMemoriaEnRegimenEstable) {
    MachineHost host({44'100, 64});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));

    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    for (int warmup = 0; warmup < 8; ++warmup) {
        host.renderBlock(64, outputs, 1);
        host.service();
    }

    gAllocationCount.store(0, std::memory_order_relaxed);
    gCountAllocations.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < 200; ++block) {
        host.renderBlock(64, outputs, 1);
    }
    gCountAllocations.store(false, std::memory_order_seq_cst);

    CHECK_EQ(gAllocationCount.load(), std::uint64_t{0});
}

TEST(InvariantesAudio, ElCaminoDeFalloAcotaSusReservas) {
    bdmcs48::firmware::LoadedPackage package = bdmcs48::firmware::demo::demoPackage();
    package.images.front().data[0] = 0x01;

    MachineHost host({44'100, 64});
    std::string error;
    CHECK(host.loadPackage(package, error));

    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1); // instala y falla
    CHECK_EQ(host.status().faulted, std::uint8_t{1});

    constexpr int kBlocks = 100;
    gAllocationCount.store(0, std::memory_order_relaxed);
    gCountAllocations.store(true, std::memory_order_seq_cst);
    for (int block = 0; block < kBlocks; ++block) {
        host.renderBlock(64, outputs, 1);
    }
    gCountAllocations.store(false, std::memory_order_seq_cst);

    // Con el mensaje de fallo como buffer de tamano fijo (char[48]) en
    // EngineStatus y MachineSnapshot, el camino de fallo no realiza ninguna
    // reserva dinamica de memoria: cero reservas en ambos caminos (nominal y fallo).
    const std::uint64_t allocations = gAllocationCount.load();
    std::printf("      (reservas con la maquina en fallo: %llu en %d bloques)\n",
                static_cast<unsigned long long>(allocations), kBlocks);
    CHECK_EQ(allocations, std::uint64_t{0});
}

TEST(InvariantesAudio, EstresAudio44100Y48000ConBuffers64A1024) {
    for (const std::uint32_t rate : {44'100u, 48'000u}) {
        MachineHost host({rate, 1024});
        std::string error;
        CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));
        CHECK(error.empty());

        // Simula la variabilidad de buffers en un DAW (64 a 1024) durante 2 segundos de audio
        const std::uint32_t bufferSizes[] = {64, 128, 256, 512, 1024, 128, 64, 512, 1024, 256};
        std::vector<float> buffer(1024, 0.0f);
        float* outputs[1] = {buffer.data()};

        const std::uint32_t totalTargetSamples = rate * 2u;
        std::uint32_t totalRendered = 0;
        std::size_t bIdx = 0;
        double energySum = 0.0;

        while (totalRendered < totalTargetSamples) {
            const std::uint32_t currentSize = bufferSizes[bIdx % (sizeof(bufferSizes) / sizeof(bufferSizes[0]))];
            bIdx++;
            const std::uint32_t count = std::min(currentSize, totalTargetSamples - totalRendered);

            std::fill(buffer.begin(), buffer.end(), 0.0f);
            host.renderBlock(count, outputs, 1);
            host.service();

            for (std::uint32_t i = 0; i < count; ++i) {
                const float s = buffer[i];
                CHECK(std::isfinite(s));
                CHECK(s >= -1.0f && s <= 1.0f);
                energySum += std::abs(s);
            }
            totalRendered += count;
        }

        // Verifica que durante 2 segundos de reproduccion se haya emitido senal valida y no silencio
        CHECK_EQ(totalRendered, totalTargetSamples);
        CHECK(energySum > 0.01);
    }
}

TEST(InvariantesAudio, CircuitBendingEstresYReversibilidad) {
    bdmcs48::machines::GenericMcs48Machine machine(
        bdmcs48::machines::GenericMcs48Machine::Config{44'100, 6'000'000, 15, 128});
    std::vector<std::uint8_t> toyRom(1024, 0x00); // NOPs
    toyRom[0] = 0x04; toyRom[1] = 0x00;           // JMP 0x0000
    bdmcs48::core::ProgramMemory program;
    program.loadBank(0, 0, toyRom.data(), toyRom.size());
    machine.setProgram(program);

    std::vector<float> buffer(256, 0.0f);
    float* outputs[1] = {buffer.data()};

    // 1. Probar degradacion extrema de cuantizacion DAC (de 8 a 1 bit)
    for (int bits = 8; bits >= 1; --bits) {
        machine.setParameter(bdmcs48::machines::GenericMcs48Machine::kParameterDacQuantize,
                             static_cast<float>(bits - 1) / 7.0f);
        CHECK_EQ(machine.dacQuantize(), static_cast<std::uint8_t>(bits));
        machine.runBlock(256, outputs, 1);
        for (float s : buffer) {
            CHECK(std::isfinite(s));
            CHECK(s >= -1.0f && s <= 1.0f);
        }
    }

    // 2. Probar Freeze (congelacion de CPU)
    machine.setParameter(bdmcs48::machines::GenericMcs48Machine::kParameterFreeze, 1.0f);
    CHECK(machine.isFrozen());
    machine.runBlock(256, outputs, 1);
    const float frozenSample = buffer[0];
    for (float s : buffer) {
        CHECK_EQ(s, frozenSample);
    }

    // Descongelar
    machine.setParameter(bdmcs48::machines::GenericMcs48Machine::kParameterFreeze, 0.0f);
    CHECK(!machine.isFrozen());
    machine.runBlock(256, outputs, 1);

    // 3. Probar Clock Bend a extremos
    machine.setParameter(bdmcs48::machines::GenericMcs48Machine::kParameterEngineClock, 0.0f); // Min scale 0.0625
    machine.runBlock(256, outputs, 1);
    machine.setParameter(bdmcs48::machines::GenericMcs48Machine::kParameterEngineClock, 1.0f); // Max scale 8.0
    machine.runBlock(256, outputs, 1);

    // 4. Reset: debe restaurar el baseline neutro
    machine.reset();
    CHECK(!machine.isFrozen());
    CHECK_EQ(machine.dacQuantize(), std::uint8_t{8});
    CHECK_NEAR(machine.clockScale(), 1.0, 1e-4);
    CHECK_EQ(machine.dacValue(), std::uint8_t{0});
}

TEST(InvariantesAudio, SnapshotMedidoresYFormaDeOndaCoalescing) {
    MachineHost host({48'000, 128});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));

    std::vector<float> buffer(128, 0.0f);
    float* outputs[1] = {buffer.data()};

    // Renderiza un bloque para generar senal activa
    host.renderBlock(128, outputs, 1);

    MachineSnapshot snap;
    CHECK(host.snapshot(snap));

    // Medidores calculados en tiempo real por el bloque
    CHECK(snap.peakLevel > 0.0f && snap.peakLevel <= 1.05f);
    CHECK(snap.rmsLevel > 0.0f && snap.rmsLevel <= snap.peakLevel);

    // Forma de onda reducida (8 muestras diezmadas)
    bool hasNonZeroWave = false;
    for (std::size_t i = 0; i < 8; ++i) {
        if (snap.waveform[i] != 0) {
            hasNonZeroWave = true;
        }
    }
    CHECK(hasNonZeroWave);

    // Reconstruccion coherente en status() de control
    const EngineStatus status = host.status();
    CHECK_NEAR(status.peakLevel, snap.peakLevel, 1e-5);
    CHECK_NEAR(status.rmsLevel, snap.rmsLevel, 1e-5);
    CHECK_EQ(std::memcmp(status.waveform, snap.waveform, sizeof(status.waveform)), 0);
}
