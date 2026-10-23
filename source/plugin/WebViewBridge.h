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

#include <string>

namespace bdmcs48::plugin {

class InstrumentAudioProcessor;

class WebViewBridge {
public:
    explicit WebViewBridge(InstrumentAudioProcessor& processor);

    // Estado completo (parametros, matriz, firmware, motor y presets).
    juce::String stateJson();
    juce::String presetsJson();

    // Teclado virtual.
    juce::String noteOn(int note, float velocity);
    juce::String noteOff(int note);
    juce::String allNotesOff();

    // Parametros: id del APVTS y valor normalizado 0..1.
    juce::String setParameter(const juce::String& id, float normalizedValue);

    // Fila de la matriz: indice 0..7 y objeto JSON con los campos que cambian.
    juce::String applyRowJson(int index, const juce::String& json);

    // Transporte.
    juce::String resetMachine();

    // Presets del Standalone.
    juce::String savePreset(const juce::String& name);
    juce::String loadPreset(const juce::String& name);

private:
    InstrumentAudioProcessor& processor_;
};

} // namespace bdmcs48::plugin
