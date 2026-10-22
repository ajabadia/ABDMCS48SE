#include "TestFramework.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "firmware/demo/MonoVoiceFirmware.h"
#include "host/MachineHost.h"
#include "instrument/MonoVoice.h"
#include "machines/GenericMcs48Machine.h"

namespace {

using bdmcs48::host::MachineHost;
using bdmcs48::instrument::MonoVoice;
using bdmcs48::instrument::VoiceConfig;

constexpr std::uint32_t kSampleRate = 44'100;
constexpr std::uint32_t kBlockSize = 64;

// Cuenta cruces por cero en una ventana y devuelve la frecuencia estimada.
// La salida del DAC es una onda cuadrada sin muestras a cero, asi que cada
// periodo produce exactamente dos cruces.
double measureFrequency(const std::vector<float>& samples, double fromSeconds, double toSeconds) {
    const auto begin = static_cast<std::size_t>(fromSeconds * kSampleRate);
    const auto end = std::min<std::size_t>(samples.size(),
                                           static_cast<std::size_t>(toSeconds * kSampleRate));
    int crossings = 0;
    for (std::size_t i = begin + 1; i < end; ++i) {
        const bool before = samples[i - 1] >= 0.0f;
        const bool after = samples[i] >= 0.0f;
        if (before != after) {
            ++crossings;
        }
    }
    const double seconds = static_cast<double>(end - begin) / kSampleRate;
    return seconds > 0.0 ? static_cast<double>(crossings) / (2.0 * seconds) : 0.0;
}

double rms(const std::vector<float>& samples, double fromSeconds) {
    const auto begin = static_cast<std::size_t>(fromSeconds * kSampleRate);
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = begin; i < samples.size(); ++i) {
        sum += static_cast<double>(samples[i]) * samples[i];
        ++count;
    }
    return count == 0 ? 0.0 : std::sqrt(sum / static_cast<double>(count));
}

struct VoiceRig {
    MachineHost host{{kSampleRate, kBlockSize}};
    MonoVoice voice;
    int postedNote = -2;
    std::vector<float> samples;

    bool prepare(std::string& error) {
        if (!host.loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), error)) {
            return false;
        }
        voice.prepare(kSampleRate);
        float warmup[kBlockSize] = {};
        float* outputs[1] = {warmup}; // instala la maquina
        host.renderBlock(kBlockSize, outputs, 1);
        return true;
    }

    void postClockIfNeeded() {
        if (voice.currentNote() < 0 || voice.currentNote() == postedNote) {
            return;
        }
        postedNote = voice.currentNote();
        host.postCommand(bdmcs48::machines::GenericMcs48Machine::kParameterEngineClock + 0x1000u,
                         voice.clockCommandValue());
    }

    void render(double seconds) {
        const std::size_t total = static_cast<std::size_t>(seconds * kSampleRate);
        std::size_t done = 0;
        std::vector<float> block(kBlockSize, 0.0f);
        while (done < total) {
            const auto frames = static_cast<std::uint32_t>(
                std::min<std::size_t>(kBlockSize, total - done));
            postClockIfNeeded();
            float* outputs[1] = {block.data()};
            host.renderBlock(frames, outputs, 1);
            for (std::uint32_t i = 0; i < frames; ++i) {
                block[i] *= voice.nextGain();
            }
            samples.insert(samples.end(), block.begin(), block.begin() + frames);
            done += frames;
        }
    }
};

} // namespace

TEST(Instrument, LaFrecuenciaNativaCoincideConLaConstante) {
    // Si cambia el programa de voz hay que recalibrar la constante: el host
    // afina desde ella y una desviacion aqui desafina todas las notas.
    bdmcs48::host::MachineHost host{{kSampleRate, kBlockSize}};
    std::string error;
    CHECK(host.loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), error));

    std::vector<float> samples;
    std::vector<float> block(kBlockSize, 0.0f);
    for (int i = 0; i < 1'400; ++i) { // ~2 s
        float* outputs[1] = {block.data()};
        host.renderBlock(kBlockSize, outputs, 1);
        samples.insert(samples.end(), block.begin(), block.end());
    }

    CHECK_NEAR(measureFrequency(samples, 0.5, 1.5),
               bdmcs48::firmware::demo::kMonoVoiceBaseFrequencyHz, 0.5);
}

TEST(Instrument, LaNotaLa4SuenaA440) {
    VoiceRig rig;
    std::string error;
    CHECK(rig.prepare(error));

    rig.voice.noteOn(69, 1.0f);
    rig.render(1.5);

    const double frequency = measureFrequency(rig.samples, 0.5, 1.5);
    CHECK_NEAR(frequency, 440.0, 2.0);
}

TEST(Instrument, UnaOctavaAbajoSuenaAMitadDeFrecuencia) {
    VoiceRig rig;
    std::string error;
    CHECK(rig.prepare(error));

    rig.voice.noteOn(57, 1.0f); // A3
    rig.render(1.5);

    CHECK_NEAR(measureFrequency(rig.samples, 0.5, 1.5), 220.0, 1.5);
}

TEST(Instrument, LaNotaLa2SuenaA110) {
    // Nota grave: el reloj baja a ~148 kHz y la CPU ejecuta menos ciclos por
    // muestra. Antes de comparar contra el limite absoluto de ciclos, el
    // sobrante de cada instruccion aceleraba la CPU y esta nota salia ~11 %
    // alta.
    VoiceRig rig;
    std::string error;
    CHECK(rig.prepare(error));

    rig.voice.noteOn(45, 1.0f); // A2
    rig.render(2.5);

    CHECK_NEAR(measureFrequency(rig.samples, 1.0, 2.5), 110.0, 1.0);
}

TEST(Instrument, CambiarDeNotaCambiaLaAfinacion) {
    VoiceRig rig;
    std::string error;
    CHECK(rig.prepare(error));

    rig.voice.noteOn(69, 1.0f); // A4
    rig.render(0.75);
    rig.voice.noteOff(69);
    rig.voice.noteOn(81, 1.0f); // A5
    rig.render(0.75);

    CHECK_NEAR(measureFrequency(rig.samples, 0.25, 0.7), 440.0, 3.0);
    CHECK_NEAR(measureFrequency(rig.samples, 0.95, 1.45), 880.0, 5.0);
}

TEST(Instrument, AlSoltarLaNotaElAudioSeApaga) {
    VoiceRig rig;
    std::string error;
    CHECK(rig.prepare(error));

    rig.voice.noteOn(69, 1.0f);
    rig.render(0.5);
    CHECK(rms(rig.samples, 0.25) > 0.1);

    rig.voice.noteOff(69);
    rig.render(0.5); // 80 ms de liberacion + margen

    CHECK_NEAR(rms(rig.samples, 0.6), 0.0, 1e-6);
}
