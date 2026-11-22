// WebViewBridge.h - Contrato JSON entre la WebView y C++.
//
// La WebView solo envia intenciones; este puente valida, aplica y responde.
// C++ es la unica fuente de verdad: la UI nunca decide el estado del motor.
//
// Todos los metodos se llaman en el hilo de mensajes. Devuelven una cadena
// vacia cuando la operacion tuvo exito o el mensaje de error cuando fallo,
// de modo que el frontend pueda mostrar el fallo sin inventarse el estado.

#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <string>

namespace bdmcs48::plugin {

class InstrumentAudioProcessor;

class WebViewBridge {
public:
    explicit WebViewBridge(InstrumentAudioProcessor& processor);

    // Estado completo (parametros, matriz, firmware, motor y presets).
    juce::String stateJson();
    juce::String presetsJson();
    juce::String firmwaresJson();

    // Teclado virtual.
    juce::String noteOn(int note, float velocity);
    juce::String noteOff(int note);
    juce::String allNotesOff();

    // Parametros: id del APVTS y valor normalizado 0..1.
    juce::String setParameter(const juce::String& id, float normalizedValue);
    juce::String beginParameterGesture(const juce::String& id);
    juce::String endParameterGesture(const juce::String& id);

    // Fila de la matriz: indice 0..7 y objeto JSON con los campos que cambian.
    juce::String applyRowJson(int index, const juce::String& json);

    // Transporte.
    juce::String resetMachine();

    // Presets del Standalone.
    juce::String savePreset(const juce::String& name);
    juce::String loadPreset(const juce::String& name);
    juce::String loadFactoryPreset(int index);
    juce::String loadFirmware(const juce::String& name);
    juce::String setDiagnosticTone(bool enabled);

    // Microcode Drag-and-Drop & SysEx
    juce::String loadRawRomData(const juce::String& filename, const juce::String& base64Data);
    juce::String exportSysExDump();
    juce::String importSysExDump(const juce::String& hexString);

    // Workspace / ROM to Package
    juce::String getWorkspaceFolder();
    juce::String setWorkspaceFolder(const juce::String& path);
    void browseWorkspaceFolder(std::function<void(juce::String)> completion);
    juce::String listRomFiles();
    void pickRomFile(std::function<void(juce::String)> completion);
    juce::String convertRomToPackage(const juce::String& json);

private:
    InstrumentAudioProcessor& processor_;
};

} // namespace bdmcs48::plugin
