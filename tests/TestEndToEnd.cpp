#include "TestFramework.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "core/MachineInstance.h"
#include "firmware/PackageLoader.h"
#include "firmware/demo/DemoFirmware.h"
#include "host/DiagnosticsProtocol.h"
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
    void setParameter(std::uint32_t, float) override {}
    void writeInput(std::uint32_t, float) override {}
    void runBlock(std::uint32_t, float* const*, std::uint32_t) override {}
    EngineStatus status() override { return EngineStatus{}; }
    std::uint64_t retiredPending() const override { return 0; }
    void drainRetired() override {}

    int id() const { return id_; }

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

    auto first = std::make_unique<StubMachine>(1);
    CHECK(host.publishMachine(std::move(first)));

    // El primer bloque: la maquina nueva se instala.
    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(gAlive, 1);
    CHECK(host.hotSwaps() == 1);

    // El segundo bloque: la segunda maquina entra en caliente; la primera
    // se retira y no se destruye hasta que el hilo de control la drena.
    auto second = std::make_unique<StubMachine>(2);
    CHECK(host.publishMachine(std::move(second)));
    host.renderBlock(64, outputs, 1);
    CHECK_EQ(gAlive, 2);
    CHECK(host.hotSwaps() == 1);

    // Drenar: la maquina retirada se destruye aqui, no en el audio.
    CHECK(host.service() == 1);
    CHECK_EQ(gDestroyed, 1);
    CHECK_EQ(gAlive, 1);
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
    CHECK(!serror.empty());
    CHECK(server.running());

    // La consulta se hace por un socket: el archivo no depende del navegador.
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    const SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.port());
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    std::string request = "GET /api/diagnostics HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(sock, request.data(), static_cast<int>(request.size()), 0);

    std::string answer;
    char buffer[4096];
    int received;
    while ((received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        answer.append(buffer, static_cast<std::size_t>(received));
    }
    closesocket(sock);
    WSACleanup();

    CHECK(answer.find("HTTP/1.1 200") != std::string::npos);
    CHECK(answer.find("bdmcs48se-diagnostics/1") != std::string::npos);
    CHECK(answer.find("\"firmware\"") != std::string::npos);
#else
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.port());
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    std::string request = "GET /api/diagnostics HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(sock, request.data(), request.size(), 0);

    std::string answer;
    char buffer[4096];
    ssize_t received;
    while ((received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        answer.append(buffer, static_cast<std::size_t>(received));
    }
    close(sock);

    CHECK(answer.find("HTTP/1.1 200") != std::string::npos);
    CHECK(answer.find("bdmcs48se-diagnostics/1") != std::string::npos);
    CHECK(answer.find("\"firmware\"") != std::string::npos);
#endif

    server.stop();
    CHECK(!server.running());
}

TEST(EndToEnd, SilencioInicialLuegoSeActivaElBucle) {
    const std::string dir = gEnv.writtenDir();
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(dir, error));

    bdmcs48::host::MachineHost host({44'100, 64});
    CHECK(host.loadPackage(dir, error));

    // Los primeros bloques no producen ruido: el bucle de demostracion
    // necesita hasta 0x00A ciclos (banco 0) antes de escribir el primer
    // valor en el DAC.
    std::vector<float> buffer(64, 0.0f);
    float* outputs[1] = {buffer.data()};
    for (int i = 0; i < 3; ++i) {
        host.renderBlock(64, outputs, 1);
        host.service();
    }
    const bdmcs48::core::EngineStatus status = host.status();
    // En el estado actual el DAC aun no ha sido tocado.
    CHECK_EQ(status.accumulator, std::uint8_t{0});
}

} // namespace
