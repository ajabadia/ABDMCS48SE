// ProcessBlockBenchmark.cpp - Coste de renderBlock() en la matriz de audio.
//
// Mide el camino completo que usa el host (maquina MCS-48 + reloj + DAC) sin
// JUCE y sin DAW, con std::chrono como unica dependencia. La matriz cubre los
// objetivos declarados en las fases 3 y 5 del roadmap:
//
//   * frecuencia de muestreo: 44 100 y 48 000 Hz
//   * tamano de bloque: 64, 128, 256, 512 y 1024 muestras
//   * instancias simultaneas: 1, 2 y 4 (cada una con su propio MachineHost)
//
// Para cada combinacion publica milisegundos por bloque (total y por
// instancia), el factor de tiempo real y el porcentaje de un nucleo que
// consume una instancia. No hay umbral de presupuesto acordado: el benchmark
// informa, y solo el modo --smoke falla si una regresion de orden de magnitud
// deja el render por debajo de un minimo evidente.
//
// Uso:
//   bdmcs48se_bench                        matriz completa (tabla legible)
//   bdmcs48se_bench --csv                  la misma matriz en CSV
//   bdmcs48se_bench --smoke                subconjunto rapido + umbral
//   bdmcs48se_bench --blocks 64,1024 --instances 1,8
//   bdmcs48se_bench --sample-rates 48000 --seconds 2 --repeats 5
//
// Metodo: se descarta el calentamiento (instalacion de la maquina y cache),
// se mide el mismo fragmento de audio `repeats` veces y se publica el minimo,
// que es la medida menos contaminada por el planificador. `seconds` es tiempo
// de audio por medida, no tiempo de pared.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "firmware/demo/DemoFirmware.h"
#include "host/MachineHost.h"

namespace {

using Clock = std::chrono::steady_clock;

// Umbral del modo --smoke: deliberadamente bajo. Detecta un bloqueo o un
// recorrido multiple del bloque, no caracteriza la maquina de desarrollo.
constexpr double kMinRealtimeFactor = 4.0;
constexpr int kSmokeWarmupBlocks = 4;

struct Options {
    bool smoke = false;
    bool csv = false;
    std::vector<std::uint32_t> sampleRates{44'100, 48'000};
    std::vector<std::uint32_t> blockSizes{64, 128, 256, 512, 1024};
    std::vector<std::uint32_t> instances{1, 2, 4};
    double seconds = 1.0;
    int repeats = 3;
};

struct Measurement {
    std::uint32_t sampleRateHz = 0;
    std::uint32_t blockSize = 0;
    std::uint32_t instanceCount = 0;
    double audioSeconds = 0.0;
    double millisecondsPerBlockTotal = 0.0;
    double millisecondsPerBlockInstance = 0.0;
    double realtimeFactorTotal = 0.0;
    double realtimeFactorInstance = 0.0;
};

bool parseList(const char* text, std::vector<std::uint32_t>& out) {
    std::vector<std::uint32_t> values;
    std::istringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            continue;
        }
        char* end = nullptr;
        const unsigned long value = std::strtoul(token.c_str(), &end, 10);
        if (end == nullptr || *end != '\0' || value == 0 || value > 10'000'000ul) {
            return false;
        }
        values.push_back(static_cast<std::uint32_t>(value));
    }
    if (values.empty()) {
        return false;
    }
    out = values;
    return true;
}

// Mide `instances` hosts independientes renderizando el mismo fragmento de
// audio. Devuelve una medida vacia (sampleRateHz == 0) si algo no carga.
Measurement measure(std::uint32_t sampleRateHz, std::uint32_t blockSize,
                    std::uint32_t instanceCount, double seconds, int repeats) {
    const bdmcs48::firmware::LoadedPackage package = bdmcs48::firmware::demo::demoPackage();

    std::vector<std::unique_ptr<bdmcs48::host::MachineHost>> hosts;
    hosts.reserve(instanceCount);
    for (std::uint32_t index = 0; index < instanceCount; ++index) {
        auto host = std::make_unique<bdmcs48::host::MachineHost>(
            bdmcs48::host::MachineHost::Config{sampleRateHz, blockSize});
        std::string error;
        if (!host->loadPackage(package, error)) {
            std::fprintf(stderr, "no se pudo cargar el paquete (%u Hz, %u): %s\n",
                         sampleRateHz, blockSize, error.c_str());
            return {};
        }
        hosts.push_back(std::move(host));
    }

    std::vector<std::vector<float>> buffers(instanceCount);
    for (auto& buffer : buffers) {
        buffer.assign(blockSize, 0.0f);
    }

    const std::uint64_t blocks =
        static_cast<std::uint64_t>(seconds * static_cast<double>(sampleRateHz)) / blockSize;
    if (blocks == 0) {
        return {};
    }

    // Calentamiento: primera instalacion de la maquina y cache tibia.
    for (int warmup = 0; warmup < kSmokeWarmupBlocks; ++warmup) {
        for (std::uint32_t index = 0; index < instanceCount; ++index) {
            float* outputs[1] = {buffers[index].data()};
            hosts[index]->renderBlock(blockSize, outputs, 1);
            hosts[index]->service();
        }
    }

    double bestSeconds = 0.0;
    for (int repetition = 0; repetition < repeats; ++repetition) {
        const auto start = Clock::now();
        for (std::uint64_t block = 0; block < blocks; ++block) {
            for (std::uint32_t index = 0; index < instanceCount; ++index) {
                float* outputs[1] = {buffers[index].data()};
                hosts[index]->renderBlock(blockSize, outputs, 1);
            }
        }
        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        if (repetition == 0 || elapsed < bestSeconds) {
            bestSeconds = elapsed;
        }
    }

    const double audioSeconds = static_cast<double>(blocks * blockSize) /
                                static_cast<double>(sampleRateHz);
    Measurement result;
    result.sampleRateHz = sampleRateHz;
    result.blockSize = blockSize;
    result.instanceCount = instanceCount;
    result.audioSeconds = audioSeconds;
    result.millisecondsPerBlockTotal = (bestSeconds * 1000.0) / static_cast<double>(blocks);
    result.millisecondsPerBlockInstance =
        result.millisecondsPerBlockTotal / static_cast<double>(instanceCount);
    result.realtimeFactorTotal = bestSeconds > 0.0 ? audioSeconds / bestSeconds : 0.0;
    // Cada instancia renderiza los mismos `audioSeconds`: el factor por
    // instancia es el total multiplicado por el numero de instancias.
    result.realtimeFactorInstance =
        result.realtimeFactorTotal * static_cast<double>(instanceCount);
    return result;
}

void printCsvHeader() {
    std::printf("sample_rate_hz,block_size,instances,audio_seconds,ms_per_block_total,"
                "ms_per_block_instance,realtime_factor_total,realtime_factor_instance,"
                "cpu_percent_one_core_per_instance\n");
}

void printCsvRow(const Measurement& measurement) {
    const double cpuPercent = measurement.realtimeFactorInstance > 0.0
                                  ? 100.0 / measurement.realtimeFactorInstance
                                  : 0.0;
    std::printf("%u,%u,%u,%.2f,%.4f,%.4f,%.1f,%.1f,%.2f\n",
                measurement.sampleRateHz, measurement.blockSize, measurement.instanceCount,
                measurement.audioSeconds, measurement.millisecondsPerBlockTotal,
                measurement.millisecondsPerBlockInstance, measurement.realtimeFactorTotal,
                measurement.realtimeFactorInstance, cpuPercent);
}

void printTableHeader(std::uint32_t sampleRateHz, double seconds, int repeats) {
    std::printf("\n%u Hz - %.2f s de audio por medida, minimo de %d repeticiones\n",
                sampleRateHz, seconds, repeats);
    std::printf("%8s %5s %16s %16s %14s %14s %10s\n",
                "bloque", "inst", "ms/bloque tot", "ms/bloque inst", "tiempo real", "x inst",
                "CPU % inst");
}

void printTableRow(const Measurement& measurement) {
    std::printf("%8u %5u %16.4f %16.4f %14.1f %14.1f %10.2f\n",
                measurement.blockSize, measurement.instanceCount,
                measurement.millisecondsPerBlockTotal, measurement.millisecondsPerBlockInstance,
                measurement.realtimeFactorTotal, measurement.realtimeFactorInstance,
                measurement.realtimeFactorInstance > 0.0
                    ? 100.0 / measurement.realtimeFactorInstance
                    : 0.0);
}

void printHelp() {
    std::printf(
        "uso: bdmcs48se_bench [opciones]\n"
        "  --smoke                    subconjunto rapido y umbral de tiempo real\n"
        "  --csv                      salida CSV en lugar de tabla\n"
        "  --sample-rates LISTA       por defecto 44100,48000\n"
        "  --blocks LISTA             por defecto 64,128,256,512,1024\n"
        "  --instances LISTA          por defecto 1,2,4\n"
        "  --seconds N                audio por medida (defecto 1.0)\n"
        "  --repeats N                repeticiones por medida (defecto 3)\n"
        "  --help                     esta ayuda\n");
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto nextValue = [&](std::vector<std::uint32_t>& target) {
            return i + 1 < argc && parseList(argv[++i], target);
        };

        if (argument == "--smoke") {
            options.smoke = true;
        } else if (argument == "--csv") {
            options.csv = true;
        } else if (argument == "--sample-rates") {
            if (!nextValue(options.sampleRates)) {
                std::fprintf(stderr, "lista invalida en --sample-rates\n");
                return 2;
            }
        } else if (argument == "--blocks") {
            if (!nextValue(options.blockSizes)) {
                std::fprintf(stderr, "lista invalida en --blocks\n");
                return 2;
            }
        } else if (argument == "--instances") {
            if (!nextValue(options.instances)) {
                std::fprintf(stderr, "lista invalida en --instances\n");
                return 2;
            }
        } else if (argument == "--seconds") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "falta el valor de --seconds\n");
                return 2;
            }
            options.seconds = std::strtod(argv[++i], nullptr);
            if (options.seconds <= 0.0) {
                std::fprintf(stderr, "--seconds debe ser positivo\n");
                return 2;
            }
        } else if (argument == "--repeats") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "falta el valor de --repeats\n");
                return 2;
            }
            options.repeats = std::atoi(argv[++i]);
            if (options.repeats <= 0) {
                std::fprintf(stderr, "--repeats debe ser positivo\n");
                return 2;
            }
        } else if (argument == "--help" || argument == "-h") {
            printHelp();
            return 0;
        } else {
            std::fprintf(stderr, "argumento desconocido: %s\n", argument.c_str());
            printHelp();
            return 2;
        }
    }

    // El modo --smoke es el que corre en CTest: subconjunto minimo, una sola
    // repeticion y una sola instancia, pero cubriendo todos los tamanos.
    if (options.smoke) {
        options.sampleRates = {48'000};
        options.blockSizes = {64, 128, 256, 512, 1024};
        options.instances = {1};
        options.seconds = 0.5;
        options.repeats = 1;
    }

    if (options.csv) {
        printCsvHeader();
    }

    bool belowThreshold = false;
    std::size_t measured = 0;
    for (std::uint32_t sampleRateHz : options.sampleRates) {
        if (!options.csv) {
            printTableHeader(sampleRateHz, options.seconds, options.repeats);
        }
        for (std::uint32_t blockSize : options.blockSizes) {
            for (std::uint32_t instanceCount : options.instances) {
                const Measurement measurement = measure(sampleRateHz, blockSize, instanceCount,
                                                        options.seconds, options.repeats);
                if (measurement.sampleRateHz == 0) {
                    return 2;
                }
                ++measured;
                if (options.csv) {
                    printCsvRow(measurement);
                } else {
                    printTableRow(measurement);
                }
                if (measurement.realtimeFactorInstance < kMinRealtimeFactor) {
                    belowThreshold = true;
                }
            }
        }
    }

    if (!options.csv) {
        std::printf("\n%d medidas. Tiempo real por instancia: (audio de una instancia) / "
                    "(pared). CPU %% inst: 100 / ese factor.\n",
                    static_cast<int>(measured));
    }

    if (options.smoke) {
        if (belowThreshold) {
            std::fprintf(stderr, "[FALLO] alguna combinacion no llega a x%.0f tiempo real\n",
                         kMinRealtimeFactor);
            return 1;
        }
        std::printf("[OK] la matriz de humo supera x%.0f tiempo real por instancia\n",
                    kMinRealtimeFactor);
    }
    return 0;
}
