#include "TestFramework.h"

#include <cmath>
#include <cstdint>

#include "instrument/MonoVoice.h"
#include "machines/GenericMcs48Machine.h"

namespace {

using bdmcs48::instrument::MonoVoice;
using bdmcs48::instrument::VoiceConfig;

constexpr double kSampleRate = 48'000.0;

// Reconstruye la escala de reloj que aplicara la maquina a partir del entero
// que viaja por la cola de comandos (MachineHost divide por 32767).
double scaleFromCommand(std::int32_t command) {
    const double normalized = static_cast<double>(command) / 32767.0;
    return bdmcs48::machines::GenericMcs48Machine::kMinClockScale +
           normalized * (bdmcs48::machines::GenericMcs48Machine::kMaxClockScale -
                         bdmcs48::machines::GenericMcs48Machine::kMinClockScale);
}

} // namespace

TEST(MonoVoice, LosLimitesPorDefectoCoincidenConElPerfil) {
    const VoiceConfig config;
    CHECK_EQ(config.minClockScale,
             static_cast<double>(bdmcs48::machines::GenericMcs48Machine::kMinClockScale));
    CHECK_EQ(config.maxClockScale,
             static_cast<double>(bdmcs48::machines::GenericMcs48Machine::kMaxClockScale));
}

TEST(MonoVoice, La4ApuntaA440Hz) {
    MonoVoice voice;
    voice.prepare(kSampleRate);
    voice.noteOn(69, 1.0f);

    const double expected = 440.0 / VoiceConfig{}.baseFrequencyHz;
    CHECK_NEAR(voice.clockScaleForNote(69), expected, 1e-9);
    CHECK_NEAR(scaleFromCommand(voice.clockCommandValue()), expected, 2e-5);
}

TEST(MonoVoice, CadaOctavaDuplicaLaEscala) {
    MonoVoice voice;
    voice.prepare(kSampleRate);
    const double low = voice.clockScaleForNote(45); // A2
    const double high = voice.clockScaleForNote(57); // A3
    CHECK_NEAR(high / low, 2.0, 1e-9);
}

TEST(MonoVoice, LaUltimaNotaManda) {
    MonoVoice voice;
    voice.prepare(kSampleRate);

    voice.noteOn(60, 1.0f);
    voice.noteOn(64, 0.5f);
    CHECK_EQ(voice.currentNote(), 64);
    CHECK_NEAR(voice.velocity(), 0.5, 1e-6);

    voice.noteOff(64);
    // 60 sigue pulsada: la voz vuelve a ella sin liberar la envolvente.
    CHECK_EQ(voice.currentNote(), 60);
    CHECK(voice.sounding());

    voice.noteOff(60);
    CHECK_EQ(voice.currentNote(), -1);
    // La envolvente nunca avanzo (no se renderizo audio), asi que la
    // liberacion es instantanea y la voz queda en silencio.
    CHECK(!voice.sounding());
}

TEST(MonoVoice, LaLiberacionLlegaAlSilencio) {
    VoiceConfig config;
    config.attackMs = 0.0;
    config.releaseMs = 10.0;
    MonoVoice voice;
    voice.prepare(kSampleRate, config);

    voice.noteOn(69, 1.0f);
    CHECK_NEAR(voice.nextGain(), 1.0, 1e-6);

    voice.noteOff(69);
    CHECK_EQ(voice.currentNote(), 69);
    float gain = 1.0f;
    int samples = 0;
    while (voice.sounding() && samples < 48'000) {
        gain = voice.nextGain();
        ++samples;
    }
    CHECK_EQ(voice.currentNote(), -1);
    CHECK_NEAR(gain, 0.0, 1e-6);
    CHECK(samples <= static_cast<int>(kSampleRate * 0.02)); // 10 ms + 1 muestra
}

TEST(MonoVoice, RepetirElTiempoDeLiberacionNoAlargalaCola) {
    // El host escribe el parametro en cada bloque; si eso recompone la
    // pendiente con la ganancia actual, la cola se vuelve casi infinita.
    VoiceConfig config;
    config.attackMs = 0.0;
    config.releaseMs = 80.0;
    MonoVoice voice;
    voice.prepare(kSampleRate, config);

    voice.noteOn(69, 1.0f);
    voice.nextGain();
    voice.noteOff(69);

    int samples = 0;
    while (voice.sounding() && samples < 48'000) {
        voice.setReleaseMs(80.0);
        voice.nextGain();
        ++samples;
    }
    CHECK(samples <= static_cast<int>(kSampleRate * 0.09)); // 80 ms + 1 muestra
}

TEST(MonoVoice, UnNoteOffDesconocidoNoCortaLaNota) {
    MonoVoice voice;
    voice.prepare(kSampleRate);
    voice.noteOn(72, 1.0f);
    voice.noteOff(71);
    CHECK_EQ(voice.currentNote(), 72);
    CHECK(voice.sounding());
}

TEST(MonoVoice, RepetirLaNotaLaDejaComoUltima) {
    MonoVoice voice;
    voice.prepare(kSampleRate);
    voice.noteOn(60, 1.0f);
    voice.noteOn(62, 1.0f);
    voice.noteOn(60, 1.0f);
    CHECK_EQ(voice.currentNote(), 60);

    voice.noteOff(60);
    CHECK_EQ(voice.currentNote(), 62);
}

TEST(MonoVoice, GlidePortamentoLegato) {
    VoiceConfig config;
    config.attackMs = 0.0;
    config.releaseMs = 100.0;
    config.glideMs = 50.0; // 50 ms
    MonoVoice voice;
    voice.prepare(kSampleRate, config);

    // Staccato attack: no glide inicial
    voice.noteOn(60, 1.0f);
    CHECK(!voice.isGliding());
    CHECK_NEAR(voice.currentPitch(), 60.0, 1e-4);
    voice.nextGain(); // avanza a Stage::Hold

    // Legato: 60 sigue sostenida y se pulsa 72 -> dispara glide
    voice.noteOn(72, 1.0f);
    CHECK(voice.isGliding());
    CHECK_EQ(voice.currentNote(), 72);

    // Muestra intermedia (tras 100 muestras ~2ms): el pitch ha subido pero aún no llega a 72
    for (int i = 0; i < 100; ++i) {
        voice.nextGain();
    }
    CHECK(voice.currentPitch() > 60.0);
    CHECK(voice.currentPitch() < 72.0);

    // Tras tiempo suficiente (~100 ms > 50 ms), el glide concluye en 72
    for (int i = 0; i < 4800; ++i) {
        voice.nextGain();
    }
    CHECK(!voice.isGliding());
    CHECK_NEAR(voice.currentPitch(), 72.0, 1e-3);
}

TEST(MonoVoice, GlideStaccatoNoDisparaPortamento) {
    VoiceConfig config;
    config.attackMs = 0.0;
    config.releaseMs = 5.0;
    config.glideMs = 50.0;
    MonoVoice voice;
    voice.prepare(kSampleRate, config);

    voice.noteOn(60, 1.0f);
    voice.nextGain();
    voice.noteOff(60);
    // Dejar que termine la liberación (silencio absoluto)
    for (int i = 0; i < 500; ++i) {
        voice.nextGain();
    }
    CHECK(!voice.sounding());

    // Nueva nota staccato desde el silencio: entra directa sin glide
    voice.noteOn(72, 1.0f);
    CHECK(!voice.isGliding());
    CHECK_NEAR(voice.currentPitch(), 72.0, 1e-4);
}

TEST(MonoVoice, CurvasDeVelocidad) {
    MonoVoice voice;
    voice.prepare(kSampleRate);
    voice.setVelocitySens(1.0f); // 100% sensible a velocidad

    // Linear
    voice.setVelocityCurve(bdmcs48::instrument::VelocityCurve::Linear);
    voice.noteOn(60, 0.5f);
    CHECK_NEAR(voice.velocity(), 0.5f, 1e-4);

    // Exponential (v^2 -> 0.5^2 = 0.25)
    voice.setVelocityCurve(bdmcs48::instrument::VelocityCurve::Exponential);
    voice.noteOn(60, 0.5f);
    CHECK_NEAR(voice.velocity(), 0.25f, 1e-4);

    // Logarithmic (sqrt(v) -> sqrt(0.25) = 0.5)
    voice.setVelocityCurve(bdmcs48::instrument::VelocityCurve::Logarithmic);
    voice.noteOn(60, 0.25f);
    CHECK_NEAR(voice.velocity(), 0.5f, 1e-4);

    // Stepped (4 niveles: 0.25, 0.5, 0.75, 1.0)
    voice.setVelocityCurve(bdmcs48::instrument::VelocityCurve::Stepped);
    voice.noteOn(60, 0.1f);
    CHECK_NEAR(voice.velocity(), 0.25f, 1e-4);
    voice.noteOn(60, 0.35f);
    CHECK_NEAR(voice.velocity(), 0.5f, 1e-4);
    voice.noteOn(60, 0.65f);
    CHECK_NEAR(voice.velocity(), 0.75f, 1e-4);
    voice.noteOn(60, 0.95f);
    CHECK_NEAR(voice.velocity(), 1.0f, 1e-4);
}
