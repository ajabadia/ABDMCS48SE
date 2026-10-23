#include "plugin/PluginEditor.h"

#include <cstring>
#include <optional>
#include <vector>

#include "webui/WebUIResources.h"

namespace bdmcs48::plugin {
namespace {

juce::WebBrowserComponent::Options withResourceProvider(
    juce::WebBrowserComponent::Options options) {
    using Resource = juce::WebBrowserComponent::Resource;

    auto provider = [](const juce::String& path) -> std::optional<Resource> {
        bdmcs48::webui::Resource embedded;
        if (!bdmcs48::webui::resourceFor(path.toStdString(), embedded) ||
            embedded.data == nullptr) {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(embedded.size);
        if (embedded.size > 0) {
            std::memcpy(bytes.data(), embedded.data, embedded.size);
        }
        return Resource{std::move(bytes), juce::String(embedded.mimeType)};
    };

    return options.withResourceProvider(std::move(provider));
}

} // namespace

juce::StringArray InstrumentEditor::nativeFunctionNames() {
    return {"getState", "listPresets", "noteOn", "noteOff", "allNotesOff",
            "setParameter", "setRow", "resetMachine", "savePreset", "loadPreset"};
}

juce::WebBrowserComponent::Options InstrumentEditor::makeOptions(InstrumentEditor& editor) {
    using Options = juce::WebBrowserComponent::Options;

    auto options = Options{}
                       .withBackend(Options::Backend::webview2)
                       .withWinWebView2Options(
                           Options::WinWebView2{}.withUserDataFolder(
                               juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("ABDMCS48SE-webview"))
                               .withStatusBarDisabled())
                       .withNativeIntegrationEnabled()
                       .withKeepPageLoadedWhenBrowserIsHidden();

    options = withResourceProvider(std::move(options));

    for (const auto& name : nativeFunctionNames()) {
        options = options.withNativeFunction(
            juce::Identifier(name),
            [&editor, name](const juce::Array<juce::var>& args,
                            juce::WebBrowserComponent::NativeFunctionCompletion done) {
                editor.handleNative(name, args, std::move(done));
            });
    }

    return options;
}

InstrumentEditor::InstrumentEditor(InstrumentAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      processor_(processor),
      bridge_(processor),
      webView_(makeOptions(*this)) {
    addAndMakeVisible(webView_);
    setResizable(true, true);
    setResizeLimits(720, 520, 1600, 1200);
    setSize(980, 680);
    webView_.goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz(20);
}

void InstrumentEditor::paint(juce::Graphics& graphics) {
    graphics.fillAll(juce::Colour(0xff101317));
}

void InstrumentEditor::resized() {
    webView_.setBounds(getLocalBounds());
}

void InstrumentEditor::timerCallback() {
    // C++ -> pagina: un unico evento con el estado ya resuelto. La pagina no
    // inventa PC, ciclos ni nivel; los recibe de aqui.
    webView_.emitEventIfBrowserIsVisible("bdmcs48-state", processor_.uiStateJson());
}

void InstrumentEditor::handleNative(const juce::String& name,
                                    const juce::Array<juce::var>& args,
                                    juce::WebBrowserComponent::NativeFunctionCompletion done) {
    const auto intArg = [&args](int index) {
        return args.size() > index ? static_cast<int>(args[index]) : 0;
    };
    const auto floatArg = [&args](int index) {
        return args.size() > index ? static_cast<float>(args[index]) : 0.0f;
    };
    const auto stringArg = [&args](int index) {
        return args.size() > index ? args[index].toString() : juce::String();
    };

    if (name == "getState") {
        done(bridge_.stateJson());
    } else if (name == "listPresets") {
        done(bridge_.presetsJson());
    } else if (name == "noteOn") {
        done(bridge_.noteOn(intArg(0), floatArg(1)));
    } else if (name == "noteOff") {
        done(bridge_.noteOff(intArg(0)));
    } else if (name == "allNotesOff") {
        done(bridge_.allNotesOff());
    } else if (name == "setParameter") {
        done(bridge_.setParameter(stringArg(0), floatArg(1)));
    } else if (name == "setRow") {
        done(bridge_.applyRowJson(intArg(0), stringArg(1)));
    } else if (name == "resetMachine") {
        done(bridge_.resetMachine());
    } else if (name == "savePreset") {
        done(bridge_.savePreset(stringArg(0)));
    } else if (name == "loadPreset") {
        done(bridge_.loadPreset(stringArg(0)));
    } else {
        done("funcion desconocida: " + name);
    }
}

juce::AudioProcessorEditor* createInstrumentEditor(InstrumentAudioProcessor& processor) {
    return new InstrumentEditor(processor);
}

} // namespace bdmcs48::plugin
