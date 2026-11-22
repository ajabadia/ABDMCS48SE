#include "plugin/PluginEditor.h"

#include <cstring>
#include <optional>
#include <vector>

#include "webui/WebUIResources.h"

#if JUCE_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace bdmcs48::plugin {
namespace {

// Carpeta de datos de WebView2 aislada por build. Reusar una sola carpeta (o
// %TEMP%) es lo que deja la ventana en blanco cuando una instancia anterior
// murio a lo bruto: su msedgewebview2.exe sigue vivo, bloquea EBWebView y el
// renderer nuevo no arranca. El sello se toma del propio ejecutable, asi que
// cambia con cada build sin exigir una definicion de CMake por compilacion.
// Carpeta de datos de WebView2 aislada por proceso (PID) conforme a ADR-003.
// Reusar una sola carpeta (o %TEMP%) es lo que deja la ventana en blanco cuando
// una instancia anterior murio a lo bruto o dos instancias corren a la vez:
// msedgewebview2.exe bloquea EBWebView y el renderer nuevo no arranca.
// Respeta la variable de entorno WEBVIEW2_USER_DATA_FOLDER si esta definida.
juce::File webView2UserDataFolder() {
#if JUCE_WINDOWS
    const auto pid = static_cast<juce::int64>(::GetCurrentProcessId());
#else
    const auto pid = static_cast<juce::int64>(juce::Time::getMillisecondCounter());
#endif

    const juce::String envFolder = juce::SystemStats::getEnvironmentVariable("WEBVIEW2_USER_DATA_FOLDER", "").trim();
    const juce::File base = envFolder.isNotEmpty()
                                ? juce::File(envFolder)
                                : juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                      .getChildFile("ABDMCS48SE")
                                      .getChildFile("WebView2");

    const juce::File current = base.getChildFile(juce::String(pid));

    if (base.isDirectory()) {
        for (const auto& entry :
             juce::RangedDirectoryIterator(base, false, "*", juce::File::findDirectories)) {
            if (entry.getFile() != current) {
                entry.getFile().deleteRecursively();
            }
        }
    }

    current.createDirectory();
    return current;
}

// En desarrollo la pagina se sirve desde WebUI/dist en disco, que es lo que
// regenera `node Scripts/build_webui.js` sin recompilar el plugin. En un build
// instalado ese directorio no existe y manda el bundle embebido.
std::optional<juce::File> diskFallbackFor(const std::string& path) {
    const auto projectDir = juce::File(__FILE__).getParentDirectory().getParentDirectory().getParentDirectory();
    const auto file = projectDir.getChildFile("WebUI").getChildFile("dist").getChildFile(
        juce::String(path).replace("\\", "/"));
    if (file.existsAsFile()) {
        return file;
    }
    return std::nullopt;
}

juce::WebBrowserComponent::Options withResourceProvider(
    juce::WebBrowserComponent::Options options) {
    using Resource = juce::WebBrowserComponent::Resource;

    auto provider = [](const juce::String& path) -> std::optional<Resource> {
        bdmcs48::webui::Resource embedded;
        if (bdmcs48::webui::resourceFor(path.toStdString(), embedded) && embedded.data != nullptr) {
            std::vector<std::byte> bytes(embedded.size);
            if (embedded.size > 0) {
                std::memcpy(bytes.data(), embedded.data, embedded.size);
            }
            return Resource{std::move(bytes), juce::String(embedded.mimeType)};
        }

        if (auto file = diskFallbackFor(path.toStdString())) {
            juce::MemoryBlock block;
            if (file->loadFileAsData(block)) {
                std::vector<std::byte> bytes(static_cast<std::size_t>(block.getSize()));
                if (block.getSize() > 0) {
                    std::memcpy(bytes.data(), block.getData(), static_cast<std::size_t>(block.getSize()));
                }
                return Resource{std::move(bytes), juce::String(bdmcs48::webui::mimeTypeFor(
                                                       path.toStdString()))};
            }
        }

        // Sin recurso: 404 real. Devolver index.html para cualquier ruta
        // escondia un asset mal empaquetado detras de una pagina a medias.
        return std::nullopt;
    };

    return options.withResourceProvider(std::move(provider));
}

} // namespace

juce::StringArray InstrumentEditor::nativeFunctionNames() {
    return {"getState", "listPresets", "listFirmwares", "setDiagnosticTone", "noteOn", "noteOff", "allNotesOff",
            "setParameter", "beginParameterGesture", "endParameterGesture", "setRow", "resetMachine", "savePreset",
            "loadPreset", "loadFirmware", "getWorkspaceFolder", "setWorkspaceFolder", "browseWorkspaceFolder",
            "listRomFiles", "pickRomFile", "convertRomToPackage", "loadFactoryPreset", "loadRawRomData",
            "exportSysExDump", "importSysExDump"};
}

juce::WebBrowserComponent::Options InstrumentEditor::makeOptions(InstrumentEditor& editor) {
    using Options = juce::WebBrowserComponent::Options;

    auto options = Options{}
                       .withBackend(Options::Backend::webview2)
                       .withWinWebView2Options(
                           Options::WinWebView2{}
                               .withUserDataFolder(webView2UserDataFolder())
                               // Sin color de fondo la ventana parpadea en
                               // blanco hasta que la pagina pinta.
                               .withBackgroundColour(juce::Colour(0xff101317))
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
    } else if (name == "listFirmwares") {
        done(bridge_.firmwaresJson());
    } else if (name == "noteOn") {
        done(bridge_.noteOn(intArg(0), floatArg(1)));
    } else if (name == "noteOff") {
        done(bridge_.noteOff(intArg(0)));
    } else if (name == "allNotesOff") {
        done(bridge_.allNotesOff());
    } else if (name == "setParameter") {
        done(bridge_.setParameter(stringArg(0), floatArg(1)));
    } else if (name == "beginParameterGesture") {
        done(bridge_.beginParameterGesture(stringArg(0)));
    } else if (name == "endParameterGesture") {
        done(bridge_.endParameterGesture(stringArg(0)));
    } else if (name == "setRow") {
        done(bridge_.applyRowJson(intArg(0), stringArg(1)));
    } else if (name == "resetMachine") {
        done(bridge_.resetMachine());
    } else if (name == "savePreset") {
        done(bridge_.savePreset(stringArg(0)));
    } else if (name == "loadPreset") {
        done(bridge_.loadPreset(stringArg(0)));
    } else if (name == "loadFirmware") {
        done(bridge_.loadFirmware(stringArg(0)));
    } else if (name == "setDiagnosticTone") {
        done(bridge_.setDiagnosticTone(args.size() > 0 ? static_cast<bool>(args[0]) : false));
    } else if (name == "getWorkspaceFolder") {
        done(bridge_.getWorkspaceFolder());
    } else if (name == "setWorkspaceFolder") {
        done(bridge_.setWorkspaceFolder(stringArg(0)));
    } else if (name == "browseWorkspaceFolder") {
        bridge_.browseWorkspaceFolder([done = std::move(done)](juce::String result) mutable {
            done(std::move(result));
        });
    } else if (name == "listRomFiles") {
        done(bridge_.listRomFiles());
    } else if (name == "pickRomFile") {
        bridge_.pickRomFile([done = std::move(done)](juce::String result) mutable {
            done(std::move(result));
        });
    } else if (name == "convertRomToPackage") {
        done(bridge_.convertRomToPackage(stringArg(0)));
    } else if (name == "loadFactoryPreset") {
        done(bridge_.loadFactoryPreset(intArg(0)));
    } else if (name == "loadRawRomData") {
        done(bridge_.loadRawRomData(stringArg(0), stringArg(1)));
    } else if (name == "exportSysExDump") {
        done(bridge_.exportSysExDump());
    } else if (name == "importSysExDump") {
        done(bridge_.importSysExDump(stringArg(0)));
    } else {
        done("funcion desconocida: " + name);
    }
}

juce::AudioProcessorEditor* createInstrumentEditor(InstrumentAudioProcessor& processor) {
    return new InstrumentEditor(processor);
}

} // namespace bdmcs48::plugin
