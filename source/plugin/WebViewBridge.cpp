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

} // namespace bdmcs48::plugin
