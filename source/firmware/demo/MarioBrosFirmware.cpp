#include "firmware/demo/MarioBrosFirmware.h"

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

// --- Programa de efectos Mario Bros -----------------------------------------
//
// Genera efectos sonoros controlados por latches:
// - Lee P1 para controlar el patrón de sonido
// - Bit 7 = 1: modo efectos (coin/jump/stomp)
// - Bits 0-6: seleccionan tipo de efecto
//
// Código simplificado para generar tonos estilo Mario

constexpr std::uint8_t kProgram[] = {
    // Inicio
    0x09,              // IN A, P1
    0x53, 0x80,        // ANL A, #0x80
    0x96, 0x30,        // JNZ 0x030 (si bit 7 = 1, modo efectos)
    
    // Modo tono simple - similar a mono voice
    0x23, 0xc0,        // MOV A,#0xC0
    0x39,              // OUTL P1,A
    0xb8, 0xdc,        // MOV R0,#220
    0xe8, 0x25,        // DJNZ R0,0x025
    0x23, 0x40,        // MOV A,#0x40
    0x39,              // OUTL P1,A
    0xb8, 0xdc,        // MOV R0,#220
    0xe8, 0x2c,        // DJNZ R0,0x02C
    0x04, 0x00,        // JMP 0x000
    
    // Modo efectos (0x030)
    0x09,              // IN A, P1
    0x53, 0x7f,        // ANL A, #0x7F
    0xb9,              // MOV R1,A (guardar tipo de efecto)
    
    // Efecto coin: subida rápida de tono
    0xf9,              // MOV A,R1
    0x03, 0x00,        // ADD A,#0x00 (comparar con 0)
    0xc6, 0x50,        // JZ 0x050 (coin)
    
    // Por defecto, tono simple
    0x04, 0x20,        // JMP 0x020
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
    json += "  \"id\": \"mcs48.mario-bros-sfx\",\n";
    json += "  \"name\": \"Mario Bros Sound Effects\",\n";
    json += "  \"version\": \"0.1.0\",\n";
    json += "  \"author\": \"ABDMCS48SE\",\n";
    json += "  \"description\": \"" +
            jsonEscape("Efectos sonoros estilo Mario Bros con control por latch. "
                       "Usa la matriz de comandos para generar coin, jump y otros efectos.") + "\",\n";
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
    json += "      \"id\": \"program-mario\",\n";
    json += "      \"file\": \"roms/mario-bros-sfx.rom\",\n";
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

const std::vector<MarioBrosImage>& marioBrosImages() {
    static const std::vector<MarioBrosImage> images = [] {
        std::vector<MarioBrosImage> result;
        result.push_back(MarioBrosImage{"program-mario", "program", 0, 0,
                                        std::vector<std::uint8_t>(kProgram,
                                                                  kProgram + sizeof(kProgram))});
        return result;
    }();
    return images;
}

std::string marioBrosDescription() {
    return "Efectos sonoros estilo Mario Bros con control por latch.\n"
           "\n"
           "Control por latch (P1):\n"
           "  Bit 7 = 1: modo efectos sonoros\n"
           "  Bits 0-6: tipo de efecto (0 = coin style)\n"
           "\n"
           "Usa la matriz de comandos con destino 'latch_write' para disparar efectos.\n"
           "\n"
           "Escribe el paquete en disco:\n"
           "  bdmcs48se mario <directorio>\n";
}

LoadedPackage marioBrosPackage() {
    const std::vector<MarioBrosImage>& images = marioBrosImages();

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    const std::string text = buildManifest(Sha256::hexOf(images[0].data));
    if (!FirmwarePackage::parseManifest(text, package.manifest, issues)) {
        return {};
    }

    for (const MarioBrosImage& image : images) {
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

bool writeMarioBrosPackage(const std::string& directory, std::string& error) {
    std::error_code code;
    const fs::path root = fs::absolute(fs::path(directory), code);
    if (code) {
        error = "no se pudo resolver la ruta '" + directory + "'";
        return false;
    }

    const std::vector<MarioBrosImage>& images = marioBrosImages();
    for (const MarioBrosImage& image : images) {
        const fs::path path = root / "roms" / ("mario-bros-sfx.rom");
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
