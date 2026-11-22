#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <system_error>

#include "firmware/Sha256.h"
#include "firmware/demo/DemoFirmware.h"
#include "firmware/demo/DiagnosticFirmware.h"
#include "firmware/demo/MonoVoiceFirmware.h"
#include "firmware/demo/EnhancedMonoVoiceFirmware.h"
#include "firmware/demo/WavetablePercussionFirmware.h"
#include "plugin/UiProtocol.h"

namespace bdmcs48::plugin {
namespace {

namespace fs = std::filesystem;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Gain",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), -6.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1}, "Release",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 80.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"attack", 1}, "Attack",
        juce::NormalisableRange<float>(1.0f, 100.0f, 0.1f), 4.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"diagnosticTone", 1}, "Diagnostic Tone", false));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"tuningA4", 1}, "Master Tuning A4",
        juce::NormalisableRange<float>(432.0f, 448.0f, 0.1f), 440.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dcBlocker", 1}, "DC Blocker Feedback",
        juce::NormalisableRange<float>(0.900f, 0.999f, 0.001f), 0.995f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"velocitySens", 1}, "Velocity Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"glide", 1}, "Glide",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f, 0.4f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"velocityCurve", 1}, "Velocity Curve",
        juce::StringArray{"Linear", "Exponential", "Logarithmic", "Stepped"}, 0));
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dacTolerance", 1}, "DAC Tolerance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"dacQuantize", 1}, "DAC Quantization",
        juce::StringArray{"1-bit", "2-bit", "3-bit", "4-bit", "5-bit", "6-bit", "7-bit", "8-bit"}, 7));
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"freeze", 1}, "Circuit Freeze",
        juce::StringArray{"Off", "Frozen"}, 0));
    return {parameters.begin(), parameters.end()};
}

const char* mutationName(bdmcs48::instrument::MatrixMutation mutation) {
    using bdmcs48::instrument::MatrixMutation;
    switch (mutation) {
    case MatrixMutation::None: return "none";
    case MatrixMutation::Increment: return "increment";
    case MatrixMutation::Decrement: return "decrement";
    case MatrixMutation::Random: return "random";
    case MatrixMutation::Wrap: return "wrap";
    }
    return "none";
}

bdmcs48::instrument::MatrixMutation mutationFromName(const juce::String& name) {
    using bdmcs48::instrument::MatrixMutation;
    if (name == "increment") return MatrixMutation::Increment;
    if (name == "decrement") return MatrixMutation::Decrement;
    if (name == "random") return MatrixMutation::Random;
    if (name == "wrap") return MatrixMutation::Wrap;
    return MatrixMutation::None;
}

std::string jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                out += buffer;
            } else {
                out.push_back(c);
            }
        }
    }
    return out;
}

bool writeBinaryFile(const fs::path& path, const std::vector<std::uint8_t>& data, juce::String& error) {
    std::error_code code;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), code);
        if (code) {
            error = "no se pudo crear el directorio '" + juce::String(path.parent_path().string()) + "'";
            return false;
        }
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "no se pudo abrir '" + juce::String(path.string()) + "' para escritura";
        return false;
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    stream.flush();
    if (!stream) {
        error = "fallo al escribir '" + juce::String(path.string()) + "'";
        return false;
    }
    return true;
}

bool writeTextFile(const fs::path& path, const std::string& text, juce::String& error) {
    std::error_code code;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), code);
        if (code) {
            error = "no se pudo crear el directorio '" + juce::String(path.parent_path().string()) + "'";
            return false;
        }
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "no se pudo abrir '" + juce::String(path.string()) + "' para escritura";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream) {
        error = "fallo al escribir '" + juce::String(path.string()) + "'";
        return false;
    }
    return true;
}

} // namespace

juce::PropertiesFile::Options InstrumentAudioProcessor::getPropertiesFileOptions() const {
    juce::PropertiesFile::Options options;
    options.applicationName = "ABDMCS48SE";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "ABDMCS48SE";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.doNotSave = false;
    options.millisecondsBeforeSaving = 500;
    return options;
}

std::unique_ptr<juce::PropertiesFile> InstrumentAudioProcessor::getPropertiesFile() const {
    return std::make_unique<juce::PropertiesFile>(getPropertiesFileOptions());
}

InstrumentAudioProcessor::InstrumentAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "PARAMETERS", createParameterLayout()),
      matrixRows_(bdmcs48::instrument::CommandMatrix::defaults()) {
    matrix_.prepare(44'100.0);
    matrix_.publishRows(matrixRows_);
    matrix_.consumePublishedRows();
    voice_.prepare(44'100.0, [] {
        bdmcs48::instrument::VoiceConfig config;
        config.baseFrequencyHz = bdmcs48::firmware::demo::kEnhancedMonoVoiceBaseFrequencyHz;
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
        loaded = host->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), error);
        if (loaded) {
            loadedPackagePath_.clear();
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
    if (host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), error)) {
        loadedPackagePath_.clear();
        const auto& manifest = host_->manifest();
        firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
        return;
    }
    if (host_->loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), error)) {
        loadedPackagePath_.clear();
        const auto& manifest = host_->manifest();
        firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
    }
}

void InstrumentAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    installHost(sampleRate, samplesPerBlock);

    matrix_.prepare(sampleRate);
    matrix_.clearSchedule();
    midiCollector_.reset(sampleRate);
    midiCollector_.ensureStorageAllocated(4 * 1024);
    underruns_.store(0, std::memory_order_relaxed);

    bdmcs48::instrument::VoiceConfig config;
    config.baseFrequencyHz = bdmcs48::firmware::demo::kEnhancedMonoVoiceBaseFrequencyHz;
    config.releaseMs = parameters_.getRawParameterValue("release")->load();
    if (auto* pAttack = parameters_.getRawParameterValue("attack")) {
        config.attackMs = pAttack->load();
    }
    if (auto* pTuning = parameters_.getRawParameterValue("tuningA4")) {
        config.referenceHz = pTuning->load();
    }
    if (auto* pGlide = parameters_.getRawParameterValue("glide")) {
        config.glideMs = pGlide->load();
    }
    if (auto* pCurve = parameters_.getRawParameterValue("velocityCurve")) {
        config.velocityCurve = static_cast<bdmcs48::instrument::VelocityCurve>(
            std::clamp(static_cast<int>(pCurve->load()), 0, 3));
    }
    voice_.prepare(sampleRate, config);
    if (auto* pVel = parameters_.getRawParameterValue("velocitySens")) {
        voice_.setVelocitySens(pVel->load());
    }

    postedNote_ = -2;
    postedClockValue_ = -1;
    postedDacTolerance_ = -1.0f;
    if (auto* pDac = parameters_.getRawParameterValue("dacTolerance")) {
        postedDacTolerance_ = pDac->load();
        if (host_) {
            host_->postCommand(kDacToleranceCommand, static_cast<std::int32_t>(postedDacTolerance_ * 32767.0f));
        }
    }
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

    const float gain = juce::Decibels::decibelsToGain(
        parameters_.getRawParameterValue("gain")->load());
    const float velocity = voice_.velocity();
    float peak = 0.0f;

    const float dcCoeff = parameters_.getRawParameterValue("dcBlocker") != nullptr
                              ? parameters_.getRawParameterValue("dcBlocker")->load()
                              : 0.995f;

    constexpr int kGlideSubBlock = 16;
    int samplesRendered = 0;

    while (samplesRendered < numSamples) {
        const int chunkSize = voice_.isGliding()
            ? std::min(kGlideSubBlock, numSamples - samplesRendered)
            : (numSamples - samplesRendered);

        if (voice_.currentNote() >= 0) {
            const auto clockCmd = voice_.clockCommandValue();
            if (voice_.currentNote() != postedNote_ || clockCmd != postedClockValue_) {
                postedNote_ = voice_.currentNote();
                postedClockValue_ = clockCmd;
                host->postCommand(kEngineClockCommand, clockCmd);
            }
        }

        float* output = buffer.getWritePointer(0, startSample + samplesRendered);
        float* outputs[1] = {output};
        host->renderBlock(static_cast<std::uint32_t>(chunkSize), outputs, 1);

        for (int i = 0; i < chunkSize; ++i) {
            const float envelope = voice_.nextGain();
            const float input = output[i];
            const float filtered = input - dcX1_ + dcCoeff * dcY1_;
            dcX1_ = input;
            dcY1_ = filtered;
            const float value = filtered * envelope * velocity * gain;
            output[i] = value;
            peak = std::max(peak, std::abs(value));
        }

        samplesRendered += chunkSize;
    }

    if (diagnosticToneEnabled_.load(std::memory_order_relaxed)) {
        constexpr float kFreq = 440.0f;
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        const float phaseDelta = static_cast<float>(2.0 * juce::MathConstants<double>::pi * kFreq / sr);
        for (int i = 0; i < numSamples; ++i) {
            const float tone = 0.2f * std::sin(diagnosticTonePhase_);
            diagnosticTonePhase_ += phaseDelta;
            if (diagnosticTonePhase_ >= static_cast<float>(2.0 * juce::MathConstants<double>::pi)) {
                diagnosticTonePhase_ -= static_cast<float>(2.0 * juce::MathConstants<double>::pi);
            }
            buffer.addSample(0, startSample + i, tone);
        }
    }

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel) {
        buffer.copyFrom(channel, startSample, buffer, 0, startSample, numSamples);
    }

    displayLevel_.store(peak, std::memory_order_relaxed);
}

void InstrumentAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int total = buffer.getNumSamples();
    if (host_.get() == nullptr) {
        // Sin maquina no hay audio: se cuenta como underrun para que el panel
        // de estado no lo confunda con silencio musical.
        underruns_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Las notas del teclado virtual entran por el mismo colector que usa un
    // host real y se mezclan con el MIDI del DAW antes de renderizar.
    {
        juce::MidiBuffer collected;
        midiCollector_.removeNextBlockOfMessages(collected, total);
        for (const auto metadata : collected) {
            midi.addEvent(metadata.getMessage(), metadata.samplePosition);
        }
    }

    matrix_.consumePublishedRows();
    if (const auto seed = pendingSeed_.exchange(0, std::memory_order_acq_rel); seed != 0) {
        matrix_.setSeed(seed);
    }
    voice_.setReleaseMs(parameters_.getRawParameterValue("release")->load());
    if (auto* pAttack = parameters_.getRawParameterValue("attack")) {
        voice_.setAttackMs(pAttack->load());
    }
    if (auto* pTuning = parameters_.getRawParameterValue("tuningA4")) {
        voice_.setReferenceHz(pTuning->load());
    }
    if (auto* pVel = parameters_.getRawParameterValue("velocitySens")) {
        voice_.setVelocitySens(pVel->load());
    }
    if (auto* pGlide = parameters_.getRawParameterValue("glide")) {
        voice_.setGlideMs(pGlide->load());
    }
    if (auto* pCurve = parameters_.getRawParameterValue("velocityCurve")) {
        voice_.setVelocityCurve(static_cast<bdmcs48::instrument::VelocityCurve>(
            std::clamp(static_cast<int>(pCurve->load()), 0, 3)));
    }
    if (auto* pDac = parameters_.getRawParameterValue("dacTolerance")) {
        const float dacVal = pDac->load();
        if (std::abs(dacVal - postedDacTolerance_) > 1e-4f) {
            postedDacTolerance_ = dacVal;
            host_->postCommand(kDacToleranceCommand, static_cast<std::int32_t>(dacVal * 32767.0f));
        }
    }
    if (auto* pQuant = parameters_.getRawParameterValue("dacQuantize")) {
        const float qVal = pQuant->load();
        if (std::abs(qVal - postedDacQuantize_) > 1e-4f) {
            postedDacQuantize_ = qVal;
            host_->postCommand(0x1000u + 3u, static_cast<std::int32_t>((qVal / 7.0f) * 32767.0f));
        }
    }
    if (auto* pFrz = parameters_.getRawParameterValue("freeze")) {
        const float frzVal = pFrz->load();
        if (std::abs(frzVal - postedFreeze_) > 1e-4f) {
            postedFreeze_ = frzVal;
            host_->postCommand(0x1000u + 4u, static_cast<std::int32_t>(frzVal >= 0.5f ? 32767 : 0));
        }
    }
    if (auto* pDiag = parameters_.getRawParameterValue("diagnosticTone")) {
        diagnosticToneEnabled_.store(pDiag->load() > 0.5f, std::memory_order_relaxed);
    }

    int position = 0;
    const std::uint64_t blockStart = matrix_.now();

    auto applyDueCommands = [this]() {
        bdmcs48::instrument::CommandMatrix::Command command;
        while (matrix_.popDue(command)) {
            host_->postCommand(command.id, command.value);
            lastCommandId_.store(static_cast<int>(command.id), std::memory_order_relaxed);
            lastCommandValue_.store(command.value, std::memory_order_relaxed);
        }
    };

    // Renderiza hasta `target`, partiendo el bloque en cada deadline de la
    // matriz. Asi un retardo de 5 ms cae en su muestra y no al principio del
    // bloque siguiente.
    auto renderTo = [&](int target) {
        while (position < target) {
            const std::uint64_t deadline = matrix_.nextDeadline();
            if (deadline != 0 && deadline <= matrix_.now()) {
                applyDueCommands();
                continue;
            }

            int boundary = target;
            if (deadline != 0) {
                const std::uint64_t offset = deadline - blockStart;
                if (offset < static_cast<std::uint64_t>(total)) {
                    boundary = std::min(target, static_cast<int>(offset));
                }
            }
            if (boundary <= position) {
                applyDueCommands();
                continue;
            }

            renderSegment(buffer, position, boundary - position);
            matrix_.advance(static_cast<std::uint64_t>(boundary - position));
            position = boundary;
            applyDueCommands();
        }
    };

    for (const auto metadata : midi) {
        const int at = juce::jlimit(0, total, metadata.samplePosition);
        renderTo(at);
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            voice_.noteOn(message.getNoteNumber(), message.getFloatVelocity());
            matrix_.noteOn(message.getNoteNumber());
        } else if (message.isNoteOff()) {
            voice_.noteOff(message.getNoteNumber());
        } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
            voice_.allNotesOff();
        } else if (message.isSysEx()) {
            handleSysEx(message.getSysExData(), message.getSysExDataSize());
        }
    }
    renderTo(total);

    matrixDropped_.store(matrix_.droppedCommands(), std::memory_order_relaxed);
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

// --- Teclado virtual ------------------------------------------------------

void InstrumentAudioProcessor::noteOnFromUi(int note, float velocity) {
    auto message = juce::MidiMessage::noteOn(1, juce::jlimit(0, 127, note),
                                             juce::jlimit(0.0f, 1.0f, velocity));
    message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector_.addMessageToQueue(message);
}

void InstrumentAudioProcessor::noteOffFromUi(int note) {
    auto message = juce::MidiMessage::noteOff(1, juce::jlimit(0, 127, note));
    message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector_.addMessageToQueue(message);
}

void InstrumentAudioProcessor::allNotesOffFromUi() {
    auto message = juce::MidiMessage::allNotesOff(1);
    message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
    midiCollector_.addMessageToQueue(message);
}

// --- Matriz de comandos ---------------------------------------------------

void InstrumentAudioProcessor::publishMatrixRows() {
    matrix_.publishRows(matrixRows_);
}

bool InstrumentAudioProcessor::setMatrixRow(int index, const bdmcs48::instrument::MatrixRow& row) {
    if (index < 0 || index >= static_cast<int>(matrixRows_.size())) {
        return false;
    }

    auto copy = row;
    copy.note = juce::jlimit(0, 127, copy.note);
    copy.value = bdmcs48::instrument::CommandMatrix::clampForTarget(copy.target, copy.value);
    copy.probability = juce::jlimit(0.0f, 1.0f, copy.probability);
    copy.delayMs = juce::jlimit(0.0f, 5'000.0f, copy.delayMs);
    const float span = bdmcs48::instrument::CommandMatrix::maxFor(copy.target) -
                       bdmcs48::instrument::CommandMatrix::minFor(copy.target);
    copy.mutationAmount = juce::jlimit(0.0f, span, copy.mutationAmount);

    const auto& existing = matrixRows_[static_cast<std::size_t>(index)];
    if (copy.value != existing.value) {
        // Un valor nuevo en la UI es el punto de partida de la mutacion; si
        // solo cambian otros campos, se conserva el valor ya mutado.
        copy.currentValue = copy.value;
    } else {
        copy.currentValue = existing.currentValue;
    }
    if (copy.target != existing.target) {
        copy.currentValue = copy.value;
    }

    matrixRows_[static_cast<std::size_t>(index)] = copy;
    publishMatrixRows();
    return true;
}

void InstrumentAudioProcessor::setMatrixSeed(std::uint32_t seed) {
    const auto value = seed == 0 ? 1u : seed;
    seedValue_.store(value, std::memory_order_relaxed);
    pendingSeed_.store(value, std::memory_order_release);
}

juce::ValueTree InstrumentAudioProcessor::matrixState() const {
    juce::ValueTree tree("CommandMatrix");
    tree.setProperty("seed", static_cast<int>(matrixSeed()), nullptr);
    for (const auto& row : matrixRows_) {
        juce::ValueTree node("Row");
        node.setProperty("enabled", row.enabled, nullptr);
        node.setProperty("note", row.note, nullptr);
        juce::String targetStr = "engine_clock";
        if (row.target == bdmcs48::instrument::MatrixTarget::LatchWrite) targetStr = "latch_write";
        else if (row.target == bdmcs48::instrument::MatrixTarget::DacQuantize) targetStr = "dac_quantize";
        else if (row.target == bdmcs48::instrument::MatrixTarget::Freeze) targetStr = "freeze";
        node.setProperty("target", targetStr, nullptr);
        node.setProperty("value", row.value, nullptr);
        node.setProperty("probability", row.probability, nullptr);
        node.setProperty("delayMs", row.delayMs, nullptr);
        node.setProperty("mutation", juce::String(mutationName(row.mutation)), nullptr);
        node.setProperty("mutationAmount", row.mutationAmount, nullptr);
        tree.appendChild(node, nullptr);
    }
    return tree;
}

void InstrumentAudioProcessor::applyMatrixState(const juce::ValueTree& state) {
    auto rows = bdmcs48::instrument::CommandMatrix::defaults();
    std::size_t index = 0;
    for (const auto node : state) {
        if (!node.hasType("Row") || index >= rows.size()) {
            continue;
        }
        auto& row = rows[index++];
        row.enabled = static_cast<bool>(node.getProperty("enabled", row.enabled));
        row.note = juce::jlimit(0, 127, static_cast<int>(node.getProperty("note", row.note)));
        const auto target = node.getProperty("target").toString();
        if (target == "latch_write") {
            row.target = bdmcs48::instrument::MatrixTarget::LatchWrite;
        } else if (target == "dac_quantize") {
            row.target = bdmcs48::instrument::MatrixTarget::DacQuantize;
        } else if (target == "freeze") {
            row.target = bdmcs48::instrument::MatrixTarget::Freeze;
        } else {
            row.target = bdmcs48::instrument::MatrixTarget::EngineClock;
        }
        row.value = bdmcs48::instrument::CommandMatrix::clampForTarget(
            row.target, static_cast<float>(node.getProperty("value", row.value)));
        row.currentValue = row.value;
        row.probability = juce::jlimit(0.0f, 1.0f,
                                       static_cast<float>(node.getProperty("probability", 1.0)));
        row.delayMs = juce::jlimit(0.0f, 5'000.0f,
                                   static_cast<float>(node.getProperty("delayMs", 0.0)));
        row.mutation = mutationFromName(node.getProperty("mutation").toString());
        row.mutationAmount = static_cast<float>(node.getProperty("mutationAmount", 0.0));
    }

    matrixRows_ = rows;
    publishMatrixRows();

    const auto seed = static_cast<std::uint32_t>(static_cast<int>(state.getProperty("seed", 0)));
    if (seed != 0) {
        setMatrixSeed(seed);
    }
}

// --- Presets --------------------------------------------------------------

juce::StringArray InstrumentAudioProcessor::factoryPresetNames() {
    return {
        "01 Default Mono Voice",
        "02 Mario Bros Arcade",
        "03 Vintage Lo-Fi 8048",
        "04 Fast Legato Glide",
        "05 Lo-Fi Percussion & Wavetable"
    };
}

bool InstrumentAudioProcessor::loadFactoryPreset(int index, juce::String& error) {
    if (index < 0 || index >= factoryPresetNames().size()) {
        error = "indice de preset de fabrica invalido";
        return false;
    }

    if (index == 0) { // Default Mono Voice
        std::string err;
        if (!host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), err)) {
            host_->loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), err);
        }
        loadedPackagePath_.clear();
        firmwareLabel_ = host_->manifest().name;

        if (auto* pGain = parameters_.getParameter("gain")) pGain->setValueNotifyingHost(pGain->convertTo0to1(-6.0f));
        if (auto* pRel = parameters_.getParameter("release")) pRel->setValueNotifyingHost(pRel->convertTo0to1(80.0f));
        if (auto* pAtt = parameters_.getParameter("attack")) pAtt->setValueNotifyingHost(pAtt->convertTo0to1(4.0f));
        if (auto* pGli = parameters_.getParameter("glide")) pGli->setValueNotifyingHost(pGli->convertTo0to1(0.0f));
        if (auto* pCur = parameters_.getParameter("velocityCurve")) pCur->setValueNotifyingHost(pCur->convertTo0to1(0.0f));
        if (auto* pTol = parameters_.getParameter("dacTolerance")) pTol->setValueNotifyingHost(pTol->convertTo0to1(0.0f));
        if (auto* pDc = parameters_.getParameter("dcBlocker")) pDc->setValueNotifyingHost(pDc->convertTo0to1(0.995f));
        if (auto* pTune = parameters_.getParameter("tuningA4")) pTune->setValueNotifyingHost(pTune->convertTo0to1(440.0f));

        matrixRows_ = bdmcs48::instrument::CommandMatrix::defaults();
        publishMatrixRows();
        return true;
    }

    if (index == 1) { // Mario Bros Arcade Sound Board
        const auto marioDirUser = workspaceDirectory().getChildFile("firmwares").getChildFile("user").getChildFile("mario-bros-sound-board");
        const auto marioDirApp = firmwareDirectory().getChildFile("mario-bros-sound-board");
        const auto marioDirWk = juce::File::getCurrentWorkingDirectory().getChildFile("workspace/firmwares/user/mario-bros-sound-board");

        bool loaded = false;
        if (marioDirUser.getChildFile("firmware.json").existsAsFile()) {
            loaded = loadPackage(marioDirUser, error);
        } else if (marioDirApp.getChildFile("firmware.json").existsAsFile()) {
            loaded = loadPackage(marioDirApp, error);
        } else if (marioDirWk.getChildFile("firmware.json").existsAsFile()) {
            loaded = loadPackage(marioDirWk, error);
        }

        if (!loaded) {
            std::string err;
            host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), err);
        }

        matrixRows_ = bdmcs48::instrument::CommandMatrix::defaults();
        const int notes[8] = {60, 62, 64, 65, 67, 69, 71, 72};
        const float latches[8] = {0.01f, 0.02f, 0.04f, 0.08f, 0.16f, 0.32f, 0.64f, 1.00f};
        for (std::size_t i = 0; i < matrixRows_.size() && i < 8; ++i) {
            matrixRows_[i].enabled = true;
            matrixRows_[i].note = notes[i];
            matrixRows_[i].target = bdmcs48::instrument::MatrixTarget::LatchWrite;
            matrixRows_[i].value = latches[i];
            matrixRows_[i].currentValue = latches[i];
            matrixRows_[i].probability = 1.0f;
            matrixRows_[i].delayMs = 0.0f;
            matrixRows_[i].mutation = bdmcs48::instrument::MatrixMutation::None;
            matrixRows_[i].mutationAmount = 0.0f;
        }
        publishMatrixRows();

        if (auto* pGain = parameters_.getParameter("gain")) pGain->setValueNotifyingHost(pGain->convertTo0to1(-3.0f));
        if (auto* pRel = parameters_.getParameter("release")) pRel->setValueNotifyingHost(pRel->convertTo0to1(250.0f));
        if (auto* pAtt = parameters_.getParameter("attack")) pAtt->setValueNotifyingHost(pAtt->convertTo0to1(1.0f));
        if (auto* pGli = parameters_.getParameter("glide")) pGli->setValueNotifyingHost(pGli->convertTo0to1(0.0f));
        if (auto* pTol = parameters_.getParameter("dacTolerance")) pTol->setValueNotifyingHost(pTol->convertTo0to1(0.35f));
        return true;
    }

    if (index == 2) { // Vintage Lo-Fi 8048
        std::string err;
        host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), err);
        if (auto* pGain = parameters_.getParameter("gain")) pGain->setValueNotifyingHost(pGain->convertTo0to1(-6.0f));
        if (auto* pRel = parameters_.getParameter("release")) pRel->setValueNotifyingHost(pRel->convertTo0to1(140.0f));
        if (auto* pAtt = parameters_.getParameter("attack")) pAtt->setValueNotifyingHost(pAtt->convertTo0to1(2.0f));
        if (auto* pGli = parameters_.getParameter("glide")) pGli->setValueNotifyingHost(pGli->convertTo0to1(35.0f));
        if (auto* pCur = parameters_.getParameter("velocityCurve")) pCur->setValueNotifyingHost(pCur->convertTo0to1(3.0f)); // Stepped
        if (auto* pTol = parameters_.getParameter("dacTolerance")) pTol->setValueNotifyingHost(pTol->convertTo0to1(0.65f));
        return true;
    }

    if (index == 3) { // Fast Legato Glide
        std::string err;
        host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), err);
        if (auto* pGain = parameters_.getParameter("gain")) pGain->setValueNotifyingHost(pGain->convertTo0to1(-6.0f));
        if (auto* pRel = parameters_.getParameter("release")) pRel->setValueNotifyingHost(pRel->convertTo0to1(180.0f));
        if (auto* pAtt = parameters_.getParameter("attack")) pAtt->setValueNotifyingHost(pAtt->convertTo0to1(2.0f));
        if (auto* pGli = parameters_.getParameter("glide")) pGli->setValueNotifyingHost(pGli->convertTo0to1(90.0f));
        if (auto* pCur = parameters_.getParameter("velocityCurve")) pCur->setValueNotifyingHost(pCur->convertTo0to1(1.0f)); // Exponential
        if (auto* pTol = parameters_.getParameter("dacTolerance")) pTol->setValueNotifyingHost(pTol->convertTo0to1(0.15f));
        return true;
    }

    if (index == 4) { // Lo-Fi Percussion & Wavetable
        std::string err;
        host_->loadPackage(bdmcs48::firmware::demo::wavetablePercussionPackage(), err);
        loadedPackagePath_.clear();
        firmwareLabel_ = host_->manifest().name;

        matrixRows_ = bdmcs48::instrument::CommandMatrix::defaults();
        matrixRows_[0].enabled = true;
        matrixRows_[0].note = 36; // C1 Kick
        matrixRows_[0].target = bdmcs48::instrument::MatrixTarget::LatchWrite;
        matrixRows_[0].value = 0.25f; // Modo 01 (Kick)
        matrixRows_[0].currentValue = 0.25f;

        matrixRows_[1].enabled = true;
        matrixRows_[1].note = 38; // D1 Snare
        matrixRows_[1].target = bdmcs48::instrument::MatrixTarget::LatchWrite;
        matrixRows_[1].value = 0.50f; // Modo 10 (Snare)
        matrixRows_[1].currentValue = 0.50f;

        matrixRows_[2].enabled = true;
        matrixRows_[2].note = 42; // F#1 Hi-Hat
        matrixRows_[2].target = bdmcs48::instrument::MatrixTarget::LatchWrite;
        matrixRows_[2].value = 0.75f; // Modo 11 (Hi-Hat)
        matrixRows_[2].currentValue = 0.75f;

        publishMatrixRows();

        if (auto* pGain = parameters_.getParameter("gain")) pGain->setValueNotifyingHost(pGain->convertTo0to1(-3.0f));
        if (auto* pRel = parameters_.getParameter("release")) pRel->setValueNotifyingHost(pRel->convertTo0to1(120.0f));
        if (auto* pAtt = parameters_.getParameter("attack")) pAtt->setValueNotifyingHost(pAtt->convertTo0to1(1.0f));
        if (auto* pTol = parameters_.getParameter("dacTolerance")) pTol->setValueNotifyingHost(pTol->convertTo0to1(0.20f));
        return true;
    }

    return true;
}

juce::File InstrumentAudioProcessor::presetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDMCS48SE")
        .getChildFile("presets");
}

juce::StringArray InstrumentAudioProcessor::presetNames() const {
    juce::StringArray names = factoryPresetNames();
    const auto directory = presetDirectory();
    if (directory.isDirectory()) {
        for (const auto& file : directory.findChildFiles(juce::File::findFiles, false, "*.bdm48")) {
            const auto base = file.getFileNameWithoutExtension();
            if (!names.contains(base)) {
                names.add(base);
            }
        }
    }
    const auto wkPresets = workspaceDirectory().getChildFile("presets");
    if (wkPresets.isDirectory() && wkPresets != directory) {
        for (const auto& file : wkPresets.findChildFiles(juce::File::findFiles, false, "*.bdm48")) {
            const auto base = file.getFileNameWithoutExtension();
            if (!names.contains(base)) {
                names.add(base);
            }
        }
    }
    return names;
}

bool InstrumentAudioProcessor::savePreset(const juce::String& name, juce::String& error) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) {
        error = "el preset necesita un nombre";
        return false;
    }
    const auto directory = presetDirectory();
    if (!directory.createDirectory()) {
        error = "no se pudo crear " + directory.getFullPathName();
        return false;
    }

    juce::MemoryBlock state;
    getStateInformation(state);
    const auto file = directory.getChildFile(juce::File::createLegalFileName(trimmed) + ".bdm48");
    if (!file.replaceWithData(state.getData(), state.getSize())) {
        error = "no se pudo escribir " + file.getFullPathName();
        return false;
    }
    return true;
}

bool InstrumentAudioProcessor::loadPreset(const juce::String& name, juce::String& error) {
    const auto trimmed = name.trim();
    const auto factory = factoryPresetNames();
    for (int i = 0; i < factory.size(); ++i) {
        if (factory[i].equalsIgnoreCase(trimmed) ||
            trimmed.startsWithIgnoreCase(juce::String(i + 1) + " ") ||
            trimmed.startsWithIgnoreCase("0" + juce::String(i + 1))) {
            return loadFactoryPreset(i, error);
        }
    }

    auto file = presetDirectory().getChildFile(
        juce::File::createLegalFileName(trimmed) + ".bdm48");
    if (!file.existsAsFile()) {
        file = workspaceDirectory().getChildFile("presets").getChildFile(
            juce::File::createLegalFileName(trimmed) + ".bdm48");
    }
    if (!file.existsAsFile()) {
        error = "preset no encontrado: " + name;
        return false;
    }
    juce::MemoryBlock state;
    if (!file.loadFileAsData(state)) {
        error = "no se pudo leer " + file.getFullPathName();
        return false;
    }
    setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    return true;
}

bool InstrumentAudioProcessor::loadRawRomData(const juce::String& filename, const juce::MemoryBlock& romData, juce::String& error) {
    const auto size = romData.getSize();
    if (size != 1024 && size != 2048 && size != 4096) {
        error = "tamano de ROM invalido: " + juce::String(size) + " bytes (esperado: 1024, 2048 o 4096 bytes)";
        return false;
    }

    const auto romDir = workspaceDirectory().getChildFile("roms");
    romDir.createDirectory();
    const auto safeName = juce::File::createLegalFileName(filename.isEmpty() ? "dropped_rom.bin" : filename);
    const auto romFile = romDir.getChildFile(safeName);
    if (!romFile.replaceWithData(romData.getData(), romData.getSize())) {
        error = "no se pudo escribir archivo de ROM: " + romFile.getFullPathName();
        return false;
    }

    const juce::String hash = bdmcs48::firmware::Sha256::hexOf(
        romData.getData(), romData.getSize()).c_str();

    RomToPackageParams params;
    params.romPath = romFile.getFullPathName();
    params.packageId = "user-" + romFile.getFileNameWithoutExtension().toLowerCase() + "-" + hash.substring(0, 8);
    params.packageName = "ROM " + romFile.getFileNameWithoutExtension();
    params.cpuVariant = size <= 1024 ? "8048" : (size <= 2048 ? "8049" : "8039");
    params.clockHz = 6000000;
    params.bankCount = size > 2048 ? 2 : 1;
    params.bankSizeBytes = std::min<int>(static_cast<int>(size), 2048);
    params.addressBits = size > 2048 ? 12 : (size > 1024 ? 11 : 10);

    if (!convertRomToPackage(params, error)) {
        return false;
    }

    const auto pkgDir = workspaceDirectory().getChildFile("firmwares").getChildFile("user").getChildFile(params.packageId);
    return loadPackage(pkgDir, error);
}

juce::MidiMessage InstrumentAudioProcessor::createSysExDump() const {
    std::vector<std::uint8_t> sysex;
    sysex.push_back(0xF0);
    sysex.push_back(0x7D);
    sysex.push_back(0x48);
    sysex.push_back(0x02);

    auto encodeFloat = [&](float val) {
        std::uint16_t q = static_cast<std::uint16_t>(std::clamp(val, 0.0f, 1.0f) * 65535.0f);
        sysex.push_back(static_cast<std::uint8_t>((q >> 12) & 0x0F));
        sysex.push_back(static_cast<std::uint8_t>((q >> 8) & 0x0F));
        sysex.push_back(static_cast<std::uint8_t>((q >> 4) & 0x0F));
        sysex.push_back(static_cast<std::uint8_t>(q & 0x0F));
    };

    const char* paramIds[] = {"gain", "release", "attack", "diagnosticTone", "tuningA4", "dcBlocker", "velocitySens", "glide", "velocityCurve", "dacTolerance"};
    for (const auto* id : paramIds) {
        float norm = 0.0f;
        if (auto* param = parameters_.getParameter(id)) {
            norm = param->getValue();
        }
        encodeFloat(norm);
    }

    for (const auto& row : matrixRows_) {
        sysex.push_back(row.enabled ? 1 : 0);
        sysex.push_back(static_cast<std::uint8_t>(row.note & 0x7F));
        sysex.push_back(row.target == bdmcs48::instrument::MatrixTarget::LatchWrite ? 1 : 0);
        encodeFloat(row.value);
        encodeFloat(row.probability);
        encodeFloat(std::clamp(row.delayMs / 5000.0f, 0.0f, 1.0f));
        sysex.push_back(static_cast<std::uint8_t>(static_cast<int>(row.mutation) & 0x07));
        encodeFloat(row.mutationAmount);
    }

    sysex.push_back(0xF7);
    return juce::MidiMessage(sysex.data(), static_cast<int>(sysex.size()));
}

bool InstrumentAudioProcessor::parseAndApplySysEx(const std::uint8_t* data, int sizeInBytes, juce::String& error) {
    if (data == nullptr || sizeInBytes < 6) {
        error = "mensaje SysEx demasiado corto";
        return false;
    }
    if (data[0] != 0xF0 || data[1] != 0x7D || data[2] != 0x48 || data[sizeInBytes - 1] != 0xF7) {
        error = "cabecera SysEx no coincide con formato ABDMCS48SE";
        return false;
    }

    const std::uint8_t command = data[3];
    if (command == 0x01) {
        midiCollector_.addMessageToQueue(createSysExDump());
        return true;
    }
    if (command != 0x02) {
        error = "comando SysEx desconocido: " + juce::String(static_cast<int>(command));
        return false;
    }

    int idx = 4;
    auto decodeFloat = [&]() -> float {
        if (idx + 4 >= sizeInBytes) return 0.0f;
        std::uint16_t q = static_cast<std::uint16_t>(
            ((data[idx] & 0x0F) << 12) |
            ((data[idx + 1] & 0x0F) << 8) |
            ((data[idx + 2] & 0x0F) << 4) |
            (data[idx + 3] & 0x0F));
        idx += 4;
        return static_cast<float>(q) / 65535.0f;
    };

    const char* paramIds[] = {"gain", "release", "attack", "diagnosticTone", "tuningA4", "dcBlocker", "velocitySens", "glide", "velocityCurve", "dacTolerance"};
    for (const auto* id : paramIds) {
        float norm = decodeFloat();
        if (auto* param = parameters_.getParameter(id)) {
            param->setValueNotifyingHost(norm);
        }
    }

    for (std::size_t i = 0; i < matrixRows_.size() && idx + 8 < sizeInBytes; ++i) {
        auto& row = matrixRows_[i];
        row.enabled = (data[idx++] != 0);
        row.note = data[idx++] & 0x7F;
        row.target = data[idx++] == 1 ? bdmcs48::instrument::MatrixTarget::LatchWrite : bdmcs48::instrument::MatrixTarget::EngineClock;
        row.value = decodeFloat();
        row.currentValue = row.value;
        row.probability = decodeFloat();
        row.delayMs = decodeFloat() * 5000.0f;
        row.mutation = static_cast<bdmcs48::instrument::MatrixMutation>(data[idx++] & 0x07);
        row.mutationAmount = decodeFloat();
    }
    publishMatrixRows();
    return true;
}

void InstrumentAudioProcessor::handleSysEx(const std::uint8_t* data, int sizeInBytes) {
    juce::String err;
    parseAndApplySysEx(data, sizeInBytes, err);
}

// --- Workspace / Settings ------------------------------------------------

juce::File InstrumentAudioProcessor::workspaceDirectory() const {
    auto props = getPropertiesFile();
    const juce::String path = props ? props->getValue("workspaceFolder", "") : juce::String("");
    if (path.isNotEmpty()) {
        juce::File dir(path);
        if (dir.isDirectory() || dir.createDirectory()) {
            return dir;
        }
    }

    // Default workspace in AppData
    auto defaultDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("ABDMCS48SE")
                          .getChildFile("Workspace");
    defaultDir.createDirectory();
    defaultDir.getChildFile("roms").createDirectory();
    defaultDir.getChildFile("firmwares").createDirectory();
    defaultDir.getChildFile("presets").createDirectory();
    defaultDir.getChildFile("banks").createDirectory();
    return defaultDir;
}

void InstrumentAudioProcessor::setWorkspaceDirectory(const juce::File& directory) {
    if (!directory.isDirectory()) {
        return;
    }
    auto props = getPropertiesFile();
    if (props) {
        props->setValue("workspaceFolder", directory.getFullPathName());
        props->saveIfNeeded();
    }
}

juce::String InstrumentAudioProcessor::getWorkspaceFolder() const {
    return workspaceDirectory().getFullPathName();
}

void InstrumentAudioProcessor::setWorkspaceFolder(const juce::String& path) {
    if (path.isEmpty()) {
        return;
    }
    juce::File dir(path);
    if (dir.isDirectory() || dir.createDirectory()) {
        setWorkspaceDirectory(dir);
    }
}

juce::StringArray InstrumentAudioProcessor::listRomFiles() const {
    juce::StringArray names;
    const auto ws = workspaceDirectory();
    const auto romDir = ws.getChildFile("roms");
    if (romDir.isDirectory()) {
        for (const auto& file : romDir.findChildFiles(juce::File::findFiles, false)) {
            if (file.hasFileExtension(".rom") || file.hasFileExtension(".bin") ||
                file.hasFileExtension(".hex") || file.hasFileExtension(".6k") ||
                file.hasFileExtension(".5k") || file.hasFileExtension(".4k")) {
                names.add(file.getFullPathName());
            }
        }
    }
    // Also check workspace root
    for (const auto& file : ws.findChildFiles(juce::File::findFiles, false)) {
        if (file.hasFileExtension(".rom") || file.hasFileExtension(".bin")) {
            names.add(file.getFullPathName());
        }
    }
    names.sort(true);
    return names;
}

bool InstrumentAudioProcessor::convertRomToPackage(const RomToPackageParams& params, juce::String& error) {
    juce::File romFile(params.romPath);
    if (!romFile.existsAsFile()) {
        error = "ROM no encontrado: " + params.romPath;
        return false;
    }

    constexpr int64_t kMaxRomBytes = 16 * 1024 * 1024;
    const auto fileSize = romFile.getSize();
    if (fileSize <= 0 || fileSize > kMaxRomBytes) {
        error = "Tamaño de ROM inválido (" + juce::String(fileSize) + " bytes; máx 16MB)";
        return false;
    }

    juce::MemoryBlock romData;
    if (!romFile.loadFileAsData(romData)) {
        error = "No se pudo leer el ROM";
        return false;
    }
    if (romData.getSize() == 0) {
        error = "ROM vacio";
        return false;
    }

    const auto ws = workspaceDirectory();
    const auto userFwDir = ws.getChildFile("firmwares").getChildFile("user");
    userFwDir.createDirectory();

    juce::String packageId = params.packageId.trim();
    if (packageId.isEmpty()) {
        packageId = params.packageName.trim().toLowerCase().replace(" ", "-");
    }
    if (packageId.isEmpty()) {
        packageId = romFile.getFileNameWithoutExtension().toLowerCase().replace(" ", "-");
    }
    packageId = juce::File::createLegalFileName(packageId);

    juce::String packageName = params.packageName.trim();
    if (packageName.isEmpty()) {
        packageName = romFile.getFileNameWithoutExtension();
    }

    auto pkgDir = userFwDir.getChildFile(packageId);
    pkgDir.createDirectory();
    pkgDir.getChildFile("roms").createDirectory();

    std::string imagesJson;
    if (romData.getSize() <= 2048) {
        juce::String romFileName = "program.rom";
        auto outRom = pkgDir.getChildFile("roms").getChildFile(romFileName);
        if (!outRom.replaceWithData(romData.getData(), romData.getSize())) {
            error = "No se pudo escribir el ROM en el paquete";
            return false;
        }

        std::vector<std::uint8_t> romVec(static_cast<std::size_t>(romData.getSize()));
        std::memcpy(romVec.data(), romData.getData(), static_cast<std::size_t>(romData.getSize()));
        std::string romHash = bdmcs48::firmware::Sha256::hexOf(romVec);

        imagesJson = "    {\n"
                     "      \"id\": \"program\",\n"
                     "      \"file\": \"roms/" + romFileName.toStdString() + "\",\n"
                     "      \"region\": \"program\",\n"
                     "      \"bank\": 0,\n"
                     "      \"offset\": 0,\n"
                     "      \"size\": " + std::to_string(romData.getSize()) + ",\n"
                     "      \"sha256\": \"" + romHash + "\"\n"
                     "    }\n";
    } else {
        const std::size_t totalBytes = static_cast<std::size_t>(romData.getSize());
        const std::size_t chunk0Size = 2048;
        const std::size_t chunk1Size = std::min(totalBytes - 2048, static_cast<std::size_t>(2048));

        auto outRom0 = pkgDir.getChildFile("roms").getChildFile("program-bank0.rom");
        if (!outRom0.replaceWithData(romData.getData(), chunk0Size)) {
            error = "No se pudo escribir el banco 0 en el paquete";
            return false;
        }
        auto outRom1 = pkgDir.getChildFile("roms").getChildFile("program-bank1.rom");
        if (!outRom1.replaceWithData(static_cast<const char*>(romData.getData()) + 2048, chunk1Size)) {
            error = "No se pudo escribir el banco 1 en el paquete";
            return false;
        }

        std::vector<std::uint8_t> vec0(chunk0Size);
        std::memcpy(vec0.data(), romData.getData(), chunk0Size);
        std::string hash0 = bdmcs48::firmware::Sha256::hexOf(vec0);

        std::vector<std::uint8_t> vec1(chunk1Size);
        std::memcpy(vec1.data(), static_cast<const char*>(romData.getData()) + 2048, chunk1Size);
        std::string hash1 = bdmcs48::firmware::Sha256::hexOf(vec1);

        imagesJson = "    {\n"
                     "      \"id\": \"program-bank0\",\n"
                     "      \"file\": \"roms/program-bank0.rom\",\n"
                     "      \"region\": \"program\",\n"
                     "      \"bank\": 0,\n"
                     "      \"offset\": 0,\n"
                     "      \"size\": " + std::to_string(chunk0Size) + ",\n"
                     "      \"sha256\": \"" + hash0 + "\"\n"
                     "    },\n"
                     "    {\n"
                     "      \"id\": \"program-bank1\",\n"
                     "      \"file\": \"roms/program-bank1.rom\",\n"
                     "      \"region\": \"program\",\n"
                     "      \"bank\": 1,\n"
                     "      \"offset\": 0,\n"
                     "      \"size\": " + std::to_string(chunk1Size) + ",\n"
                     "      \"sha256\": \"" + hash1 + "\"\n"
                     "    }\n";
    }

    // Build manifest
    std::string manifest;
    manifest += "{\n";
    manifest += "  \"format\": \"bdmcs48se-firmware\",\n";
    manifest += "  \"formatVersion\": 1,\n";
    manifest += "  \"id\": \"" + jsonEscape(packageId.toStdString()) + "\",\n";
    manifest += "  \"name\": \"" + jsonEscape(packageName.toStdString()) + "\",\n";
    manifest += "  \"version\": \"" + jsonEscape(params.packageVersion.toStdString()) + "\",\n";
    if (params.author.trim().isNotEmpty())
        manifest += "  \"author\": \"" + jsonEscape(params.author.toStdString()) + "\",\n";
    if (params.description.trim().isNotEmpty())
        manifest += "  \"description\": \"" + jsonEscape(params.description.toStdString()) + "\",\n";
    manifest += "  \"license\": \"" + jsonEscape(params.license.toStdString()) + "\",\n";
    if (params.provenance.trim().isNotEmpty())
        manifest += "  \"provenance\": \"" + jsonEscape(params.provenance.toStdString()) + "\",\n";
    manifest += "  \"machine\": {\n";
    manifest += "    \"profile\": \"" + jsonEscape(params.profile.toStdString()) + "\",\n";
    manifest += "    \"cpu\": { \"family\": \"mcs48\", \"variant\": \"" + jsonEscape(params.cpuVariant.toStdString()) + "\", \"clockHz\": " + std::to_string(params.clockHz) + " },\n";
    manifest += "    \"programMemory\": { \"addressBits\": " + std::to_string(params.addressBits) + ", \"bankSizeBytes\": " + std::to_string(params.bankSizeBytes) + ", \"bankCount\": " + std::to_string(params.bankCount) + " }\n";
    manifest += "  },\n";
    manifest += "  \"images\": [\n";
    manifest += imagesJson;
    manifest += "  ],\n";
    manifest += "  \"inputs\": [],\n";
    manifest += "  \"outputs\": [\n";
    manifest += "    { \"id\": \"audio\", \"type\": \"mono_stream\" }\n";
    manifest += "  ],\n";
    manifest += "  \"parameters\": [\n";
    manifest += "    { \"id\": \"engine_clock\", \"type\": \"float\", \"minimum\": 0.0625, \"maximum\": 8.0, \"initial\": 1.0, \"hostAutomatable\": true }\n";
    manifest += "  ]\n";
    manifest += "}\n";

    auto manifestFile = pkgDir.getChildFile("firmware.json");
    if (!manifestFile.replaceWithText(manifest)) {
        error = "No se pudo escribir firmware.json";
        return false;
    }

    return true;
}

// --- Firmwares ------------------------------------------------------------

juce::File InstrumentAudioProcessor::firmwareDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDMCS48SE")
        .getChildFile("firmwares");
}

juce::StringArray InstrumentAudioProcessor::firmwareNames() const {
    juce::StringArray names;
    
    // User firmwares from AppData
    const auto directory = firmwareDirectory();
    if (directory.isDirectory()) {
        for (const auto& file : directory.findChildFiles(juce::File::findDirectories, false)) {
            if (file.getChildFile("firmware.json").existsAsFile()) {
                names.add(file.getFileName());
            }
        }
    }
    
    // User firmwares from workspace
    const auto ws = workspaceDirectory();
    const auto wsFwDir = ws.getChildFile("firmwares");
    if (wsFwDir.isDirectory()) {
        for (const auto& file : wsFwDir.findChildFiles(juce::File::findDirectories, false)) {
            if (file.getChildFile("firmware.json").existsAsFile()) {
                juce::String name = file.getFileName();
                // Avoid duplicates
                bool exists = false;
                for (int i = 0; i < names.size(); ++i) {
                    if (names[i] == name) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    names.add(name);
                }
            }
        }
        // Also check user subdir
        const auto wsUserFwDir = wsFwDir.getChildFile("user");
        if (wsUserFwDir.isDirectory()) {
            for (const auto& file : wsUserFwDir.findChildFiles(juce::File::findDirectories, false)) {
                if (file.getChildFile("firmware.json").existsAsFile()) {
                    juce::String name = file.getFileName();
                    bool exists = false;
                    for (int i = 0; i < names.size(); ++i) {
                        if (names[i] == name) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        names.add(name);
                    }
                }
            }
        }
    }
    
    // Add built-in firmwares
    names.add("MCS-48 Enhanced Mono Voice (Built-in)");
    names.add("MCS-48 Lo-Fi Percussion & Wavetable (Built-in)");
    names.add("MCS-48 Mono Voice (Built-in)");
    names.add("MCS-48 Demo Tone (Built-in)");
    names.add("MCS-48 Diagnostic Test ROM (Built-in)");
    names.sort(true);
    return names;
}

bool InstrumentAudioProcessor::loadFirmware(const juce::String& name, juce::String& error) {
    juce::String trimmed = name.trim();
    if (trimmed.isEmpty()) {
        error = "el firmware necesita un nombre";
        return false;
    }
    
    // Check if it's a built-in firmware
    if (trimmed == "MCS-48 Enhanced Mono Voice (Built-in)") {
        if (!host_) {
            error = "host no inicializado";
            return false;
        }
        std::string err;
        if (host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), err)) {
            loadedPackagePath_.clear();
            const auto& manifest = host_->manifest();
            firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
            return true;
        }
        error = err;
        return false;
    } else if (trimmed == "MCS-48 Lo-Fi Percussion & Wavetable (Built-in)") {
        if (!host_) {
            error = "host no inicializado";
            return false;
        }
        std::string err;
        if (host_->loadPackage(bdmcs48::firmware::demo::wavetablePercussionPackage(), err)) {
            loadedPackagePath_.clear();
            const auto& manifest = host_->manifest();
            firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
            return true;
        }
        error = err;
        return false;
    } else if (trimmed == "MCS-48 Diagnostic Test ROM (Built-in)") {
        if (!host_) {
            error = "host no inicializado";
            return false;
        }
        std::string err;
        if (host_->loadPackage(bdmcs48::firmware::demo::diagnosticPackage(), err)) {
            loadedPackagePath_.clear();
            const auto& manifest = host_->manifest();
            firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
            return true;
        }
        error = err;
        return false;
    } else if (trimmed == "MCS-48 Mono Voice (Built-in)") {
        if (!host_) {
            error = "host no inicializado";
            return false;
        }
        std::string err;
        if (host_->loadPackage(bdmcs48::firmware::demo::monoVoicePackage(), err)) {
            loadedPackagePath_.clear();
            const auto& manifest = host_->manifest();
            firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
            return true;
        }
        error = err;
        return false;
    } else if (trimmed == "MCS-48 Demo Tone (Built-in)") {
        if (!host_) {
            error = "host no inicializado";
            return false;
        }
        std::string err;
        if (host_->loadPackage(bdmcs48::firmware::demo::demoPackage(), err)) {
            loadedPackagePath_.clear();
            const auto& manifest = host_->manifest();
            firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
            return true;
        }
        error = err;
        return false;
    }
    
    // Try to load from firmware directories
    auto tryDir = [&](const juce::File& dir) -> bool {
        if (dir.getChildFile("firmware.json").existsAsFile()) {
            return loadPackage(dir, error);
        }
        return false;
    };
    
    if (tryDir(firmwareDirectory().getChildFile(trimmed))) return true;
    if (tryDir(workspaceDirectory().getChildFile("firmwares").getChildFile(trimmed))) return true;
    if (tryDir(workspaceDirectory().getChildFile("firmwares").getChildFile("user").getChildFile(trimmed))) return true;
    
    error = "firmware no encontrado: " + name;
    return false;
}

// --- Estado para la WebView ----------------------------------------------

juce::String InstrumentAudioProcessor::firmwareHash() const {
    if (host_ == nullptr) {
        return {};
    }
    const auto& manifest = host_->manifest();
    if (manifest.images.empty()) {
        return {};
    }
    return juce::String(manifest.images.front().sha256).substring(0, 12);
}

juce::String InstrumentAudioProcessor::uiStateJson() {
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("protocol", kUiProtocolVersion);
    root->setProperty("gain", parameters_.getRawParameterValue("gain")->load());
    root->setProperty("release", parameters_.getRawParameterValue("release")->load());
    root->setProperty("attack", parameters_.getRawParameterValue("attack") ? parameters_.getRawParameterValue("attack")->load() : 4.0f);
    root->setProperty("diagnosticTone", isDiagnosticToneEnabled());
    root->setProperty("tuningA4", parameters_.getRawParameterValue("tuningA4") ? parameters_.getRawParameterValue("tuningA4")->load() : 440.0f);
    root->setProperty("dcBlocker", parameters_.getRawParameterValue("dcBlocker") ? parameters_.getRawParameterValue("dcBlocker")->load() : 0.995f);
    root->setProperty("velocitySens", parameters_.getRawParameterValue("velocitySens") ? parameters_.getRawParameterValue("velocitySens")->load() : 1.0f);
    root->setProperty("glide", parameters_.getRawParameterValue("glide") ? parameters_.getRawParameterValue("glide")->load() : 0.0f);
    root->setProperty("velocityCurve", parameters_.getRawParameterValue("velocityCurve") ? parameters_.getRawParameterValue("velocityCurve")->load() : 0.0f);
    root->setProperty("dacTolerance", parameters_.getRawParameterValue("dacTolerance") ? parameters_.getRawParameterValue("dacTolerance")->load() : 0.0f);
    root->setProperty("dacQuantize", parameters_.getRawParameterValue("dacQuantize") ? parameters_.getRawParameterValue("dacQuantize")->load() : 7.0f);
    root->setProperty("freeze", parameters_.getRawParameterValue("freeze") ? parameters_.getRawParameterValue("freeze")->load() : 0.0f);
    root->setProperty("seed", static_cast<int>(matrixSeed()));

    juce::Array<juce::var> rows;
    for (const auto& row : matrixRows_) {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("enabled", row.enabled);
        object->setProperty("note", row.note);
        juce::String targetStr = "engine_clock";
        if (row.target == bdmcs48::instrument::MatrixTarget::LatchWrite) targetStr = "latch_write";
        else if (row.target == bdmcs48::instrument::MatrixTarget::DacQuantize) targetStr = "dac_quantize";
        else if (row.target == bdmcs48::instrument::MatrixTarget::Freeze) targetStr = "freeze";
        object->setProperty("target", targetStr);
        object->setProperty("value", row.value);
        object->setProperty("currentValue", row.currentValue);
        object->setProperty("probability", row.probability);
        object->setProperty("delayMs", row.delayMs);
        object->setProperty("mutation", juce::String(mutationName(row.mutation)));
        object->setProperty("mutationAmount", row.mutationAmount);
        rows.add(juce::var(object.release()));
    }
    root->setProperty("rows", rows);

    auto firmware = std::make_unique<juce::DynamicObject>();
    firmware->setProperty("label", firmwareLabel());
    firmware->setProperty("hash", firmwareHash());
    if (host_ != nullptr) {
        const auto& manifest = host_->manifest();
        firmware->setProperty("id", juce::String(manifest.id));
        firmware->setProperty("name", juce::String(manifest.name));
        firmware->setProperty("version", juce::String(manifest.version));
    }
    root->setProperty("firmware", juce::var(firmware.release()));

    auto engine = std::make_unique<juce::DynamicObject>();
    engine->setProperty("note", currentNote());
    engine->setProperty("frequency", currentFrequency());
    engine->setProperty("sounding", sounding());
    engine->setProperty("diagnosticTone", isDiagnosticToneEnabled());
    engine->setProperty("level", level());
    engine->setProperty("lastCommandId", lastCommandId());
    engine->setProperty("lastCommandValue", lastCommandValue());
    engine->setProperty("droppedMatrixCommands", static_cast<double>(matrixDroppedCommands()));
    engine->setProperty("underruns", static_cast<double>(underruns()));
    if (host_ != nullptr) {
        const auto status = host_->status();
        engine->setProperty("pc", status.programCounter);
        engine->setProperty("a", status.accumulator);
        engine->setProperty("psw", status.psw);
        engine->setProperty("bank", status.bank);
        engine->setProperty("cycles", static_cast<double>(status.cpuCycles));
        engine->setProperty("faulted", status.faulted != 0);
        engine->setProperty("faultMessage", juce::String(status.faultMessage));
        engine->setProperty("droppedCommands", static_cast<double>(host_->droppedCommands()));
        engine->setProperty("peakLevel", static_cast<double>(status.peakLevel));
        engine->setProperty("rmsLevel", static_cast<double>(status.rmsLevel));
        juce::Array<juce::var> waveArr;
        for (std::size_t w = 0; w < 8; ++w) {
            waveArr.add(status.waveform[w]);
        }
        engine->setProperty("waveform", waveArr);
    }
    root->setProperty("engine", juce::var(engine.release()));

    juce::Array<juce::var> presets;
    for (const auto& name : presetNames()) {
        presets.add(name);
    }
    root->setProperty("presets", presets);

    juce::Array<juce::var> firmwares;
    for (const auto& name : firmwareNames()) {
        firmwares.add(name);
    }
    root->setProperty("firmwares", firmwares);

    return juce::JSON::toString(juce::var(root.release()));
}

juce::AudioProcessorEditor* InstrumentAudioProcessor::createEditor() {
#if BDMCS48SE_HAS_EDITOR
    return createInstrumentEditor(*this);
#else
    return nullptr;
#endif
}

void InstrumentAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = parameters_.copyState();

    // Guardado portable: almacenar firmwareId y firmwareSha256
    juce::String fwId;
    juce::String fwSha256;
    if (host_ != nullptr) {
        const auto& manifest = host_->manifest();
        fwId = juce::String(manifest.id);
        if (!manifest.images.empty()) {
            fwSha256 = juce::String(manifest.images.front().sha256);
        }
    }
    state.setProperty("firmwareId", fwId, nullptr);
    state.setProperty("firmwareSha256", fwSha256, nullptr);
    state.setProperty("firmwarePath", juce::String(loadedPackagePath_), nullptr);

    state.appendChild(matrixState(), nullptr);
    if (auto xml = state.createXml()) {
        copyXmlToBinary(*xml, destination);
    }
}

void InstrumentAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) {
        return;
    }
    const auto state = juce::ValueTree::fromXml(*xml);
    parameters_.replaceState(state);

    if (const auto matrix = state.getChildWithName("CommandMatrix"); matrix.isValid()) {
        applyMatrixState(matrix);
    }

    pendingStateFirmwareId_ = xml->getStringAttribute("firmwareId").toStdString();
    pendingStateFirmwareSha256_ = xml->getStringAttribute("firmwareSha256").toStdString();
    pendingStatePath_ = xml->getStringAttribute("firmwarePath").toStdString();
    triggerAsyncUpdate();
}

void InstrumentAudioProcessor::handleAsyncUpdate() {
    bool loaded = false;

    // 1. Intentar resolver el firmware de forma portable por firmwareId
    if (!pendingStateFirmwareId_.empty()) {
        const juce::String targetId = juce::String(pendingStateFirmwareId_);
        if (targetId == "mcs48.demo.enhanced" || targetId == "mcs48.demo.monovoice" || targetId == "mcs48.demo.tone") {
            std::string err;
            if (host_->loadPackage(bdmcs48::firmware::demo::enhancedMonoVoicePackage(), err)) {
                loadedPackagePath_.clear();
                const auto& manifest = host_->manifest();
                firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
                loaded = true;
            }
        } else if (targetId == "mcs48.musical.wavetable_percussion") {
            std::string err;
            if (host_->loadPackage(bdmcs48::firmware::demo::wavetablePercussionPackage(), err)) {
                loadedPackagePath_.clear();
                const auto& manifest = host_->manifest();
                firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
                loaded = true;
            }
        } else if (targetId == "mcs48.test.diagnostic") {
            std::string err;
            if (host_->loadPackage(bdmcs48::firmware::demo::diagnosticPackage(), err)) {
                loadedPackagePath_.clear();
                const auto& manifest = host_->manifest();
                firmwareLabel_ = juce::String(manifest.name) + " " + juce::String(manifest.version);
                loaded = true;
            }
        } else if (targetId == "canon.mario_arcade.sound" || targetId == "mario-bros-sound-board") {
            const auto marioDirUser = workspaceDirectory().getChildFile("firmwares").getChildFile("user").getChildFile("mario-bros-sound-board");
            const auto marioDirApp = firmwareDirectory().getChildFile("mario-bros-sound-board");
            juce::String err;
            if (marioDirUser.getChildFile("firmware.json").existsAsFile() && loadPackage(marioDirUser, err)) {
                loaded = true;
            } else if (marioDirApp.getChildFile("firmware.json").existsAsFile() && loadPackage(marioDirApp, err)) {
                loaded = true;
            }
        } else {
            // Buscar en workspace/firmwares por manifest.id o sha256
            auto searchDir = [&](const juce::File& baseDir) -> bool {
                if (!baseDir.isDirectory()) return false;
                for (const auto& entry : baseDir.findChildFiles(juce::File::findDirectories, false)) {
                    const auto jsonFile = entry.getChildFile("firmware.json");
                    if (jsonFile.existsAsFile()) {
                        juce::String err;
                        juce::var parsed = juce::JSON::parse(jsonFile);
                        if (parsed.isObject()) {
                            const juce::String parsedId = parsed.getProperty("id", "").toString();
                            if (parsedId == targetId) {
                                return loadPackage(entry, err);
                            }
                        }
                    }
                }
                return false;
            };

            if (searchDir(workspaceDirectory().getChildFile("firmwares").getChildFile("user"))) loaded = true;
            else if (searchDir(workspaceDirectory().getChildFile("firmwares"))) loaded = true;
            else if (searchDir(firmwareDirectory())) loaded = true;
        }
    }

    // 2. Si no se resolvio por ID, intentar por la ruta (fallback secundario para sesion local)
    if (!loaded && !pendingStatePath_.empty()) {
        juce::String error;
        const juce::File file(pendingStatePath_);
        if (file.exists()) {
            loaded = loadPackage(file, error);
        }
    }

    // 3. Fallback de resiliencia final si la ROM original no esta disponible en el equipo
    if (!loaded) {
        loadDefaultFirmware();
    }

    pendingStateFirmwareId_.clear();
    pendingStateFirmwareSha256_.clear();
    pendingStatePath_.clear();
}

} // namespace bdmcs48::plugin

// Punto de entrada que usan los envoltorios de JUCE (VST3, Standalone).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new bdmcs48::plugin::InstrumentAudioProcessor();
}
