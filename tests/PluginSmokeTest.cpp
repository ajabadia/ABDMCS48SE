// PluginSmokeTest.cpp - Prueba headless del AudioProcessor y del puente.
//
// Construye el processor como lo haria un host y comprueba:
//   * la afinacion (440 Hz para A4) atravesando todo el camino: MIDI ->
//     voz -> cola de comandos -> reloj de la CPU -> firmware -> DAC,
//   * que al soltar la nota la salida llega al silencio,
//   * que la matriz de comandos dispara, retrasa, muta y no dispara con
//     probabilidad cero,
//   * que el estado (incluida la matriz) sobrevive a un roundtrip.
//
// No abre ventanas ni dispositivos de audio: solo llama a prepareToPlay y
// processBlock, que es exactamente lo que hace un DAW.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include <string>

#include "plugin/PluginProcessor.h"
#include "plugin/UiProtocol.h"
#include "plugin/WebViewBridge.h"
#include "webui/WebUIResources.h"

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

using bdmcs48::plugin::InstrumentAudioProcessor;
using bdmcs48::plugin::WebViewBridge;

// Renderiza `blocks` bloques sin MIDI y devuelve la salida del ultimo bloque.
std::vector<float> render(InstrumentAudioProcessor& processor, int blocks,
                          juce::AudioBuffer<float>& buffer) {
    juce::MidiBuffer midi;
    std::vector<float> last;
    for (int i = 0; i < blocks; ++i) {
        processor.processBlock(buffer, midi);
        last.assign(buffer.getReadPointer(0), buffer.getReadPointer(0) + kBlockSize);
    }
    return last;
}

void testAudio() {
    InstrumentAudioProcessor processor;
    check(!processor.hasEditor(), "el binario headless no instala editor");

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
}

void testMatrixDisparaYTransmite() {
    InstrumentAudioProcessor processor;
    processor.setPlayConfigDetails(0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay(kSampleRate, kBlockSize);
    WebViewBridge bridge(processor);

    const auto error = bridge.applyRowJson(
        0, R"({"enabled":true,"note":69,"target":"latch_write","value":200,
              "probability":1.0,"delayMs":0,"mutation":"none","mutationAmount":0})");
    check(error.isEmpty(), "la fila de la matriz se acepta");

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
    processor.processBlock(buffer, midi);

    check(processor.lastCommandId() == 0, "Note On dispara el comando de latch");
    const int expected = static_cast<int>(std::lround(200.0 / 255.0 * 32767.0));
    check(std::abs(processor.lastCommandValue() - expected) <= 2,
          "el valor viaja normalizado como espera el host");

    // El otro destino usa el rango de parametro del perfil (0x1000+).
    bridge.applyRowJson(1, R"({"enabled":true,"note":72,"target":"engine_clock",
                              "value":1.5,"probability":1.0,"delayMs":0,
                              "mutation":"none","mutationAmount":0})");
    juce::MidiBuffer clockMidi;
    clockMidi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.9f), 0);
    processor.processBlock(buffer, clockMidi);

    check(processor.lastCommandId() ==
              static_cast<int>(bdmcs48::instrument::CommandMatrix::kEngineClockCommandId),
          "engine_clock viaja como parametro del perfil");
    const double normalized = (1.5 - 0.0625) / (8.0 - 0.0625);
    check(std::abs(processor.lastCommandValue() -
                   static_cast<int>(std::lround(normalized * 32767.0))) <= 2,
          "la escala de reloj se normaliza antes de enviarla");
}

void testMatrixRetardoYProbabilidad() {
    InstrumentAudioProcessor processor;
    processor.setPlayConfigDetails(0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay(kSampleRate, kBlockSize);
    WebViewBridge bridge(processor);

    bridge.applyRowJson(0, R"({"enabled":true,"note":70,"target":"latch_write",
                              "value":10,"probability":1.0,"delayMs":100,
                              "mutation":"none","mutationAmount":0})");

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.9f), 0);
    processor.processBlock(buffer, midi);
    check(processor.lastCommandId() == -1, "un delay de 100 ms no aplica el comando de inmediato");

    // 50 ms: aun no debe haber comando.
    render(processor, static_cast<int>(0.050 * kSampleRate / kBlockSize), buffer);
    check(processor.lastCommandId() == -1, "a mitad del delay el comando sigue pendiente");

    // 100 ms mas: el comando ya se aplico.
    render(processor, static_cast<int>(0.060 * kSampleRate / kBlockSize), buffer);
    check(processor.lastCommandId() == 0, "al vencer el delay el comando se aplica");

    // Probabilidad cero: nunca dispara.
    InstrumentAudioProcessor quiet;
    quiet.setPlayConfigDetails(0, 2, kSampleRate, kBlockSize);
    quiet.prepareToPlay(kSampleRate, kBlockSize);
    WebViewBridge quietBridge(quiet);
    quietBridge.applyRowJson(0, R"({"enabled":true,"note":71,"target":"latch_write",
                                   "value":10,"probability":0.0,"delayMs":0,
                                   "mutation":"none","mutationAmount":0})");

    juce::AudioBuffer<float> quietBuffer(2, kBlockSize);
    juce::MidiBuffer quietMidi;
    quietMidi.addEvent(juce::MidiMessage::noteOn(1, 71, 0.9f), 0);
    quiet.processBlock(quietBuffer, quietMidi);
    check(quiet.lastCommandId() == -1, "probabilidad cero no genera comando");
}

void testMatrixMutacion() {
    InstrumentAudioProcessor processor;
    processor.setPlayConfigDetails(0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay(kSampleRate, kBlockSize);
    WebViewBridge bridge(processor);

    bridge.applyRowJson(0, R"({"enabled":true,"note":60,"target":"latch_write",
                             "value":18,"probability":1.0,"delayMs":0,
                             "mutation":"increment","mutationAmount":1})");

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    const auto playOnce = [&](int note) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.9f), 0);
        processor.processBlock(buffer, midi);
    };

    playOnce(60);
    const int first = processor.lastCommandValue();
    playOnce(60);
    const int second = processor.lastCommandValue();

    check(std::abs(first - static_cast<int>(std::lround(18.0 / 255.0 * 32767.0))) <= 2,
          "la primera ejecucion aplica el valor base");
    check(std::abs(second - static_cast<int>(std::lround(19.0 / 255.0 * 32767.0))) <= 2,
          "la mutacion incrementa la siguiente ejecucion");
}

void testRecursosEmbebidos() {
    // El plugin no depende de un directorio WebUI/ junto al binario: la
    // pagina viaja en BinaryData.
    bdmcs48::webui::Resource resource;
    check(bdmcs48::webui::resourceFor("/", resource), "la raiz sirve index.html");
    check(std::string(resource.mimeType).find("text/html") == 0,
          "index.html se sirve como HTML");
    check(std::string(resource.data, resource.size).find("Matriz de comandos") !=
              std::string::npos,
          "index.html contiene la matriz");

    check(bdmcs48::webui::resourceFor("/main.js", resource), "el JavaScript esta embebido");
    check(std::string(resource.data, resource.size).find("__JUCE__") != std::string::npos,
          "main.js usa el puente de JUCE");
    check(std::string(resource.data, resource.size).find("PROTOCOL_VERSION") !=
              std::string::npos,
          "main.js comprueba la version del contrato");

    check(bdmcs48::webui::resourceFor("/main.css", resource), "el CSS esta embebido");
    check(std::string(resource.mimeType).find("text/css") == 0, "main.css se sirve como CSS");
}

void testContratoDelPuente() {
    InstrumentAudioProcessor processor;
    WebViewBridge bridge(processor);

    check(!bridge.applyRowJson(99, "{}").isEmpty(), "una fila fuera de rango se rechaza");
    check(!bridge.applyRowJson(0, "no-es-json").isEmpty(), "un JSON invalido se rechaza");
    check(!bridge.applyRowJson(0, R"({"target":"otro"})").isEmpty(),
          "un destino desconocido se rechaza");
    check(!bridge.setParameter("no-existe", 0.5f).isEmpty(),
          "un parametro desconocido se rechaza");
    check(bridge.setParameter("gain", 0.5f).isEmpty(), "un parametro valido se aplica");
    check(!bridge.noteOn(200, 1.0f).isEmpty(), "una nota fuera de rango se rechaza");

    const auto presets = juce::JSON::parse(bridge.presetsJson());
    auto* presetsObject = presets.getDynamicObject();
    check(presetsObject != nullptr &&
              static_cast<int>(presetsObject->getProperty("protocol")) ==
                  bdmcs48::plugin::kUiProtocolVersion,
          "la lista de presets tambien declara la version");
    check(presetsObject != nullptr && presetsObject->getProperty("presets").isArray(),
          "la lista de presets viene dentro del documento versionado");
}

void testEstadoYMatriz() {
    InstrumentAudioProcessor processor;
    WebViewBridge bridge(processor);
    bridge.applyRowJson(2, R"({"enabled":true,"note":65,"target":"engine_clock",
                             "value":2.0,"probability":0.25,"delayMs":25,
                             "mutation":"wrap","mutationAmount":0.5})");

    const auto json = juce::JSON::parse(bridge.stateJson());
    auto* object = json.getDynamicObject();
    check(object != nullptr, "el estado es JSON valido");
    if (object != nullptr) {
        check(static_cast<int>(object->getProperty("protocol")) ==
                  bdmcs48::plugin::kUiProtocolVersion,
              "el estado declara la version del contrato");
        const auto rows = object->getProperty("rows");
        check(rows.size() == static_cast<int>(bdmcs48::instrument::CommandMatrix::kRows),
              "el estado incluye las 8 filas");
        const auto firmware = object->getProperty("firmware");
        check(firmware.getDynamicObject() != nullptr, "el estado incluye el firmware");
    }

    juce::MemoryBlock state;
    processor.getStateInformation(state);

    InstrumentAudioProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto rows = restored.matrixRows();
    check(rows[2].enabled && rows[2].note == 65, "el roundtrip conserva nota y activacion");
    check(std::abs(rows[2].value - 2.0f) < 1e-4f, "el roundtrip conserva el valor");
    check(std::abs(rows[2].probability - 0.25f) < 1e-4f, "el roundtrip conserva la probabilidad");
    check(std::abs(rows[2].delayMs - 25.0f) < 1e-4f, "el roundtrip conserva el delay");
    check(rows[2].mutation == bdmcs48::instrument::MatrixMutation::Wrap,
          "el roundtrip conserva la mutacion");
    check(std::abs(rows[2].mutationAmount - 0.5f) < 1e-4f,
          "el roundtrip conserva el paso de mutacion");

    const auto gain = restored.parameters().getRawParameterValue("gain")->load();
    check(std::abs(gain - (-6.0f)) < 1e-3f, "el roundtrip conserva los parametros");
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    testAudio();
    testMatrixDisparaYTransmite();
    testMatrixRetardoYProbabilidad();
    testMatrixMutacion();
    testRecursosEmbebidos();
    testContratoDelPuente();
    testEstadoYMatriz();

    std::printf(failures == 0 ? "\n[OK] plugin smoke\n" : "\n[FALLO] plugin smoke\n");
    return failures == 0 ? 0 : 1;
}
