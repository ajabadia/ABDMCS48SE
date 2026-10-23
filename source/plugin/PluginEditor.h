// PluginEditor.h - Editor del instrumento sobre WebView.
//
// La UI vive en source/webui/ y se sirve desde recursos embebidos: no hay
// directorio externo ni servidor. C++ conserva la autoridad (audio, MIDI,
// estado y presets); la pagina solo envia intenciones y pinta lo que recibe.

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include "plugin/PluginProcessor.h"
#include "plugin/WebViewBridge.h"

namespace bdmcs48::plugin {

class InstrumentEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer {
public:
    explicit InstrumentEditor(InstrumentAudioProcessor& processor);
    ~InstrumentEditor() override = default;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

    // Despacho de las funciones nativas de la pagina. Siempre en el hilo de
    // mensajes; `done` se puede llamar desde cualquier hilo.
    void handleNative(const juce::String& name, const juce::Array<juce::var>& args,
                      juce::WebBrowserComponent::NativeFunctionCompletion done);

    static juce::StringArray nativeFunctionNames();

private:
    void timerCallback() override;
    static juce::WebBrowserComponent::Options makeOptions(InstrumentEditor& editor);

    InstrumentAudioProcessor& processor_;
    WebViewBridge bridge_;
    juce::WebBrowserComponent webView_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentEditor)
};

} // namespace bdmcs48::plugin
