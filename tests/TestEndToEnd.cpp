#include "TestFramework.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/MachineInstance.h"
#include "firmware/PackageLoader.h"
#include "firmware/demo/DemoFirmware.h"
#include "host/DiagnosticsProtocol.h"
#include "host/HttpClient.h"
#include "host/HttpServer.h"
#include "host/MachineHost.h"
#include "machines/MachineFactory.h"

namespace fs = std::filesystem;

using bdmcs48::core::MachineExchange;
using bdmcs48::core::MachineInstance;
using bdmcs48::firmware::LoadedPackage;
using bdmcs48::host::HttpServer;
using bdmcs48::host::MachineHost;

namespace {

int gAlive = 0;
int gDestroyed = 0;

class StubMachine : public MachineInstance {
public:
    explicit StubMachine(int id) : id_(id) { ++gAlive; }
    ~StubMachine() override {
        --gAlive;
        ++gDestroyed;
    }

    std::string profileId() const override { return "stub"; }
    void reset() override {}
    void setParameter(std::uint32_t id, float value) override {
        parameters.emplace_back(id, value);
    }
    void writeInput(std::uint32_t id, float value) override {
        inputs.emplace_back(id, value);
    }
    void runBlock(std::uint32_t, float* const*, std::uint32_t) override {}
    bdmcs48::core::EngineStatus status() const override { return bdmcs48::core::EngineStatus{}; }

    int id() const { return id_; }

    // Lo que la maquina ha recibido del hilo de audio.
    std::vector<std::pair<std::uint32_t, float>> parameters;
    std::vector<std::pair<std::uint32_t, float>> inputs;

private:
    int id_ = 0;
};

struct TestEnvironment {
    TestEnvironment() {
        const fs::path dir = fs::temp_directory_path() / "bdmcs48se-e2e";
        std::error_code code;
        fs::remove_all(dir, code);
        fs::create_directories(dir, code);
        writtenDir_ = dir.string();
    }

    ~TestEnvironment() {
        std::error_code code;
        fs::remove_all(writtenDir_, code);
    }

    const std::string& writtenDir() const { return writtenDir_; }

private:
    std::string writtenDir_;
};

TestEnvironment gEnv;

} // namespace

TEST(EndToEnd, ElPaqueteDeDemostracionSeCarga) {
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));
    CHECK(error.empty());

    LoadedPackage package;
    std::vector<bdmcs48::firmware::ValidationIssue> issues;
    CHECK(bdmcs48::firmware::PackageLoader::load(dir, package, issues));
    CHECK_EQ(issues.size(), std::size_t{0});
    CHECK_EQ(package.manifest.id, std::string("mcs48.demo.tone"));
    CHECK_EQ(package.images.size(), std::size_t{2});
}

TEST(EndToEnd, ElCpuVisitaElBanco1EnTiempoReal) {
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));
    CHECK(error.empty());

    // Renderizamos frame a frame y comprobamos la trayectoria del PC.
    const std::uint32_t total = 44'100;
    const std::uint32_t block = 1;
    std::vector<std::uint16_t> pcs;
    std::vector<std::uint8_t> dacValues;
    while (pcs.size() < total) {
        std::vector<float> buffer(block, 0.0f);
        float* outputs[1] = {buffer.data()};
        host.renderBlock(block, outputs, 1);
        host.service();
        const bdmcs48::core::EngineStatus status = host.status();
        pcs.push_back(status.programCounter);
        dacValues.push_back(status.accumulator);
    }
    host.service();

    bool inBank1 = false;
    bool inBank0 = false;
    for (std::uint16_t pc : pcs) {
        if (pc >= 0x800 && pc <= 0x806) {
            inBank1 = true;
        }
        if (pc < 0x800) {
            inBank0 = true;
        }
    }
    CHECK(inBank0);
    CHECK(inBank1);
}

TEST(EndToEnd, ElAudioNoEsConstante) {
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));

    const std::uint32_t total = 44'100;
    std::vector<float> samples;
    samples.reserve(total);
    while (samples.size() < total) {
        const std::uint32_t block = total - samples.size() > 64 ? 64 : static_cast<std::uint32_t>(total - samples.size());
        std::vector<float> buffer(block, 0.0f);
        float* outputs[1] = {buffer.data()};
        host.renderBlock(block, outputs, 1);
        host.service();
        for (float s : buffer) {
            samples.push_back(s);
        }
    }
    host.service();

    bool changed = false;
    float first = samples[0];
    for (float s : samples) {
        if (s != first) {
            changed = true;
            break;
        }
    }
    CHECK(changed);
}

TEST(EndToEnd, HotSwapEnElLimiteDeBloque) {
    gAlive = 0;
    gDestroyed = 0;
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));

    // loadPackage deja la maquina del paquete pendiente: se instala en el
    // primer limite de bloque.
    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1);
    CHECK(host.hotSwaps() == 1);
    host.service(); // no hay nada retirado todavia

    auto first = std::make_unique<StubMachine>(1);
    CHECK(host.publishMachine(std::move(first)));

    // El segundo bloque: la maquina nueva sustituye a la del paquete.
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(gAlive, 1);
    CHECK(host.hotSwaps() == 2);
    // La maquina del paquete ya no suena y la destruye el hilo de control.
    CHECK(host.service() == 1);

    // El tercer bloque: la segunda maquina entra en caliente; la primera
    // se retira y no se destruye hasta que el hilo de control la drena.
    auto second = std::make_unique<StubMachine>(2);
    CHECK(host.publishMachine(std::move(second)));
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(gAlive, 2);
    CHECK_EQ(gDestroyed, 0);
    CHECK(host.hotSwaps() == 3);

    // Drenar: la maquina retirada se destruye aqui, no en el audio.
    CHECK(host.service() == 1);
    CHECK_EQ(gDestroyed, 1);
    CHECK_EQ(gAlive, 1);
}

TEST(EndToEnd, LosComandosLleganALaMaquinaActiva) {
    gAlive = 0;
    gDestroyed = 0;

    bdmcs48::host::MachineHost host({44'100, 64});
    auto machine = std::make_unique<StubMachine>(1);
    StubMachine* raw = machine.get();
    CHECK(host.publishMachine(std::move(machine)));

    // Los ids >= 0x1000 son parametros del perfil; el resto, entradas. El
    // valor viaja como entero y el host lo normaliza a 0..1.
    CHECK(host.postCommand(0x1000u + 3u, 32767));
    CHECK(host.postCommand(0u, 16384));

    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1); // instala y aplica los comandos

    CHECK_EQ(raw->parameters.size(), std::size_t{1});
    CHECK_EQ(raw->parameters[0].first, std::uint32_t{3});
    CHECK_NEAR(raw->parameters[0].second, 1.0, 1e-6);

    CHECK_EQ(raw->inputs.size(), std::size_t{1});
    CHECK_EQ(raw->inputs[0].first, std::uint32_t{0});
    CHECK_NEAR(raw->inputs[0].second, 0.5, 1e-3);

    CHECK_EQ(host.droppedCommands(), std::uint64_t{0});
}

TEST(EndToEnd, LaColaDeComandosLlenaNoBloqueaElAudio) {
    gAlive = 0;
    gDestroyed = 0;

    bdmcs48::host::MachineHost host({44'100, 64});
    auto machine = std::make_unique<StubMachine>(1);
    StubMachine* raw = machine.get();
    CHECK(host.publishMachine(std::move(machine)));

    const std::size_t capacity = host.commands().capacity();
    CHECK(capacity > 0);

    std::size_t accepted = 0;
    for (std::size_t i = 0; i < capacity + 16; ++i) {
        if (host.postCommand(0u, static_cast<std::int32_t>(i))) {
            ++accepted;
        }
    }
    CHECK_EQ(accepted, capacity);
    CHECK_EQ(host.droppedCommands(), std::uint64_t{16});

    // El hilo de audio no se bloquea ni pierde muestras: procesa el bloque y
    // aplica solo lo que cabia.
    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(raw->inputs.size(), capacity);
    CHECK_EQ(host.framesRendered(), std::uint64_t{64});
}

TEST(EndToEnd, RechazaUnSegundoCambioMientrasHayUnoPendiente) {
    gAlive = 0;
    gDestroyed = 0;
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));

    // El paquete ya deja una maquina pendiente: el host solo admite un
    // cambio por limite de bloque y debe rechazar el segundo sin tocar el
    // audio ni la maquina que ya suena.
    auto extra = std::make_unique<StubMachine>(7);
    MachineInstance* extraRaw = extra.get();
    CHECK(!host.publishMachine(std::move(extra)));
    CHECK_EQ(host.rejectedSwaps(), std::uint64_t{1});
    CHECK_EQ(host.retiredPending(), std::size_t{0});
    CHECK_EQ(gAlive, 0); // la rechazada se destruye en el hilo de control

    // El primer bloque sigue instalando la maquina del paquete.
    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(host.hotSwaps(), std::uint64_t{1});
    CHECK_EQ(gAlive, 0);

    // La rechazada no se instala nunca.
    (void)extraRaw;
    CHECK_EQ(host.retiredPending(), std::size_t{0});
}

TEST(EndToEnd, ServidorHttpSirveElDiagnostico) {
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));

    HttpServer server;
    std::string serror;
    CHECK(server.start(0, "", [&host](const bdmcs48::host::HttpRequest& request,
                                       bdmcs48::host::HttpResponse& response) {
        if (request.path == "/api/diagnostics") {
            bdmcs48::core::MachineSnapshot snapshot;
            host.snapshot(snapshot);
            response.status = 200;
            response.contentType = "application/json; charset=utf-8";
            response.body = bdmcs48::host::buildDiagnosticsJson(
                host.context(), snapshot, host.status());
            return true;
        }
        return false;
    }, serror));
    CHECK(serror.empty());
    CHECK(server.running());

    // La consulta se hace por un socket real: el documento no depende del
    // navegador y el cliente (que aisla winsock/POSIX) vive en host/.
    std::string answer;
    std::string clientError;
    CHECK(bdmcs48::host::httpGet(server.port(), "/api/diagnostics", answer, clientError));
    CHECK(clientError.empty());

    CHECK(answer.find("HTTP/1.1 200") != std::string::npos);
    CHECK(answer.find("bdmcs48se-diagnostics/1") != std::string::npos);
    CHECK(answer.find("\"firmware\"") != std::string::npos);

    server.stop();
    CHECK(!server.running());
}

TEST(EndToEnd, ElDacSuenaYaEnElPrimerBloque) {
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));

    // Tres bloques bastan para que el programa recorra el retardo del banco
    // 1 y escriba los dos niveles del DAC (0x5A y 0xC0).
    std::vector<float> samples;
    for (int i = 0; i < 3; ++i) {
        std::vector<float> buffer(64, 0.0f);
        float* outputs[1] = {buffer.data()};
        host.renderBlock(64, outputs, 1);
        host.service();
        samples.insert(samples.end(), buffer.begin(), buffer.end());
    }

    // La conversion del DAC es la misma que hace el perfil: asi la
    // comparacion no depende de redondear a mano.
    const auto level = [](std::uint8_t value) {
        return (static_cast<float>(value) - 127.5f) / 127.5f;
    };

    // La CPU ejecuta el primer OUTL P1 (0x5A) antes de la primera muestra:
    // el primer bloque ya no es silencio.
    CHECK_EQ(samples.front(), level(0x5a));

    bool sawLow = false;
    bool sawHigh = false;
    for (float sample : samples) {
        if (sample == level(0x5a)) {
            sawLow = true;
        }
        if (sample == level(0xc0)) {
            sawHigh = true;
        }
    }
    CHECK(sawLow);
    CHECK(sawHigh);
}
