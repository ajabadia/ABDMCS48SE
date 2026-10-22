#include "host/DiagnosticsProtocol.h"

#include <cstdio>

namespace bdmcs48::host {
namespace {

std::string number(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6g", value);
    return std::string(buffer);
}

std::string quoted(const std::string& text) {
    return "\"" + jsonEscape(text) + "\"";
}

} // namespace

std::string jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
                out += buffer;
            } else {
                out.push_back(c);
            }
        }
    }
    return out;
}

std::string buildDiagnosticsJson(const DiagnosticsContext& context, const core::MachineSnapshot& snapshot,
                                  const core::EngineStatus& status) {
    std::string json;
    json += "{\n";
    json += "  \"protocol\": \"bdmcs48se-diagnostics/1\",\n";
    json += "  \"firmware\": {\n";
    json += "    \"id\": " + quoted(context.firmwareId) + ",\n";
    json += "    \"name\": " + quoted(context.firmwareName) + ",\n";
    json += "    \"version\": " + quoted(context.firmwareVersion) + "\n";
    json += "  },\n";
    json += "  \"engine\": {\n";
    json += "    \"profile\": " + quoted(context.profileId) + ",\n";
    json += "    \"sampleRateHz\": " + std::to_string(context.sampleRateHz) + ",\n";
    json += "    \"blockSize\": " + std::to_string(context.blockSize) + "\n";
    json += "  },\n";
    json += "  \"cpu\": {\n";
    json += "    \"programCounter\": " + std::to_string(snapshot.programCounter) + ",\n";
    json += "    \"accumulator\": " + std::to_string(snapshot.accumulator) + ",\n";
    json += "    \"psw\": " + std::to_string(snapshot.psw) + ",\n";
    json += "    \"bank\": " + std::to_string(snapshot.bank) + ",\n";
    json += "    \"cycles\": " + std::to_string(status.cpuCycles) + ",\n";
    json += "    \"faulted\": " + std::string(status.faulted != 0 ? "true" : "false") + ",\n";
    json += "    \"fault\": " + quoted(status.faultMessage) + "\n";
    json += "  },\n";
    json += "  \"audio\": {\n";
    json += "    \"framesRendered\": " + std::to_string(context.framesRendered) + ",\n";
    json += "    \"outputLevel\": " + number(snapshot.outputLevel) + "\n";
    json += "  },\n";
    json += "  \"lifecycle\": {\n";
    json += "    \"hotSwaps\": " + std::to_string(context.hotSwaps) + ",\n";
    json += "    \"rejectedSwaps\": " + std::to_string(context.rejectedSwaps) + ",\n";
    json += "    \"retiredPending\": " + std::to_string(context.retiredPending) + "\n";
    json += "  },\n";
    json += "  \"streams\": {\n";
    json += "    \"droppedCommands\": " + std::to_string(context.droppedCommands) + ",\n";
    json += "    \"droppedLogs\": " + std::to_string(context.droppedLogs) + "\n";
    json += "  },\n";
    json += "  \"message\": " + quoted(snapshot.message) + "\n";
    json += "}\n";
    return json;
}

std::string buildLogJson(const core::LogBus& bus, std::size_t maxRecords) {
    std::string json = "[";
    std::size_t emitted = 0;
    core::LogRecord record;
    core::LogBus* nb = const_cast<core::LogBus*>(&bus);
    while (emitted < maxRecords && nb->pop(record)) {
        if (emitted != 0) {
            json += ",";
        }
        json += "\n  {";
        json += "\"t\": " + std::to_string(record.timestampUs);
        json += ", \"id\": " + std::to_string(record.id);
        json += ", \"value\": " + std::to_string(record.value);
        json += ", \"text\": " + quoted(std::string(record.text));
        json += "}";
        ++emitted;
    }
    if (emitted != 0) {
        json += "\n";
    }
    json += "]\n";
    return json;
}

} // namespace bdmcs48::host
