#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>

#include "firmware/demo/MonoVoiceFirmware.h"

namespace bdmcs48::plugin {
namespace {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Gain",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), -6.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1}, "Release",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 80.0f));
    return {parameters.begin(), parameters.end()};
}

} // namespace

InstrumentAudioProcessor::InstrumentAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    voice_.prepare(44'100.0, [] {
        bdmcs48::instrument::VoiceConfig config;
        config.baseFrequencyHz = bdmcs48::firmware::demo::kMonoVoiceBaseFrequencyHz;
        return config;
    }());
}

InstrumentAudioProcessor::~InstrumentAudioProcessor() {
    cancelPendingUpdate();
}

void InstrumentAudioProcessor::installHost(double sampleRate, int maximumBlockSize) {
    auto host = std::make_unique<bdmcs48::host::MachineHost>(
        bdmcs48::host::MachineHost::Config{static_cast<std::uint32_t>(sampleRate),
                                          static_cast<std::uint32_t>(maximumBlockSize)});

    std::string error;
    bool loaded = false;
    if (!pendingPackagePath_.empty()) {
        loaded = host->loadPackage(pendingPackagePath_, error);
        if (loaded) {
            loadedPackagePath_ = pendingPackagePath_;
        }
    }
    if (!loaded) {
        loaded = host->loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), error);
        if (loaded) {
            loadedPackagePath_.clear();
        }
    }
    pendingPackagePath_.clear();

    if (loaded) {
        const auto& manifest = host->manifest();
        firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
    } else {
        firmwareLabel_ = "sin firmware: " + juce::String(error);
    }

    const juce::SpinLock::ScopedLockType lock(hostLock_);
    host_ = std::move(host);
}

void InstrumentAudioProcessor::loadDefaultFirmware() {
    if (!host_) {
        return;
    }
    std::string error;
    if (host_->loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), error)) {
        loadedPackagePath_.clear();
        const auto& manifest = host_->manifest();
        firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
    }
}

void InstrumentAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    installHost(sampleRate, samplesPerBlock);

    bdmcs48::instrument::VoiceConfig config;
    config.baseFrequencyHz = bdmcs48::firmware::demo::kMonoVoiceBaseFrequencyHz;
    config.releaseMs = parameters_.getRawParameterValue("release")->load();
    voice_.prepare(sampleRate, config);

    postedNote_ = -2;
    dcX1_ = 0.0f;
    dcY1_ = 0.0f;
    updateDisplay();
}

void InstrumentAudioProcessor::releaseResources() {
    // El host se mantiene: el editor puede seguir leyendo su estado.
}

bool InstrumentAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void InstrumentAudioProcessor::renderSegment(juce::AudioBuffer<float>& buffer, int startSample,
                                             int numSamples) {
    auto* host = host_.get();
    if (host == nullptr || numSamples <= 0) {
        return;
    }

    // La afinacion viaja por la cola de comandos y se aplica al principio del
    // segmento: el cambio de nota cae en el mismo bloque en que llega el MIDI,
    // no en el siguiente.
    if (voice_.currentNote() >= 0 && voice_.currentNote() != postedNote_) {
        postedNote_ = voice_.currentNote();
        host->postCommand(kEngineClockCommand, voice_.clockCommandValue());
    }

    float* output = buffer.getWritePointer(0, startSample);
    float* outputs[1] = {output};
    host->renderBlock(static_cast<std::uint32_t>(numSamples), outputs, 1);

    const float gain = juce::Decibels::decibelsToGain(
        parameters_.getRawParameterValue("gain")->load());
    const float velocity = voice_.velocity();
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        const float envelope = voice_.nextGain();
        const float input = output[i];
        // El DAC del firmware no esta centrado en 0; un paso alto de un polo
        // quita la continua sin tocar la onda cuadrada.
        const float filtered = input - dcX1_ + 0.995f * dcY1_;
        dcX1_ = input;
        dcY1_ = filtered;
        const float value = filtered * envelope * velocity * gain;
        output[i] = value;
        peak = std::max(peak, std::abs(value));
    }

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel) {
        buffer.copyFrom(channel, startSample, output, numSamples);
    }

    displayLevel_.store(peak, std::memory_order_relaxed);
}

void InstrumentAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (host_.get() == nullptr) {
        return;
    }

    voice_.setReleaseMs(parameters_.getRawParameterValue("release")->load());

    const int total = buffer.getNumSamples();
    int position = 0;

    for (const auto metadata : midi) {
        const int at = juce::jlimit(0, total, metadata.samplePosition);
        if (at > position) {
            renderSegment(buffer, position, at - position);
            position = at;
        }
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            voice_.noteOn(message.getNoteNumber(), message.getFloatVelocity());
        } else if (message.isNoteOff()) {
            voice_.noteOff(message.getNoteNumber());
        } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
            voice_.allNotesOff();
        }
    }

    if (position < total) {
        renderSegment(buffer, position, total - position);
    }

    updateDisplay();
}

void InstrumentAudioProcessor::updateDisplay() {
    const int note = voice_.currentNote();
    displayNote_.store(note, std::memory_order_relaxed);
    displayFrequency_.store(note >= 0 ? static_cast<float>(frequencyForNote(note)) : 0.0f,
                            std::memory_order_relaxed);
    sounding_.store(voice_.sounding() ? 1 : 0, std::memory_order_relaxed);
}

bool InstrumentAudioProcessor::loadPackage(const juce::File& directory, juce::String& error) {
    if (!directory.isDirectory()) {
        error = "paquete no valido: " + directory.getFullPathName();
        return false;
    }

    // El puntero solo cambia en prepareToPlay, con el audio detenido; no hace
    // falta bloquearlo para leerlo desde el hilo de mensajes.
    auto* host = host_.get();
    if (host == nullptr) {
        // Aun no hay frecuencia de muestreo: se guarda para prepareToPlay.
        pendingPackagePath_ = directory.getFullPathName().toStdString();
        return true;
    }

    std::string message;
    if (!host->loadPackage(directory.getFullPathName().toStdString(), message)) {
        error = juce::String(message);
        return false;
    }

    loadedPackagePath_ = directory.getFullPathName().toStdString();
    pendingPackagePath_.clear();
    const auto& manifest = host->manifest();
    firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
    return true;
}

void InstrumentAudioProcessor::resetMachine() {
    if (host_ == nullptr) {
        return;
    }
    std::string error;
    if (!loadedPackagePath_.empty()) {
        host_->loadPackage(loadedPackagePath_, error);
    } else {
        host_->loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), error);
    }
    voice_.allNotesOff();
}

juce::String InstrumentAudioProcessor::firmwareLabel() const {
    return firmwareLabel_;
}

juce::String InstrumentAudioProcessor::statusLine() const {
    if (host_ == nullptr) {
        return "motor no inicializado";
    }
    const auto status = host_->status();
    if (status.faulted != 0) {
        return "CPU detenida: " + juce::String(status.faultMessage);
    }
    const auto context = host_->context();
    juce::String line;
    line << juce::String(context.firmwareId) << "  ·  " << context.hotSwaps << " cambios  ·  "
         << context.droppedCommands << " comandos perdidos";
    return line;
}

double InstrumentAudioProcessor::frequencyForNote(int midiNote) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

void InstrumentAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = parameters_.copyState();
    state.setProperty("firmwarePath", juce::String(loadedPackagePath_), nullptr);
    if (auto xml = state.createXml()) {
        copyXmlToBinary(*xml, destination);
    }
}

void InstrumentAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) {
        return;
    }
    parameters_.replaceState(juce::ValueTree::fromXml(*xml));

    const auto path = xml->getStringAttribute("firmwarePath");
    if (path.isNotEmpty()) {
        pendingStatePath_ = path.toStdString();
        triggerAsyncUpdate();
    }
}

void InstrumentAudioProcessor::handleAsyncUpdate() {
    if (pendingStatePath_.empty()) {
        return;
    }
    juce::String error;
    if (!loadPackage(juce::File(pendingStatePath_), error)) {
        loadDefaultFirmware();
    }
    pendingStatePath_.clear();
}

} // namespace bdmcs48::plugin

// Punto de entrada que usan los envoltorios de JUCE (VST3, Standalone).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new bdmcs48::plugin::InstrumentAudioProcessor();
}
