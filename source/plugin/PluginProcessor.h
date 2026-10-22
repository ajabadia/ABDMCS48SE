// PluginProcessor.h - Instrumento monofonico JUCE sobre el core MCS-48.
//
// Esqueleto tecnico SIN interfaz: el editor se anadira despues. El processor
// ya expone el estado que necesitara cualquier UI (nota, frecuencia, nivel,
// etiqueta de firmware y linea de estado) y la carga de paquetes.
//
// Reparto de hilos:
//   * processBlock (hilo de audio): renderiza la maquina, aplica la envolvente
//     y encola el cambio de afinacion. Nada de memoria, ficheros ni UI.
//   * prepareToPlay: construye el MachineHost (fuera del audio thread) y
//     carga el firmware.
//   * Hilo de mensajes: carga de paquetes y lecturas de estado.
//
// El core no conoce JUCE: este fichero es el unico que traduce entre los dos.

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <juce_audio_processors/juce_audio_processors.h>

#include "host/MachineHost.h"
#include "instrument/MonoVoice.h"

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

    // Sin editor por ahora: el instrumento funciona como VST3/Standalone
    // headless. La UI se enganchara aqui cuando lleguen las instrucciones.
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

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

    // --- Estado para una futura UI ---------------------------------------
    juce::AudioProcessorValueTreeState& parameters() { return parameters_; }

    // Hilo de mensajes: instala un paquete de disco. Si falla, la maquina
    // actual sigue sonando y se devuelve el error.
    bool loadPackage(const juce::File& directory, juce::String& error);
    void resetMachine();

    juce::String firmwareLabel() const;
    juce::String statusLine() const;
    int currentNote() const { return displayNote_.load(std::memory_order_relaxed); }
    float currentFrequency() const { return displayFrequency_.load(std::memory_order_relaxed); }
    bool sounding() const { return sounding_.load(std::memory_order_relaxed) != 0; }
    float level() const { return displayLevel_.load(std::memory_order_relaxed); }

    static double frequencyForNote(int midiNote);

    // Identificador del parametro de reloj dentro de la cola de comandos.
    static constexpr std::uint32_t kEngineClockCommand = 0x1000u + 1u;

private:
    void createParameters();
    void installHost(double sampleRate, int maximumBlockSize);
    void loadDefaultFirmware();
    void renderSegment(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void updateDisplay();

    juce::AudioProcessorValueTreeState parameters_;
    std::unique_ptr<bdmcs48::host::MachineHost> host_;
    bdmcs48::instrument::MonoVoice voice_;

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

    int postedNote_ = -2;
    float dcX1_ = 0.0f;
    float dcY1_ = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentAudioProcessor)
};

} // namespace bdmcs48::plugin
