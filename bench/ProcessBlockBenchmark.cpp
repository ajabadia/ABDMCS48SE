// ProcessBlockBenchmark.cpp - Coste de renderBlock() por bloque de audio.
//
// Mide el camino completo que usa el host (maquina MCS-48 + DAC + reloj) sin
// dependencias externas: solo std::chrono. Para cada tamano de bloque de los
// que usa un plugin (64, 128, 256 y 512 muestras) publica milisegundos por
// bloque y el factor de tiempo real (cuantas veces mas rapido que el audio).
//
// Uso:
//   bdmcs48se_bench            informe legible de los cuatro tamanos
//   bdmcs48se_bench --smoke    ademas falla si algun tamano baja del umbral
//
// El umbral del modo --smoke es deliberadamente bajo: sirve para detectar una
// regresion de orden de magnitud (algo que bloquea o que recorre el bloque
// varias veces), no para medir la maquina de desarrollo.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "firmware/demo/DemoFirmware.h"
#include "host/MachineHost.h"

namespace {

namespace fs = std::filesystem;

constexpr std::uint32_t kSampleRate = 48'000;
// Con el perfil actual (6 MHz / 15 oscilaciones por ciclo) un bloque de 512
// muestras necesita ~2.700 ciclos de CPU: sobra margen incluso en Debug.
constexpr double kMinRealtimeFactor = 4.0;
constexpr double kSecondsPerSize = 0.5;

struct Measurement {
    std::uint32_t blockSize = 0;
    double millisecondsPerBlock = 0.0;
    double realtimeFactor = 0.0;
};

std::string makeTempPackage() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("bdmcs48se-bench-" + std::to_string(stamp));
    std::string error;
    if (!bdmcs48::firmware::demo::writeDemoPackage(root.string(), error)) {
        std::fprintf(stderr, "no se pudo escribir el paquete de demo: %s\n", error.c_str());
        return {};
    }
    return root.string();
}

Measurement measure(std::uint32_t blockSize, const std::string& package) {
    using Clock = std::chrono::steady_clock;

    bdmcs48::host::MachineHost host({kSampleRate, blockSize});
    std::string error;
    if (!host.loadPackage(package, error)) {
        std::fprintf(stderr, "no se pudo cargar el paquete: %s\n", error.c_str());
        return {};
    }

    std::vector<float> buffer(blockSize, 0.0f);
    float* outputs[1] = {buffer.data()};

    // Calentamiento: primera instalacion de la maquina e inicializacion de
    // la cache. No entra en la medida.
    for (int i = 0; i < 4; ++i) {
        host.renderBlock(blockSize, outputs, 1);
        host.service();
    }

    const auto totalFrames = static_cast<std::uint64_t>(kSecondsPerSize * kSampleRate);
    const std::uint64_t blocks = totalFrames / blockSize;

    const auto start = Clock::now();
    for (std::uint64_t i = 0; i < blocks; ++i) {
        host.renderBlock(blockSize, outputs, 1);
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - start).count();

    Measurement result;
    result.blockSize = blockSize;
    result.millisecondsPerBlock = (elapsed * 1000.0) / static_cast<double>(blocks);
    const double audioSeconds = static_cast<double>(blocks * blockSize) / kSampleRate;
    result.realtimeFactor = audioSeconds / elapsed;
    return result;
}

} // namespace

int main(int argc, char** argv) {
    bool smoke = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--smoke") {
            smoke = true;
        } else if (argument == "--help" || argument == "-h") {
            std::printf("uso: bdmcs48se_bench [--smoke]\n");
            return 0;
        } else {
            std::fprintf(stderr, "argumento desconocido: %s\n", argument.c_str());
            return 2;
        }
    }

    const std::string package = makeTempPackage();
    if (package.empty()) {
        return 2;
    }

    std::printf("MCS-48 processBlock a %u Hz (sin JUCE, solo el core)\n", kSampleRate);
    std::printf("%10s %16s %16s\n", "bloque", "ms/bloque", "tiempo real x");

    bool belowThreshold = false;
    const std::uint32_t sizes[] = {64, 128, 256, 512};
    for (std::uint32_t blockSize : sizes) {
        const Measurement measurement = measure(blockSize, package);
        if (measurement.blockSize == 0) {
            std::error_code code;
            fs::remove_all(package, code);
            return 2;
        }
        std::printf("%10u %16.4f %16.1f\n", measurement.blockSize,
                    measurement.millisecondsPerBlock, measurement.realtimeFactor);
        if (measurement.realtimeFactor < kMinRealtimeFactor) {
            belowThreshold = true;
        }
    }

    std::error_code code;
    fs::remove_all(package, code);

    if (smoke) {
        if (belowThreshold) {
            std::fprintf(stderr, "[FALLO] algun tamano de bloque no llega a x%.0f tiempo real\n",
                         kMinRealtimeFactor);
            return 1;
        }
        std::printf("[OK] todos los tamanos superan x%.0f tiempo real\n", kMinRealtimeFactor);
    }
    return 0;
}
