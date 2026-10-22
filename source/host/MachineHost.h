#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "core/CommandQueue.h"
#include "core/Diagnostics.h"
#include "core/MachineExchange.h"
#include "core/MachineInstance.h"
#include "firmware/FirmwareTypes.h"
#include "host/DiagnosticsProtocol.h"

namespace bdmcs48::host {

class MachineHost {
public:

    struct Config {
        std::uint32_t sampleRateHz;
        std::uint32_t blockSize;
    };

    explicit MachineHost(Config config = Config());
    ~MachineHost();

    MachineHost() = default;

    MachineHost(const MachineHost&) = delete;
    MachineHost& operator=(const MachineHost&) = delete;

    // Hilo de control: carga un paquete de disco, valida y publica la
    // maquina. Si algo falla, la maquina activa sigue sonando: el error se
    // devuelve y no se toca el estado de audio.
    bool loadPackage(const std::string& packageRoot, std::string& error);

    // Hilo de control: publica una maquina ya construida.
    bool publishMachine(std::unique_ptr<core::MachineInstance> machine);

    // Hilo de audio: renderiza un bloque de `frames` muestras.
    void renderBlock(std::uint32_t frames, float* const* outputs, std::uint32_t channels);

    // Hilo de control: destruye las maquinas retiradas por el intercambio.
    std::size_t service();

    // Hilo de control: lee la ultima instantanea publicada.
    bool snapshot(core::MachineSnapshot& out) const;

    // Hilo de control: estado de la maquina, reconstruido desde la
    // instantanea (no se toca la maquina activa desde aqui).
    core::EngineStatus status();
    core::EngineStatus status() const;

    // Hilo de control: contexto para el documento de diagnostico.
    DiagnosticsContext context() const;

    const firmware::FirmwareManifest& manifest() const { return manifest_; }
    const core::LogBus& logBus() const { return log_; }
    const core::CommandQueue& commands() const { return commands_; }

    // Encola un comando para el hilo de audio (no bloquea).
    bool postCommand(std::uint32_t id, std::int32_t value);

    // Hilo de audio: consume los comandos pendientes y los aplica.
    void applyPendingCommands();

    std::uint64_t framesRendered() const { return framesRendered_.load(std::memory_order_relaxed); }
    std::uint64_t hotSwaps() const { return exchange_.swapCount(); }
    std::uint64_t rejectedSwaps() const { return exchange_.rejectedCount(); }
    std::uint64_t droppedCommands() const { return commands_.dropped(); }
    std::uint64_t droppedLogs() const { return log_.dropped(); }
    std::size_t retiredPending() const { return exchange_.retiredPending(); }

    // Escribe un registro en el bus de log (hilo de control).
    void log(std::uint32_t id, std::int32_t value, const std::string& text);

private:
    Config config_;
    core::MachineExchange exchange_;
    core::CommandQueue commands_;
    core::SnapshotChannel snapshots_;
    core::LogBus log_;

    // Maquina activa. Solo la toca el hilo de audio.
    core::MachineInstance* active_ = nullptr;
    // Propiedad de la maquina inicial, hasta que el intercambio la retire.
    std::unique_ptr<core::MachineInstance> owned_;

    firmware::FirmwareManifest manifest_;
    std::string profileId_ = "generic-mcs48";

    std::atomic<std::uint64_t> framesRendered_{0};
    std::atomic<std::uint64_t> timestampUs_{0};
};

} // namespace bdmcs48::host
