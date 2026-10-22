// main.cpp - CLI de referencia del host headless.
//
// Subcomandos:
//   demo <dir>                     escribe el paquete de demostracion
//   inspect <paquete>              muestra el manifiesto en texto
//   diagnose <paquete> [opciones]  renderiza y vuelca el diagnostico JSON
//   render <paquete> [opciones]    renderiza audio a un fichero WAV
//   serve <paquete> [opciones]     sirve el cliente WebView y el diagnostico
//
// Todo lo que hace la CLI se hace FUERA del hilo de audio: la CLI no tiene
// hilo de audio, el bloque se renderiza en el hilo principal. El servidor
// HTTP si tiene hilo propio, y por eso el reparto de responsabilidades del
// host (audio / control) es el mismo que en el plugin.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core/MachineInstance.h"
#include "firmware/FirmwarePackage.h"
#include "firmware/PackageLoader.h"
#include "firmware/demo/DemoFirmware.h"
#include "host/DiagnosticsProtocol.h"
#include "host/HttpServer.h"
#include "host/MachineHost.h"

namespace {

constexpr std::uint32_t kDefaultSampleRate = 44'100;
constexpr std::uint32_t kDefaultBlockSize = 64;

void printUsage() {
    std::cout <<
        "ABDMCS48SE - host MCS-48 headless\n"
        "\n"
        "Uso:\n"
        "  bdmcs48se demo <directorio>\n"
        "  bdmcs48se inspect <paquete>\n"
        "  bdmcs48se diagnose <paquete> [--frames N] [--sample-rate HZ]\n"
        "  bdmcs48se render <paquete> --salida <fichero.wav> [--segundos N]\n"
        "  bdmcs48se serve <paquete> [--puerto N] [--webview <dir>] [--segundos N]\n"
        "  bdmcs48se help\n"
        "\n"
        "El paquete es un directorio con 'firmware.json' y las imagenes que este\n"
        "declara. Para empezar:  bdmcs48se demo build/demo\n";
}

struct Options {
    std::string package;
    std::string output;
    std::string webview;
    std::uint32_t frames = 4'410;   // ~0,1 s a 44,1 kHz
    std::uint32_t seconds = 2;
    std::uint32_t sampleRate = kDefaultSampleRate;
    std::uint16_t port = 8'080;
    bool help = false;
};

bool parseOptions(const std::vector<std::string>& args, Options& options, std::string& error) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& argument = args[i];
        const auto value = [&](std::string& out) -> bool {
            if (i + 1 >= args.size()) {
                return false;
            }
            out = args[++i];
            return true;
        };
        const auto number = [&](std::uint32_t& out) -> bool {
            std::string text;
            if (!value(text)) {
                return false;
            }
            try {
                out = static_cast<std::uint32_t>(std::stoul(text));
            } catch (...) {
                return false;
            }
            return true;
        };

        if (argument == "--frames") {
            if (!number(options.frames)) {
                error = "valor invalido para " + argument;
                return false;
            }
        } else if (argument == "--seconds" || argument == "--segundos") {
            if (!number(options.seconds)) {
                error = "valor invalido para " + argument;
                return false;
            }
        } else if (argument == "--sample-rate" || argument == "--frecuencia") {
            if (!number(options.sampleRate)) {
                error = "valor invalido para " + argument;
                return false;
            }
        } else if (argument == "--port" || argument == "--puerto") {
            std::uint32_t port = 0;
            if (!number(port) || port > 65'535) {
                error = "valor invalido para " + argument;
                return false;
            }
            options.port = static_cast<std::uint16_t>(port);
        } else if (argument == "--out" || argument == "--output" || argument == "--salida") {
            if (!value(options.output)) {
                error = "falta el valor de " + argument;
                return false;
            }
        } else if (argument == "--webview") {
            if (!value(options.webview)) {
                error = "falta el valor de " + argument;
                return false;
            }
        } else if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (!argument.empty() && argument[0] == '-') {
            error = "opcion desconocida: " + argument;
            return false;
        } else if (options.package.empty()) {
            options.package = argument;
        } else {
            error = "argumento sobrante: " + argument;
            return false;
        }
    }
    return true;
}

int commandDemo(const std::vector<std::string>& args) {
    Options options;
    std::string error;
    if (!parseOptions(args, options, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 2;
    }
    if (options.package.empty()) {
        std::cerr << "[ERROR] falta el directorio de destino\n";
        return 2;
    }
    if (!bdmcs48::firmware::demo::writeDemoPackage(options.package, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 1;
    }
    std::cout << "[OK] paquete de demostracion escrito en " << options.package << "\n";
    std::cout << bdmcs48::firmware::demo::demoDescription();
    return 0;
}

int commandInspect(const std::vector<std::string>& args) {
    Options options;
    std::string error;
    if (!parseOptions(args, options, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 2;
    }
    if (options.package.empty()) {
        std::cerr << "[ERROR] falta el paquete\n";
        return 2;
    }

    bdmcs48::firmware::LoadedPackage package;
    std::vector<bdmcs48::firmware::ValidationIssue> issues;
    if (!bdmcs48::firmware::PackageLoader::load(options.package, package, issues)) {
        std::cerr << "[ERROR] el paquete no es valido: "
                  << bdmcs48::firmware::FirmwarePackage::formatIssues(issues) << "\n";
        return 1;
    }

    const auto& manifest = package.manifest;
    std::cout << "id:       " << manifest.id << "\n";
    std::cout << "nombre:   " << manifest.name << "\n";
    std::cout << "version:  " << manifest.version << "\n";
    std::cout << "autor:    " << manifest.author << "\n";
    std::cout << "licencia: " << manifest.license << "\n";
    std::cout << "perfil:   " << manifest.machine.profile << "\n";
    std::cout << "cpu:      " << manifest.machine.cpu.variant << " @ " << manifest.machine.cpu.clockHz
              << " Hz\n";
    std::cout << "programa: " << manifest.machine.programMemory.addressBits << " bits, "
              << manifest.machine.programMemory.bankCount << " bancos de "
              << manifest.machine.programMemory.bankSizeBytes << " bytes\n";
    std::cout << "imagenes: " << package.images.size() << "\n";
    for (const auto& image : package.images) {
        std::cout << "  - " << image.id << " [" << image.region << "] banco " << image.bank << " offset 0x"
                  << std::hex << image.offset << std::dec << ", " << image.data.size() << " bytes\n";
    }
    std::cout << "entradas: " << manifest.inputs.size() << ", salidas: " << manifest.outputs.size()
              << ", parametros: " << manifest.parameters.size() << "\n";
    return 0;
}

int commandDiagnose(const std::vector<std::string>& args) {
    Options options;
    std::string error;
    if (!parseOptions(args, options, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 2;
    }
    if (options.package.empty()) {
        std::cerr << "[ERROR] falta el paquete\n";
        return 2;
    }

    bdmcs48::host::MachineHost host({options.sampleRate, kDefaultBlockSize});
    if (!host.loadPackage(options.package, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 1;
    }

    std::vector<float> buffer(kDefaultBlockSize);
    float* outputs[1] = {buffer.data()};
    std::uint32_t remaining = options.frames;
    while (remaining > 0) {
        const std::uint32_t block = remaining < kDefaultBlockSize ? remaining : kDefaultBlockSize;
        host.renderBlock(block, outputs, 1);
        remaining -= block;
    }
    host.service();

    bdmcs48::core::MachineSnapshot snapshot;
    host.snapshot(snapshot);
    std::cout << bdmcs48::host::buildDiagnosticsJson(host.context(), snapshot, host.status());
    return 0;
}

bool writeWav(const std::string& path, const std::vector<float>& samples, std::uint32_t sampleRate,
              std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "no se pudo abrir '" + path + "' para escritura";
        return false;
    }

    const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 2);
    const std::uint32_t riffSize = 36 + dataBytes;

    const auto putU32 = [&stream](std::uint32_t value) {
        const unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xff),
                                       static_cast<unsigned char>((value >> 8) & 0xff),
                                       static_cast<unsigned char>((value >> 16) & 0xff),
                                       static_cast<unsigned char>((value >> 24) & 0xff)};
        stream.write(reinterpret_cast<const char*>(bytes), 4);
    };
    const auto putU16 = [&stream](std::uint16_t value) {
        const unsigned char bytes[2] = {static_cast<unsigned char>(value & 0xff),
                                       static_cast<unsigned char>((value >> 8) & 0xff)};
        stream.write(reinterpret_cast<const char*>(bytes), 2);
    };

    stream.write("RIFF", 4);
    putU32(riffSize);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    putU32(16);           // tamano del bloque fmt
    putU16(1);            // PCM
    putU16(1);            // mono
    putU32(sampleRate);
    putU32(sampleRate * 2); // bytes por segundo
    putU16(2);            // alineacion
    putU16(16);           // bits por muestra
    stream.write("data", 4);
    putU32(dataBytes);

    for (const float sample : samples) {
        float clamped = std::clamp(sample, -1.0f, 1.0f);
        const auto value = static_cast<std::int16_t>(std::lround(clamped * 32'767.0f));
        putU16(static_cast<std::uint16_t>(value));
    }
    stream.flush();
    if (!stream) {
        error = "fallo al escribir '" + path + "'";
        return false;
    }
    return true;
}

int commandRender(const std::vector<std::string>& args) {
    Options options;
    std::string error;
    if (!parseOptions(args, options, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 2;
    }
    if (options.package.empty()) {
        std::cerr << "[ERROR] falta el paquete\n";
        return 2;
    }
    if (options.output.empty()) {
        std::cerr << "[ERROR] falta el fichero de salida (--salida)\n";
        return 2;
    }

    bdmcs48::host::MachineHost host({options.sampleRate, kDefaultBlockSize});
    if (!host.loadPackage(options.package, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 1;
    }

    const std::size_t totalFrames = static_cast<std::size_t>(options.seconds) * options.sampleRate;
    std::vector<float> samples(totalFrames, 0.0f);

    std::size_t rendered = 0;
    while (rendered < totalFrames) {
        const std::uint32_t block = static_cast<std::uint32_t>(
            std::min<std::size_t>(kDefaultBlockSize, totalFrames - rendered));
        float* outputs[1] = {samples.data() + rendered};
        host.renderBlock(block, outputs, 1);
        rendered += block;
    }
    host.service();

    if (!writeWav(options.output, samples, options.sampleRate, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 1;
    }
    std::cout << "[OK] " << options.output << " (" << samples.size() << " muestras a "
              << options.sampleRate << " Hz)\n";
    return 0;
}

int commandServe(const std::vector<std::string>& args) {
    Options options;
    std::string error;
    if (!parseOptions(args, options, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 2;
    }
    if (options.package.empty()) {
        std::cerr << "[ERROR] falta el paquete\n";
        return 2;
    }

    bdmcs48::host::MachineHost host({options.sampleRate, kDefaultBlockSize});
    if (!host.loadPackage(options.package, error)) {
        std::cerr << "[ERROR] " << error << "\n";
        return 1;
    }

    std::string webviewRoot = options.webview;
    if (webviewRoot.empty()) {
        // Junto al ejecutable: el CMake copia el cliente a <exe>/webview.
        webviewRoot = "webview";
    }

    bdmcs48::host::HttpServer server;
    const bool started = server.start(
        options.port, webviewRoot,
        [&host](const bdmcs48::host::HttpRequest& request, bdmcs48::host::HttpResponse& response) {
            if (request.path == "/api/diagnostics") {
                bdmcs48::core::MachineSnapshot snapshot;
                host.snapshot(snapshot);
                response.status = 200;
                response.contentType = "application/json; charset=utf-8";
                response.body = bdmcs48::host::buildDiagnosticsJson(host.context(), snapshot, host.status());
                return true;
            }
            if (request.path == "/api/logs") {
                response.status = 200;
                response.contentType = "application/json; charset=utf-8";
                response.body = bdmcs48::host::buildLogJson(host.logBus(), 64);
                return true;
            }
            return false;
        },
        error);
    if (!started) {
        std::cerr << "[ERROR] " << error << "\n";
        return 1;
    }

    std::cout << "[OK] servidor en http://127.0.0.1:" << server.port() << "/\n";
    std::cout << "     diagnostico: http://127.0.0.1:" << server.port() << "/api/diagnostics\n";
    std::cout << "     registros:    http://127.0.0.1:" << server.port() << "/api/logs\n";
    std::cout << "     Ctrl+C para salir.\n";

    // El "hilo de audio" de la CLI: renderiza bloques en tiempo real y
    // deja respirar al proceso. El servidor HTTP corre en su propio hilo.
    const auto blockDuration = std::chrono::microseconds(
        (static_cast<std::uint64_t>(kDefaultBlockSize) * 1'000'000ull) / options.sampleRate);
    std::vector<float> buffer(kDefaultBlockSize, 0.0f);
    float* outputs[1] = {buffer.data()};
    while (true) {
        const auto start = std::chrono::steady_clock::now();
        host.renderBlock(kDefaultBlockSize, outputs, 1);
        host.service();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < blockDuration) {
            std::this_thread::sleep_for(blockDuration - elapsed);
        }
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty()) {
        printUsage();
        return 0;
    }

    const std::string command = args[0];
    const std::vector<std::string> rest(args.begin() + 1, args.end());

    if (command == "help" || command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }
    if (command == "demo") {
        return commandDemo(rest);
    }
    if (command == "inspect") {
        return commandInspect(rest);
    }
    if (command == "diagnose") {
        return commandDiagnose(rest);
    }
    if (command == "render") {
        return commandRender(rest);
    }
    if (command == "serve") {
        return commandServe(rest);
    }

    std::cerr << "[ERROR] comando desconocido: " << command << "\n\n";
    printUsage();
    return 2;
}
