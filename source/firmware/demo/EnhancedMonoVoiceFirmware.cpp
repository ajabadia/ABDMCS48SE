#include "firmware/demo/EnhancedMonoVoiceFirmware.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#include "firmware/FirmwarePackage.h"
#include "firmware/Sha256.h"

namespace bdmcs48::firmware::demo {
namespace {

namespace fs = std::filesystem;

// --- Programa de voz mejorado -----------------------------------------------
//
// Genera ondas mas variadas controladas por latches:
// - Lee P1 (latch[0]) para controlar parametros de forma de onda
// - Genera onda cuadrada con ciclo de trabajo variable
// - Puede crear patrones mas complejos
//
// Codigo:
//   000: 09        IN A, P1         ; leer latch para control
//   001: 53 80     ANL A, #0x80     ; mask bit 7 para seleccionar modo
//   003: C6 20     JZ 0x020         ; si bit 7 = 0, modo cuadrado simple
//   005: 23 C0     MOV A,#0xC0      ; modo alternativo
//   007: 39        OUTL P1,A
//   008: 09        IN A, P1
//   009: 53 7F     ANL A, #0x7F     ; obtener valor para retardo
//   00B: F8        MOV R0,A
//   00C: 18        INC R0           ; asegurar retardo minimo
//   00D: E8 0D     DJNZ R0,0x00D    ; retardo variable
//   00F: 23 40     MOV A,#0x40
//   011: 39        OUTL P1,A
//   012: 09        IN A, P1
//   013: 53 7F     ANL A, #0x7F
//   015: F8        MOV R0,A
//   016: 18        INC R0
//   017: E8 17     DJNZ R0,0x017
//   019: 04 00     JMP 0x000
//   020: 23 C0     MOV A,#0xC0
//   022: 39        OUTL P1,A
//   023: B8 DC     MOV R0,#220     ; retardo fijo modo cuadrado
//   025: E8 25     DJNZ R0,0x025
//   027: 23 40     MOV A,#0x40
//   029: 39        OUTL P1,A
//   02A: B8 DC     MOV R0,#220
//   02C: E8 2C     DJNZ R0,0x02C
//   02E: 04 00     JMP 0x000

constexpr std::uint8_t kProgram[] = {
    0x09,              // IN A, P1
    0x53, 0x80,        // ANL A, #0x80
    0xc6, 0x20,        // JZ 0x020
    0x23, 0xc0,        // MOV A,#0xC0
    0x39,              // OUTL P1,A
    0x09,              // IN A, P1
    0x53, 0x7f,        // ANL A, #0x7F
    0xf8,              // MOV R0,A
    0x18,              // INC R0
    0xe8, 0x0d,        // DJNZ R0,0x00D
    0x23, 0x40,        // MOV A,#0x40
    0x39,              // OUTL P1,A
    0x09,              // IN A, P1
    0x53, 0x7f,        // ANL A, #0x7F
    0xf8,              // MOV R0,A
    0x18,              // INC R0
    0xe8, 0x17,        // DJNZ R0,0x017
    0x04, 0x00,        // JMP 0x000
    0x00, 0x00, 0x00, 0x00, 0x00, // Pad to 0x020
    // 0x020
    0x23, 0xc0,        // MOV A,#0xC0
    0x39,              // OUTL P1,A
    0xb8, 0xdc,        // MOV R0,#220
    0xe8, 0x25,        // DJNZ R0,0x025
    0x23, 0x40,        // MOV A,#0x40
    0x39,              // OUTL P1,A
    0xb8, 0xdc,        // MOV R0,#220
    0xe8, 0x2c,        // DJNZ R0,0x02C
    0x04, 0x00,        // JMP 0x000
};

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

std::string buildManifest(const std::string& programHash) {
    std::string json;
    json += "{\n";
    json += "  \"format\": \"bdmcs48se-firmware\",\n";
    json += "  \"formatVersion\": 1,\n";
    json += "  \"id\": \"mcs48.enhanced-mono-voice\",\n";
    json += "  \"name\": \"MCS-48 Enhanced Mono Voice\",\n";
    json += "  \"version\": \"0.1.0\",\n";
    json += "  \"author\": \"ABDMCS48SE\",\n";
    json += "  \"description\": \"" +
            jsonEscape("Onda cuadrada variable con control por latch. Usa el bit 7 para "
                       "seleccionar modo y los bits inferiores para controlar el ciclo "
                       "de trabajo. Mayor paleta sonora mediante la matriz de comandos.") + "\",\n";
    json += "  \"license\": \"CC0-1.0\",\n";
    json += "  \"provenance\": \"" +
            jsonEscape("Programa original escrito a mano para ABDMCS48SE. No contiene codigo "
                       "de terceros ni partes de ROMs comerciales.") + "\",\n";
    json += "  \"machine\": {\n";
    json += "    \"profile\": \"generic-mcs48\",\n";
    json += "    \"cpu\": { \"family\": \"mcs48\", \"variant\": \"8049\", \"clockHz\": 6000000 },\n";
    json += "    \"programMemory\": { \"addressBits\": 12, \"bankSizeBytes\": 2048, \"bankCount\": 2 }\n";
    json += "  },\n";
    json += "  \"images\": [\n";
    json += "    {\n";
    json += "      \"id\": \"program-voice\",\n";
    json += "      \"file\": \"roms/enhanced-mono-voice.rom\",\n";
    json += "      \"region\": \"program\",\n";
    json += "      \"bank\": 0,\n";
    json += "      \"offset\": 0,\n";
    json += "      \"size\": " + std::to_string(sizeof(kProgram)) + ",\n";
    json += "      \"sha256\": \"" + programHash + "\"\n";
    json += "    }\n";
    json += "  ],\n";
    json += "  \"inputs\": [\n";
    json += "    { \"id\": \"latch0\", \"type\": \"latch\", \"index\": 0, \"bits\": 8 }\n";
    json += "  ],\n";
    json += "  \"outputs\": [\n";
    json += "    { \"id\": \"audio\", \"type\": \"mono_stream\" }\n";
    json += "  ],\n";
    json += "  \"parameters\": [\n";
    json += "    { \"id\": \"engine_clock\", \"type\": \"float\", \"minimum\": 0.0625, \"maximum\": 8.0, ";
    json += "\"initial\": 1.0, \"hostAutomatable\": true }\n";
    json += "  ]\n";
    json += "}\n";
    return json;
}

bool writeBinaryFile(const fs::path& path, const std::vector<std::uint8_t>& data, std::string& error) {
    std::error_code code;
    fs::create_directories(path.parent_path(), code);
    if (code) {
        error = "no se pudo crear el directorio '" + path.parent_path().string() + "'";
        return false;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "no se pudo abrir '" + path.string() + "' para escritura";
        return false;
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
    }
    stream.flush();
    if (!stream) {
        error = "fallo al escribir '" + path.string() + "'";
        return false;
    }
    return true;
}

bool writeTextFile(const fs::path& path, const std::string& text, std::string& error) {
    std::error_code code;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), code);
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "no se pudo abrir '" + path.string() + "' para escritura";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream) {
        error = "fallo al escribir '" + path.string() + "'";
        return false;
    }
    return true;
}

} // namespace

const std::vector<EnhancedMonoVoiceImage>& enhancedMonoVoiceImages() {
    static const std::vector<EnhancedMonoVoiceImage> images = [] {
        std::vector<EnhancedMonoVoiceImage> result;
        result.push_back(EnhancedMonoVoiceImage{"program-voice", "program", 0, 0,
                                        std::vector<std::uint8_t>(kProgram,
                                                                  kProgram + sizeof(kProgram))});
        return result;
    }();
    return images;
}

std::string enhancedMonoVoiceDescription() {
    return "Voz monofonica mejorada con control por latch para mayor paleta sonora.\n"
           "\n"
           "Control por latch (P1):\n"
           "  Bit 7 = 1: modo variable con ciclo de trabajo controlado por bits 0-6\n"
           "  Bit 7 = 0: modo cuadrado simple (50%)\n"
           "\n"
           "Usa la matriz de comandos con destino 'latch_write' para controlar la forma\n"
           "de onda en tiempo real durante la interpretacion.\n"
           "\n"
           "Escribe el paquete en disco:\n"
           "  bdmcs48se enhanced <directorio>\n";
}

LoadedPackage enhancedMonoVoicePackage() {
    const std::vector<EnhancedMonoVoiceImage>& images = enhancedMonoVoiceImages();

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    const std::string text = buildManifest(Sha256::hexOf(images[0].data));
    if (!FirmwarePackage::parseManifest(text, package.manifest, issues)) {
        return {};
    }

    for (const EnhancedMonoVoiceImage& image : images) {
        LoadedImage loaded;
        loaded.id = image.id;
        loaded.region = image.region;
        loaded.bank = image.bank;
        loaded.offset = image.offset;
        loaded.data = image.data;
        package.images.push_back(std::move(loaded));
    }
    return package;
}

bool writeEnhancedMonoVoicePackage(const std::string& directory, std::string& error) {
    std::error_code code;
    const fs::path root = fs::absolute(fs::path(directory), code);
    if (code) {
        error = "no se pudo resolver la ruta '" + directory + "'";
        return false;
    }

    const std::vector<EnhancedMonoVoiceImage>& images = enhancedMonoVoiceImages();
    for (const EnhancedMonoVoiceImage& image : images) {
        const fs::path path = root / "roms" / ("enhanced-" + image.id + ".rom");
        if (!writeBinaryFile(path, image.data, error)) {
            return false;
        }
    }

    const std::string manifest = buildManifest(Sha256::hexOf(images[0].data));
    if (!writeTextFile(root / "firmware.json", manifest, error)) {
        return false;
    }
    return true;
}

} // namespace bdmcs48::firmware::demo
