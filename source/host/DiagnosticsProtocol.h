// DiagnosticsProtocol.h - Protocolo JSON de diagnostico.
//
// Un unico formato, usado por el volcado de la CLI y por el endpoint HTTP
// que consume el cliente WebView. Que sea el mismo evita que la UI y la
// linea de comandos obtengan datos distintos sobre la misma maquina.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/Diagnostics.h"
#include "core/MachineInstance.h"

namespace bdmcs48::host {

struct DiagnosticsContext {
    std::string firmwareId;
    std::string firmwareName;
    std::string firmwareVersion;
    std::string profileId;
    std::uint32_t sampleRateHz = 44'100;
    std::uint32_t blockSize = 64;
    std::uint64_t framesRendered = 0;
    std::uint64_t hotSwaps = 0;
    std::uint64_t rejectedSwaps = 0;
    std::uint64_t droppedCommands = 0;
    std::uint64_t droppedLogs = 0;
    std::uint64_t retiredPending = 0;
};

// Documento JSON con el estado completo de la maquina.
std::string buildDiagnosticsJson(const DiagnosticsContext& context, const core::MachineSnapshot& snapshot,
                                  const core::EngineStatus& status);

// Array JSON con los registros pendientes del bus de log.
std::string buildLogJson(const core::LogBus& bus, std::size_t maxRecords);

// Escapa una cadena para un documento JSON (comillas, barras y controles).
std::string jsonEscape(const std::string& text);

} // namespace bdmcs48::host
