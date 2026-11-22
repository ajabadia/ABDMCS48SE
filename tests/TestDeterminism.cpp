// TestDeterminism.cpp - Determinismo del render (Fase 3).
//
// Criterio: mismo firmware + misma configuracion -> mismo audio, byte a byte.
// No se compara con una tolerancia: se compara el SHA-256 del render completo
// contra un golden versionado en tests/data/determinism/.
//
// El digest se calcula sobre una representacion canonica: cada muestra como
// 4 bytes IEEE-754 binary32 en little-endian explicito. Asi el golden no
// depende del endianness del host. El camino de render del perfil generico no
// tiene PRNG ni filtros: la muestra es la LUT del DAC indexada por el valor
// que el firmware escribio en P1 (ver GenericMcs48Machine::runBlock), de modo
// que el resultado es reproducible entre compiladores mientras la aritmetica
// sea IEEE-754.
//
// Si un cambio de comportamiento es intencionado, hay que regenerar el golden:
// el test falla mostrando el digest real, que se copia al fichero.
//
// Verificado con dos compiladores y dos runtimes distintos (MSVC 14.51 + MSVC
// CRT y Clang 19.1.7 + libc++/mingw-w64): ambos dan exactamente los digests
// versionados, asi que el criterio sigue siendo byte-exacto y no una tolerancia.
// Evidencia: docs/evidence/BDMCS48SE-DET-01/. Linux/macOS se cubriran en CI.

#include "TestFramework.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "firmware/Sha256.h"
#include "firmware/demo/DemoFirmware.h"
#include "host/MachineHost.h"

namespace {

using bdmcs48::firmware::Sha256;
using bdmcs48::host::MachineHost;

#ifndef BDMCS48SE_SOURCE_DIR
#error "Falta la definicion BDMCS48SE_SOURCE_DIR: el test necesita localizar los goldens."
#endif

constexpr std::uint32_t kChannels = 1;

struct Golden {
    std::string name;
    std::uint32_t sampleRateHz = 0;
    std::uint32_t blockSize = 0;
    std::uint32_t frames = 0;
    std::string digest;
};

// SHA-256 del audio en forma canonica (float32 little-endian por muestra).
std::string digestOf(const std::vector<float>& samples) {
    Sha256 hash;
    for (float sample : samples) {
        std::uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(sample), "float debe ser binary32");
        std::memcpy(&bits, &sample, sizeof(bits));
        const std::uint8_t littleEndian[4] = {
            static_cast<std::uint8_t>(bits & 0xffu),
            static_cast<std::uint8_t>((bits >> 8) & 0xffu),
            static_cast<std::uint8_t>((bits >> 16) & 0xffu),
            static_cast<std::uint8_t>((bits >> 24) & 0xffu),
        };
        hash.update(littleEndian, sizeof(littleEndian));
    }
    return hash.hexDigest();
}

std::string goldenPath() {
    return std::string(BDMCS48SE_SOURCE_DIR) + "/tests/data/determinism/render-goldens.txt";
}

// Formato: nombre sampleRate blockSize frames sha256. Las lineas vacias y las
// que empiezan por '#' son comentarios.
std::vector<Golden> loadGoldens() {
    std::ifstream file(goldenPath());
    CHECK(file.is_open());

    std::vector<Golden> goldens;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream stream(line);
        Golden golden;
        CHECK(stream >> golden.name >> golden.sampleRateHz >> golden.blockSize >> golden.frames
                      >> golden.digest);
        CHECK_EQ(golden.digest.size(), std::size_t{64});
        goldens.push_back(golden);
    }
    return goldens;
}

// Renderiza `frames` muestras del firmware de demostracion en bloques de
// `blockSize`. Cada llamada crea un host nuevo: no se reutiliza estado.
std::vector<float> renderDemo(std::uint32_t sampleRateHz, std::uint32_t blockSize,
                              std::uint32_t frames, std::uint32_t channels = kChannels,
                              std::vector<float>* secondChannel = nullptr) {
    MachineHost host({sampleRateHz, blockSize});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));
    CHECK(error.empty());

    std::vector<float> block(static_cast<std::size_t>(blockSize) * channels, 0.0f);
    std::vector<float> samples;
    samples.reserve(static_cast<std::size_t>(frames) * channels);

    std::uint32_t rendered = 0;
    while (rendered < frames) {
        const std::uint32_t count = std::min(blockSize, frames - rendered);
        std::fill(block.begin(), block.end(), 0.0f);
        std::vector<float*> outputs(channels, nullptr);
        for (std::uint32_t channel = 0; channel < channels; ++channel) {
            outputs[channel] = block.data() + static_cast<std::size_t>(channel) * blockSize;
        }
        host.renderBlock(count, outputs.data(), channels);
        host.service();

        for (std::uint32_t frame = 0; frame < count; ++frame) {
            if (secondChannel != nullptr) {
                secondChannel->push_back(block[static_cast<std::size_t>(blockSize) + frame]);
            }
            samples.push_back(block[frame]);
        }
        rendered += count;
    }
    host.service();
    return samples;
}

// Renderiza `frames` muestras del firmware de demostracion alternando
// tamanos de bloque de una secuencia arbitraria.
std::vector<float> renderDemoWithVariableBlocks(std::uint32_t sampleRateHz,
                                                const std::vector<std::uint32_t>& blockSizes,
                                                std::uint32_t frames) {
    MachineHost host({sampleRateHz, 1024});
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::demoPackage(), error));
    CHECK(error.empty());

    std::vector<float> samples;
    samples.reserve(frames);
    std::vector<float> buffer(1024, 0.0f);
    float* outputs[1] = {buffer.data()};

    std::size_t bIndex = 0;
    std::uint32_t rendered = 0;
    while (rendered < frames) {
        const std::uint32_t targetBlock = blockSizes[bIndex % blockSizes.size()];
        bIndex++;
        const std::uint32_t count = std::min(targetBlock, frames - rendered);
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        host.renderBlock(count, outputs, 1);
        host.service();

        for (std::uint32_t i = 0; i < count; ++i) {
            samples.push_back(buffer[i]);
        }
        rendered += count;
    }
    return samples;
}

} // namespace

TEST(Determinismo, DosRendersIgualesDanElMismoDigest) {
    const std::vector<float> first = renderDemo(44'100, 256, 44'100);
    const std::vector<float> second = renderDemo(44'100, 256, 44'100);

    CHECK_EQ(first.size(), std::size_t{44'100});
    CHECK_EQ(digestOf(first), digestOf(second));
    CHECK(digestOf(first) != digestOf(std::vector<float>(44'100, 0.0f)));
}

TEST(Determinismo, ElRenderCoincideConElGolden) {
    const std::vector<Golden> goldens = loadGoldens();
    CHECK_EQ(goldens.size(), std::size_t{2});

    for (const Golden& golden : goldens) {
        const std::vector<float> render = renderDemo(golden.sampleRateHz, golden.blockSize,
                                                     golden.frames);
        CHECK_EQ(render.size(), static_cast<std::size_t>(golden.frames));
        // El mensaje del fallo muestra el digest real: es lo que se copia al
        // golden cuando el cambio de comportamiento es intencionado.
        CHECK_EQ(digestOf(render), golden.digest);
    }
}

TEST(Determinismo, ElTamanoDeBloqueNoAlteraLaSalida) {
    const std::vector<Golden> goldens = loadGoldens();
    CHECK(!goldens.empty());
    const Golden& reference = goldens.front();

    // El DAC se evalua por muestra y el reloj arrastra el sobrante de la
    // instruccion en curso, asi que partir el mismo audio en bloques
    // distintos no puede cambiar ni una muestra.
    for (std::uint32_t blockSize : {64u, 128u, 256u, 1024u}) {
        const std::vector<float> render = renderDemo(reference.sampleRateHz, blockSize,
                                                     reference.frames);
        CHECK_EQ(render.size(), static_cast<std::size_t>(reference.frames));
        CHECK_EQ(digestOf(render), reference.digest);
    }
}

TEST(Determinismo, ElTamanoDeBloqueNoAlteraLaSalida48k) {
    const std::vector<Golden> goldens = loadGoldens();
    CHECK_EQ(goldens.size(), std::size_t{2});
    const Golden& golden48k = goldens[1];

    for (std::uint32_t blockSize : {64u, 128u, 256u, 512u, 1024u}) {
        const std::vector<float> render = renderDemo(golden48k.sampleRateHz, blockSize,
                                                     golden48k.frames);
        CHECK_EQ(render.size(), static_cast<std::size_t>(golden48k.frames));
        CHECK_EQ(digestOf(render), golden48k.digest);
    }
}

TEST(Determinismo, EstresBuffersVariables44100Y48000CoincidenConGolden) {
    const std::vector<Golden> goldens = loadGoldens();
    CHECK_EQ(goldens.size(), std::size_t{2});

    const std::vector<std::uint32_t> variableBlocks = {64, 128, 512, 64, 1024, 256, 64, 512};

    for (const Golden& golden : goldens) {
        const std::vector<float> variableRender = renderDemoWithVariableBlocks(
            golden.sampleRateHz, variableBlocks, golden.frames);
        CHECK_EQ(variableRender.size(), static_cast<std::size_t>(golden.frames));
        CHECK_EQ(digestOf(variableRender), golden.digest);
    }
}

TEST(Determinismo, LosCanalesRecibenLaMismaMuestra) {
    std::vector<float> right;
    const std::vector<float> left = renderDemo(44'100, 256, 4'410, 2, &right);

    CHECK_EQ(left.size(), std::size_t{4'410});
    CHECK_EQ(right.size(), std::size_t{4'410});
    CHECK(left == right);
}
