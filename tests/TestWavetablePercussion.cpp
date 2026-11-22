// TestWavetablePercussion.cpp - Pruebas del firmware musical de percusion lo-fi y wavetable.
//
// Verifica que el microcodigo original para MCS-48 sintetiza correctamente:
//   * Modo 0: Oscilador Wavetable con tablas en Pagina 3 leidas por MOVP3 (Senoidal, Triangular, Sierra, Chime)
//   * Modo 1: Bombo lo-fi (Kick) con barrido descendente de tono
//   * Modo 2: Caja lo-fi (Snare) con generador pseudo-aleatorio LFSR de 8 bits
//   * Modo 3: Platillo lo-fi (Hi-Hat) con modulacion metalica de alta frecuencia
//   * Determinismo estricto bit-a-bit y ausencia total de fallos de CPU (faulted == 0).

#include "TestFramework.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "firmware/demo/WavetablePercussionFirmware.h"
#include "host/MachineHost.h"

using namespace bdmcs48;

TEST(WavetablePercussion, ManifiestoEImagenesValidas) {
    const auto pkg = firmware::demo::wavetablePercussionPackage();
    CHECK_EQ(pkg.manifest.id, std::string("mcs48.musical.wavetable_percussion"));
    CHECK_EQ(pkg.manifest.name, std::string("MCS-48 Lo-Fi Percussion & Wavetable Synth"));
    CHECK_EQ(pkg.images.size(), std::size_t{1});
    CHECK_EQ(pkg.images[0].data.size(), std::size_t{2048});
    CHECK(!pkg.manifest.images[0].sha256.empty());

    // Verifica que la Pagina 3 (0x0300..0x03FF) contiene las tablas de onda
    const auto& b0 = pkg.images[0].data;
    // Tabla 0 (0x0300, Senoidal): muestra 0 = 128 (centro), muestra 16 = 252 (pico)
    CHECK_EQ(b0[0x0300], std::uint8_t{128});
    CHECK(b0[0x0300 + 16] >= 248 && b0[0x0300 + 16] <= 255);

    // Tabla 1 (0x0340, Triangular): muestra 0 = 0, muestra 32 = 255
    CHECK_EQ(b0[0x0340], std::uint8_t{0});
    CHECK_EQ(b0[0x0340 + 32], std::uint8_t{255});

    // Tabla 2 (0x0380, Sierra): rampa lineal ascendente
    CHECK_EQ(b0[0x0380], std::uint8_t{0});
    CHECK_EQ(b0[0x0380 + 10], std::uint8_t{40});
}

TEST(WavetablePercussion, ModoWavetableOsciladorGeneraAudioSinFallo) {
    const auto pkg = firmware::demo::wavetablePercussionPackage();
    host::MachineHost host({48'000, 128});
    std::string error;
    CHECK(host.loadPackage(pkg, error));
    CHECK(error.empty());

    // Puerto 2 (Latch 1): Modo 00 (Wavetable), Pitch step = 4 (bits 0-4 = 3 -> INC A = 4)
    host.postCommand(1, 0x03);

    // Puerto 1 (Latch 0): Tabla 0 (Senoidal)
    host.postCommand(0, 0x00);

    std::vector<float> buffer(128, 0.0f);
    float* outputs[1] = {buffer.data()};

    // Renderiza 5 bloques para estabilizar el acumulador de fase
    for (int i = 0; i < 5; ++i) {
        host.renderBlock(128, outputs, 1);
    }

    const auto status = host.status();
    CHECK_EQ(status.faulted, std::uint8_t{0});

    // Verifica que la senal de audio oscila limpiamente entre -1.0 y 1.0
    bool hasPositive = false;
    bool hasNegative = false;
    for (float s : buffer) {
        CHECK(!std::isnan(s));
        CHECK(!std::isinf(s));
        CHECK(s >= -1.05f && s <= 1.05f);
        if (s > 0.1f) hasPositive = true;
        if (s < -0.1f) hasNegative = true;
    }
    CHECK(hasPositive && hasNegative);
}

TEST(WavetablePercussion, ConmutacionEntreLasCuatroTablasProduceTimbresDistintos) {
    const auto pkg = firmware::demo::wavetablePercussionPackage();
    host::MachineHost host({48'000, 128});
    std::string error;
    CHECK(host.loadPackage(pkg, error));

    // Modo Wavetable con Pitch step = 2
    host.postCommand(1, 0x01);

    std::vector<std::vector<float>> tableOutputs(4, std::vector<float>(128, 0.0f));

    for (int table = 0; table < 4; ++table) {
        host.postCommand(0, table); // Selecciona tabla 0, 1, 2, 3
        float* outputs[1] = {tableOutputs[table].data()};
        for (int b = 0; b < 3; ++b) {
            host.renderBlock(128, outputs, 1);
        }
        const auto status = host.status();
        CHECK_EQ(status.faulted, std::uint8_t{0});
    }

    // Comprueba que las tablas de onda producen formas de onda distintas
    bool distinctWaveforms = false;
    for (std::size_t i = 0; i < 128; ++i) {
        if (std::abs(tableOutputs[0][i] - tableOutputs[2][i]) > 0.05f) {
            distinctWaveforms = true;
            break;
        }
    }
    CHECK(distinctWaveforms);
}

TEST(WavetablePercussion, ModoPercusionKickSnareHiHat) {
    const auto pkg = firmware::demo::wavetablePercussionPackage();
    host::MachineHost host({48'000, 128});
    std::string error;
    CHECK(host.loadPackage(pkg, error));

    std::vector<float> buffer(128, 0.0f);
    float* outputs[1] = {buffer.data()};

    // 1. Probar Modo Kick Drum (bits 7-6 = 01 -> Latch 1 = 0x40)
    host.postCommand(1, 0x40);
    for (int b = 0; b < 4; ++b) {
        host.renderBlock(128, outputs, 1);
    }
    auto status = host.status();
    CHECK_EQ(status.faulted, std::uint8_t{0});
    bool kickActive = false;
    for (float s : buffer) {
        if (std::abs(s) > 0.05f) kickActive = true;
    }
    CHECK(kickActive);

    // 2. Probar Modo Snare Drum (bits 7-6 = 10 -> Latch 1 = 0x80)
    host.postCommand(1, 0x80);
    for (int b = 0; b < 4; ++b) {
        host.renderBlock(128, outputs, 1);
    }
    status = host.status();
    CHECK_EQ(status.faulted, std::uint8_t{0});
    bool snareActive = false;
    for (float s : buffer) {
        if (std::abs(s) > 0.05f) snareActive = true;
    }
    CHECK(snareActive);

    // 3. Probar Modo Hi-Hat (bits 7-6 = 11 -> Latch 1 = 0xC0)
    host.postCommand(1, 0xC0);
    for (int b = 0; b < 4; ++b) {
        host.renderBlock(128, outputs, 1);
    }
    status = host.status();
    CHECK_EQ(status.faulted, std::uint8_t{0});
    bool hihatActive = false;
    for (float s : buffer) {
        if (std::abs(s) > 0.05f) hihatActive = true;
    }
    CHECK(hihatActive);
}

TEST(WavetablePercussion, DeterminismoEstrictoDeSintesis) {
    const auto pkg = firmware::demo::wavetablePercussionPackage();

    auto runSession = [&]() -> std::vector<float> {
        host::MachineHost host({48'000, 128});
        std::string err;
        CHECK(host.loadPackage(pkg, err));
        host.postCommand(1, 0x45); // Modo Kick con tono especifico
        host.postCommand(0, 0x02);

        std::vector<float> totalAudio;
        std::vector<float> block(128, 0.0f);
        float* outputs[1] = {block.data()};
        for (int i = 0; i < 5; ++i) {
            host.renderBlock(128, outputs, 1);
            totalAudio.insert(totalAudio.end(), block.begin(), block.end());
        }
        return totalAudio;
    };

    const auto runA = runSession();
    const auto runB = runSession();

    CHECK_EQ(runA.size(), runB.size());
    bool nonZero = false;
    for (std::size_t i = 0; i < runA.size(); ++i) {
        CHECK_EQ(runA[i], runB[i]);
        if (std::abs(runA[i]) > 0.05f) {
            nonZero = true;
        }
    }
    CHECK(nonZero);
}
