// PluginProcessor.h - Instrumento monofonico JUCE sobre el core MCS-48.
//
// Reparto de hilos:
//   * processBlock (hilo de audio): renderiza la maquina, aplica la envolvente,
//     encola el cambio de afinacion y ejecuta la matriz de comandos. Nada de
//     memoria, ficheros ni UI.
//   * prepareToPlay: construye el MachineHost (fuera del audio thread) y
//     carga el firmware.
//   * Hilo de mensajes: carga de paquetes, edicion de la matriz, presets y
//     lecturas de estado. Los cambios de matriz llegan al audio como una
//     copia completa publicada con CommandMatrix::publishRows().
//
// El core no conoce JUCE: este fichero es el unico que traduce entre los dos.
// La matriz de comandos tambien vive fuera del core: envia comandos ya
// resueltos por el mismo camino que el resto del host.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "host/MachineHost.h"
#include "instrument/CommandMatrix.h"
#include "instrument/MonoVoice.h"

// El target del plugin define esto a 1; los binarios headless (plugin_smoke)
// lo dejan a 0 porque no enlazan el editor ni el backend WebView.
#ifndef BDMCS48SE_HAS_EDITOR
#define BDMCS48SE_HAS_EDITOR 0
#endif

namespace bdmcs48::plugin {

class InstrumentAudioProcessor final : public juce::AudioProcessor,
                                       private juce::AsyncUpdater {
public:
    InstrumentAudioProcessor();
    ~InstrumentAudioProcessor() override;

    // --- AudioProcessor ---------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return BDMCS48SE_HAS_EDITOR != 0; }

    const juce::String getName() const override { return "BDMCS48SE Instrument"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Carga diferida del paquete pedido por un preset. Se ejecuta en el hilo
    // de mensajes, nunca en el de audio ni durante la restauracion de estado.
    void handleAsyncUpdate() override;

    // --- Estado ----------------------------------------------------------
    juce::AudioProcessorValueTreeState& parameters() { return parameters_; }

    // Hilo de mensajes: instala un paquete de disco. Si falla, la maquina
    // actual sigue sonando y se devuelve el error.
    bool loadPackage(const juce::File& directory, juce::String& error);
    void resetMachine();

    // Teclado virtual del editor: las notas entran por el mismo colector de
    // MIDI que usa el host, no por un camino paralelo.
    void noteOnFromUi(int note, float velocity);
    void noteOffFromUi(int note);
    void allNotesOffFromUi();

    // --- Matriz de comandos (hilo de mensajes) ---------------------------
    using MatrixRows = std::array<bdmcs48::instrument::MatrixRow,
                                  bdmcs48::instrument::CommandMatrix::kRows>;

    MatrixRows matrixRows() const { return matrixRows_; }
    // Reemplaza una fila; la copia completa se publica al hilo de audio y se
    // guarda en el estado del plugin.
    bool setMatrixRow(int index, const bdmcs48::instrument::MatrixRow& row);
    void setMatrixSeed(std::uint32_t seed);
    std::uint32_t matrixSeed() const { return seedValue_.load(std::memory_order_relaxed); }
    std::uint64_t matrixDroppedCommands() const {
        return matrixDropped_.load(std::memory_order_relaxed);
    }

    // Ultimo comando que la matriz entrego a la maquina. Sirve al panel de
    // estado y a las pruebas de integracion.
    int lastCommandId() const { return lastCommandId_.load(std::memory_order_relaxed); }
    int lastCommandValue() const { return lastCommandValue_.load(std::memory_order_relaxed); }

    // --- Presets del Standalone ------------------------------------------
    juce::StringArray presetNames() const;
    bool savePreset(const juce::String& name, juce::String& error);
    bool loadPreset(const juce::String& name, juce::String& error);
    static juce::File presetDirectory();

    // Estado completo en JSON para la WebView. La autoridad es C++.
    juce::String uiStateJson();

    // --- Diagnostico ------------------------------------------------------
    juce::String firmwareLabel() const;
    juce::String firmwareHash() const;
    juce::String statusLine() const;
    int currentNote() const { return displayNote_.load(std::memory_order_relaxed); }
    float currentFrequency() const { return displayFrequency_.load(std::memory_order_relaxed); }
    bool sounding() const { return sounding_.load(std::memory_order_relaxed) != 0; }
    float level() const { return displayLevel_.load(std::memory_order_relaxed); }
    std::uint64_t underruns() const { return underruns_.load(std::memory_order_relaxed); }

    static double frequencyForNote(int midiNote);

    // Identificador del parametro de reloj dentro de la cola de comandos.
    static constexpr std::uint32_t kEngineClockCommand = 0x1000u + 1u;

private:
    void createParameters();
    void installHost(double sampleRate, int maximumBlockSize);
    void loadDefaultFirmware();
    void renderSegment(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void updateDisplay();
    void publishMatrixRows();

    juce::ValueTree matrixState() const;
    void applyMatrixState(const juce::ValueTree& state);

    juce::ValueTree createUiStateTree();

    juce::AudioProcessorValueTreeState parameters_;
    std::unique_ptr<bdmcs48::host::MachineHost> host_;
    bdmcs48::instrument::MonoVoice voice_;
    bdmcs48::instrument::CommandMatrix matrix_;
    MatrixRows matrixRows_;

    // Colector de las notas del teclado virtual. Es thread-safe y entrega
    // las notas al principio del bloque siguiente.
    juce::MidiMessageCollector midiCollector_;

    // El host solo se sustituye en prepareToPlay, cuando el audio esta
    // detenido; el hilo de audio lo dereferencia sin bloqueo.
    mutable juce::SpinLock hostLock_;

    std::string pendingPackagePath_;
    std::string pendingStatePath_;
    std::string loadedPackagePath_;
    juce::String firmwareLabel_ = "MCS-48 Mono Voice";

    std::atomic<int> displayNote_{-1};
    std::atomic<float> displayFrequency_{0.0f};
    std::atomic<float> displayLevel_{0.0f};
    std::atomic<int> sounding_{0};
    std::atomic<int> lastCommandId_{-1};
    std::atomic<int> lastCommandValue_{0};
    std::atomic<std::uint64_t> matrixDropped_{0};
    std::atomic<std::uint64_t> underruns_{0};
    // La semilla se cambia desde el hilo de mensajes; el hilo de audio la
    // adopta al principio de un bloque (0 = sin cambio pendiente).
    std::atomic<std::uint32_t> pendingSeed_{0};
    std::atomic<std::uint32_t> seedValue_{0x9e3779b9u};

    int postedNote_ = -2;
    float dcX1_ = 0.0f;
    float dcY1_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentAudioProcessor)
};

// Crea el editor sin que el processor necesite conocer su implementacion.
juce::AudioProcessorEditor* createInstrumentEditor(InstrumentAudioProcessor& processor);

} // namespace bdmcs48::plugin
