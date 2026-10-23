#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "firmware/demo/MonoVoiceFirmware.h"
#include "plugin/UiProtocol.h"

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

} // namespace

InstrumentAudioProcessor::InstrumentAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "PARAMETERS", createParameterLayout()),
      matrixRows_(bdmcs48::instrument::CommandMatrix::defaults()) {
    matrix_.prepare(44'100.0);
    matrix_.publishRows(matrixRows_);
    matrix_.consumePublishedRows();
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

    matrix_.prepare(sampleRate);
    matrix_.clearSchedule();
    midiCollector_.reset(sampleRate);
    midiCollector_.ensureStorageAllocated(4 * 1024);
    underruns_.store(0, std::memory_order_relaxed);

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
        node.setProperty("target", juce::String(
            row.target == bdmcs48::instrument::MatrixTarget::EngineClock ? "engine_clock"
                                                                        : "latch_write"),
            nullptr);
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
        row.target = target == "latch_write" ? bdmcs48::instrument::MatrixTarget::LatchWrite
                                             : bdmcs48::instrument::MatrixTarget::EngineClock;
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

juce::File InstrumentAudioProcessor::presetDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ABDMCS48SE")
        .getChildFile("presets");
}

juce::StringArray InstrumentAudioProcessor::presetNames() const {
    juce::StringArray names;
    const auto directory = presetDirectory();
    if (!directory.isDirectory()) {
        return names;
    }
    for (const auto& file : directory.findChildFiles(juce::File::findFiles, false, "*.bdm48")) {
        names.add(file.getFileNameWithoutExtension());
    }
    names.sort(true);
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
    const auto file = presetDirectory().getChildFile(
        juce::File::createLegalFileName(name.trim()) + ".bdm48");
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
    root->setProperty("seed", static_cast<int>(matrixSeed()));

    juce::Array<juce::var> rows;
    for (const auto& row : matrixRows_) {
        auto object = std::make_unique<juce::DynamicObject>();
        object->setProperty("enabled", row.enabled);
        object->setProperty("note", row.note);
        object->setProperty("target", juce::String(
            row.target == bdmcs48::instrument::MatrixTarget::EngineClock ? "engine_clock"
                                                                        : "latch_write"));
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
    }
    root->setProperty("engine", juce::var(engine.release()));

    juce::Array<juce::var> presets;
    for (const auto& name : presetNames()) {
        presets.add(name);
    }
    root->setProperty("presets", presets);

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
