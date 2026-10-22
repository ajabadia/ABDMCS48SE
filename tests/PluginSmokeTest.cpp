// PluginSmokeTest.cpp - Prueba headless del AudioProcessor (sin editor).
//
// Construye el processor como lo haria un host, le manda una nota MIDI y
// comprueba dos cosas sobre el audio renderizado:
//   * la afinacion (440 Hz para A4) atravesando todo el camino: MIDI ->
//     voz -> cola de comandos -> reloj de la CPU -> firmware -> DAC,
//   * que al soltar la nota la salida llega al silencio.
//
// No abre ventanas ni dispositivos de audio: solo llama a prepareToPlay y
// processBlock, que es exactamente lo que hace un DAW.

#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "plugin/PluginProcessor.h"

namespace {

constexpr double kSampleRate = 44'100.0;
constexpr int kBlockSize = 64;

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::printf("[FALLO] %s\n", what);
        ++failures;
    } else {
        std::printf("[ok]    %s\n", what);
    }
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    bdmcs48::plugin::InstrumentAudioProcessor processor;
    check(!processor.hasEditor(), "el esqueleto no tiene editor");

    processor.setPlayConfigDetails(0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
    processor.processBlock(buffer, midi);

    // 1,5 s con la nota mantenida; se mide la afinacion en la segunda mitad.
    std::vector<float> rendered;
    for (int block = 0; block < 1'033; ++block) {
        if (block > 0) {
            midi.clear();
            processor.processBlock(buffer, midi);
        }
        const float* channel = buffer.getReadPointer(0);
        rendered.insert(rendered.end(), channel, channel + kBlockSize);
    }

    const std::size_t begin = static_cast<std::size_t>(0.5 * kSampleRate);
    const std::size_t end = static_cast<std::size_t>(1.4 * kSampleRate);
    int crossings = 0;
    for (std::size_t i = begin + 1; i < end; ++i) {
        if ((rendered[i - 1] >= 0.0f) != (rendered[i] >= 0.0f)) {
            ++crossings;
        }
    }
    const double seconds = static_cast<double>(end - begin) / kSampleRate;
    const double frequency = static_cast<double>(crossings) / (2.0 * seconds);
    std::printf("       frecuencia medida: %.2f Hz\n", frequency);
    check(std::abs(frequency - 440.0) < 3.0, "A4 suena a 440 Hz");

    // La nota suena tambien en el canal derecho (mono replicado).
    double rmsRight = 0.0;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const double value = buffer.getReadPointer(1)[i];
        rmsRight += value * value;
    }
    check(rmsRight > 1e-6, "el canal derecho recibe la señal");

    // Note off: 80 ms de liberacion + margen.
    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
    processor.processBlock(buffer, midi);

    std::vector<float> tail;
    for (int block = 0; block < 400; ++block) {
        midi.clear();
        processor.processBlock(buffer, midi);
        const float* channel = buffer.getReadPointer(0);
        tail.insert(tail.end(), channel, channel + kBlockSize);
    }

    const std::size_t tailBegin = static_cast<std::size_t>(0.3 * kSampleRate);
    double rms = 0.0;
    for (std::size_t i = tailBegin; i < tail.size(); ++i) {
        rms += static_cast<double>(tail[i]) * tail[i];
    }
    rms = std::sqrt(rms / static_cast<double>(tail.size() - tailBegin));
    std::printf("       RMS tras soltar: %.6f\n", rms);
    check(rms < 1e-4, "al soltar la nota la salida se apaga");

    check(processor.parameters().getParameter("gain") != nullptr &&
              processor.parameters().getParameter("release") != nullptr,
          "parametros gain y release presentes");

    std::printf(failures == 0 ? "\n[OK] plugin smoke\n" : "\n[FALLO] plugin smoke\n");
    return failures == 0 ? 0 : 1;
}
