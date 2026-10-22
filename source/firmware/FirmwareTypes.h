// FirmwareTypes.h - Modelo del manifiesto de un paquete de firmware.
//
// Refleja el formato conceptual descrito en docs/FIRMWARE_PACKAGE.md. Aqui
// solo hay DATOS: el parseo y las reglas semanticas viven en
// FirmwarePackage.cpp, y la lectura de disco en PackageLoader.cpp.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bdmcs48::firmware {

struct CpuDescription {
    std::string family = "mcs48";
    std::string variant = "8049";
    std::uint32_t clockHz = 6'000'000;
};

struct ProgramMemoryDescription {
    std::uint32_t addressBits = 12;
    std::uint32_t bankSizeBytes = 2048;
    std::uint32_t bankCount = 2;
};

struct MachineDescription {
    std::string profile = "generic-mcs48";
    CpuDescription cpu;
    ProgramMemoryDescription programMemory;
};

struct ImageDescription {
    std::string id;
    std::string file;     // ruta RELATIVA dentro del paquete
    std::string region;   // "program" | "data"
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;  // 0 = el tamaño real del fichero
    std::string sha256;      // 64 caracteres hex
};

struct InputDescription {
    std::string id;
    std::string type;   // "latch" | "trigger" | "gate"
    std::uint32_t index = 0;
    std::uint32_t bits = 8;
};

struct OutputDescription {
    std::string id;
    std::string type;   // "mono_stream" | "latch_out"
};

struct ParameterDescription {
    std::string id;
    std::string type;   // "float" | "int" | "bool"
    double minimum = 0.0;
    double maximum = 1.0;
    double initial = 0.0;
    bool hostAutomatable = false;
};

struct FirmwareManifest {
    std::string format;
    std::uint32_t formatVersion = 0;
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string license;
    std::string provenance;

    MachineDescription machine;
    std::vector<ImageDescription> images;
    std::vector<InputDescription> inputs;
    std::vector<OutputDescription> outputs;
    std::vector<ParameterDescription> parameters;
};

struct ValidationIssue {
    std::string field;
    std::string message;
};

} // namespace bdmcs48::firmware
