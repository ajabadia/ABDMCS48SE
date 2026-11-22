#include "plugin/WebViewBridge.h"

#include <cmath>

#include "instrument/CommandMatrix.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UiProtocol.h"

namespace bdmcs48::plugin {
namespace {

using bdmcs48::instrument::CommandMatrix;
using bdmcs48::instrument::MatrixMutation;
using bdmcs48::instrument::MatrixRow;
using bdmcs48::instrument::MatrixTarget;

MatrixMutation mutationFromString(const juce::String& name) {
    if (name == "increment") return MatrixMutation::Increment;
    if (name == "decrement") return MatrixMutation::Decrement;
    if (name == "random") return MatrixMutation::Random;
    if (name == "wrap") return MatrixMutation::Wrap;
    return MatrixMutation::None;
}

juce::var readProperty(const juce::DynamicObject& object, const juce::Identifier& name) {
    return object.getProperty(name);
}

} // namespace

WebViewBridge::WebViewBridge(InstrumentAudioProcessor& processor)
    : processor_(processor) {}

juce::String WebViewBridge::stateJson() {
    return processor_.uiStateJson();
}

juce::String WebViewBridge::presetsJson() {
    juce::Array<juce::var> names;
    for (const auto& name : processor_.presetNames()) {
        names.add(name);
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("protocol", kUiProtocolVersion);
    root->setProperty("presets", names);
    return juce::JSON::toString(juce::var(root.release()));
}

juce::String WebViewBridge::firmwaresJson() {
    juce::Array<juce::var> names;
    for (const auto& name : processor_.firmwareNames()) {
        names.add(name);
    }

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("protocol", kUiProtocolVersion);
    root->setProperty("firmwares", names);
    return juce::JSON::toString(juce::var(root.release()));
}

juce::String WebViewBridge::noteOn(int note, float velocity) {
    if (note < 0 || note > 127) {
        return "nota fuera de rango";
    }
    processor_.noteOnFromUi(note, velocity);
    return {};
}

juce::String WebViewBridge::noteOff(int note) {
    if (note < 0 || note > 127) {
        return "nota fuera de rango";
    }
    processor_.noteOffFromUi(note);
    return {};
}

juce::String WebViewBridge::allNotesOff() {
    processor_.allNotesOffFromUi();
    return {};
}

juce::String WebViewBridge::setParameter(const juce::String& id, float normalizedValue) {
    auto* parameter = processor_.parameters().getParameter(id);
    if (parameter == nullptr) {
        return "parametro desconocido: " + id;
    }
    parameter->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
    return {};
}

juce::String WebViewBridge::beginParameterGesture(const juce::String& id) {
    auto* parameter = processor_.parameters().getParameter(id);
    if (parameter != nullptr) {
        parameter->beginChangeGesture();
    }
    return {};
}

juce::String WebViewBridge::endParameterGesture(const juce::String& id) {
    auto* parameter = processor_.parameters().getParameter(id);
    if (parameter != nullptr) {
        parameter->endChangeGesture();
    }
    return {};
}

juce::String WebViewBridge::applyRowJson(int index, const juce::String& json) {
    if (index < 0 || index >= static_cast<int>(CommandMatrix::kRows)) {
        return "fila fuera de rango";
    }

    juce::var parsed;
    if (juce::JSON::parse(json, parsed).failed()) {
        return "json invalido";
    }
    auto* object = parsed.getDynamicObject();
    if (object == nullptr) {
        return "la fila debe ser un objeto";
    }

    auto row = processor_.matrixRows()[static_cast<std::size_t>(index)];

    if (object->hasProperty("enabled")) {
        row.enabled = static_cast<bool>(readProperty(*object, "enabled"));
    }
    if (object->hasProperty("note")) {
        row.note = static_cast<int>(readProperty(*object, "note"));
    }
    if (object->hasProperty("target")) {
        const auto target = readProperty(*object, "target").toString();
        if (target == "engine_clock") {
            row.target = MatrixTarget::EngineClock;
        } else if (target == "latch_write") {
            row.target = MatrixTarget::LatchWrite;
        } else if (target == "dac_quantize") {
            row.target = MatrixTarget::DacQuantize;
        } else if (target == "freeze") {
            row.target = MatrixTarget::Freeze;
        } else {
            return "destino desconocido: " + target;
        }
    }
    if (object->hasProperty("value")) {
        row.value = static_cast<float>(readProperty(*object, "value"));
    }
    if (object->hasProperty("probability")) {
        row.probability = static_cast<float>(readProperty(*object, "probability"));
    }
    if (object->hasProperty("delayMs")) {
        row.delayMs = static_cast<float>(readProperty(*object, "delayMs"));
    }
    if (object->hasProperty("mutation")) {
        row.mutation = mutationFromString(readProperty(*object, "mutation").toString());
    }
    if (object->hasProperty("mutationAmount")) {
        row.mutationAmount = static_cast<float>(readProperty(*object, "mutationAmount"));
    }

    if (!processor_.setMatrixRow(index, row)) {
        return "no se pudo actualizar la fila";
    }
    return {};
}

juce::String WebViewBridge::resetMachine() {
    processor_.resetMachine();
    processor_.allNotesOffFromUi();
    return {};
}

juce::String WebViewBridge::savePreset(const juce::String& name) {
    juce::String error;
    if (!processor_.savePreset(name, error)) {
        return error;
    }
    return {};
}

juce::String WebViewBridge::loadPreset(const juce::String& name) {
    juce::String error;
    if (!processor_.loadPreset(name, error)) {
        return error;
    }
    return {};
}

juce::String WebViewBridge::loadFactoryPreset(int index) {
    juce::String error;
    if (!processor_.loadFactoryPreset(index, error)) {
        return error;
    }
    return {};
}

juce::String WebViewBridge::loadRawRomData(const juce::String& filename, const juce::String& base64Data) {
    juce::MemoryOutputStream mos;
    if (!juce::Base64::convertFromBase64(mos, base64Data)) {
        return "error decodificando datos base64 de la ROM";
    }
    juce::MemoryBlock block(mos.getData(), mos.getDataSize());
    juce::String error;
    if (!processor_.loadRawRomData(filename, block, error)) {
        return error;
    }
    return {};
}

juce::String WebViewBridge::exportSysExDump() {
    const auto msg = processor_.createSysExDump();
    return juce::String::toHexString(msg.getRawData(), msg.getRawDataSize());
}

juce::String WebViewBridge::importSysExDump(const juce::String& hexString) {
    juce::MemoryBlock block;
    block.loadFromHexString(hexString.trim());
    if (block.isEmpty()) {
        return "cadena hexadecimal de SysEx vacia o invalida";
    }
    juce::String error;
    if (!processor_.parseAndApplySysEx(reinterpret_cast<const std::uint8_t*>(block.getData()),
                                       static_cast<int>(block.getSize()), error)) {
        return error;
    }
    return {};
}

juce::String WebViewBridge::loadFirmware(const juce::String& name) {
    juce::String error;
    if (!processor_.loadFirmware(name, error)) {
        return error;
    }
    return {};
}

juce::String WebViewBridge::setDiagnosticTone(bool enabled) {
    processor_.setDiagnosticToneEnabled(enabled);
    return {};
}

juce::String WebViewBridge::getWorkspaceFolder() {
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("path", processor_.getWorkspaceFolder());
    return juce::JSON::toString(juce::var(root.release()));
}

juce::String WebViewBridge::setWorkspaceFolder(const juce::String& path) {
    processor_.setWorkspaceFolder(path);
    return {};
}

void WebViewBridge::browseWorkspaceFolder(std::function<void(juce::String)> completion) {
    auto chooser = std::make_shared<juce::FileChooser>("Seleccionar carpeta de trabajo",
                                                       processor_.workspaceDirectory(),
                                                       "");
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                            juce::FileBrowserComponent::canSelectDirectories,
                        [this, chooser, completion = std::move(completion)](
                            const juce::FileChooser& completedChooser) mutable {
        juce::String result = "cancelado";
        auto folder = completedChooser.getResult();
        if (folder.isDirectory()) {
            processor_.setWorkspaceDirectory(folder);
            auto root = std::make_unique<juce::DynamicObject>();
            root->setProperty("path", folder.getFullPathName());
            result = juce::JSON::toString(juce::var(root.release()));
        }
        completion(std::move(result));
    });
}

juce::String WebViewBridge::listRomFiles() {
    juce::Array<juce::var> files;
    for (const auto& name : processor_.listRomFiles()) {
        files.add(name);
    }
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("files", files);
    return juce::JSON::toString(juce::var(root.release()));
}

void WebViewBridge::pickRomFile(std::function<void(juce::String)> completion) {
    auto chooser = std::make_shared<juce::FileChooser>("Seleccionar archivo ROM o ZIP",
                                                       processor_.workspaceDirectory().getChildFile("roms"),
                                                       ".rom;*.bin;*.hex;*.6k;*.5k;*.4k;*.zip");
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                            juce::FileBrowserComponent::canSelectFiles,
                        [chooser, completion = std::move(completion)](
                            const juce::FileChooser& completedChooser) mutable {
        juce::String result = "cancelado";
        const auto file = completedChooser.getResult();
        if (file.existsAsFile()) {
            auto root = std::make_unique<juce::DynamicObject>();
            root->setProperty("path", file.getFullPathName());
            root->setProperty("name", file.getFileName());
            const auto ext = file.getFileExtension().toLowerCase();
            if (ext == "zip") {
                root->setProperty("isZip", true);
            } else {
                root->setProperty("size", static_cast<double>(file.getSize()));
            }
            result = juce::JSON::toString(juce::var(root.release()));
        }
        completion(std::move(result));
    });
}

juce::String WebViewBridge::convertRomToPackage(const juce::String& json) {
    juce::var parsed;
    if (juce::JSON::parse(json, parsed).failed()) {
        return "json invalido";
    }
    auto* object = parsed.getDynamicObject();
    if (object == nullptr) {
        return "se esperaba un objeto";
    }

    InstrumentAudioProcessor::RomToPackageParams params;
    if (object->hasProperty("romPath")) params.romPath = object->getProperty("romPath").toString();
    if (object->hasProperty("packageId")) params.packageId = object->getProperty("packageId").toString();
    if (object->hasProperty("packageName")) params.packageName = object->getProperty("packageName").toString();
    if (object->hasProperty("packageVersion")) params.packageVersion = object->getProperty("packageVersion").toString();
    if (object->hasProperty("author")) params.author = object->getProperty("author").toString();
    if (object->hasProperty("description")) params.description = object->getProperty("description").toString();
    if (object->hasProperty("license")) params.license = object->getProperty("license").toString();
    if (object->hasProperty("provenance")) params.provenance = object->getProperty("provenance").toString();
    if (object->hasProperty("profile")) params.profile = object->getProperty("profile").toString();
    if (object->hasProperty("cpuVariant")) params.cpuVariant = object->getProperty("cpuVariant").toString();
    if (object->hasProperty("clockHz")) params.clockHz = static_cast<int>(object->getProperty("clockHz"));
    if (object->hasProperty("bankCount")) params.bankCount = static_cast<int>(object->getProperty("bankCount"));
    if (object->hasProperty("bankSizeBytes")) params.bankSizeBytes = static_cast<int>(object->getProperty("bankSizeBytes"));
    if (object->hasProperty("addressBits")) params.addressBits = static_cast<int>(object->getProperty("addressBits"));

    juce::String error;
    if (!processor_.convertRomToPackage(params, error)) {
        return error;
    }
    return {};
}

} // namespace bdmcs48::plugin
