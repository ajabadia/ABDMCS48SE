#include "firmware/demo/MonoVoiceFirmware.h"

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

// --- Programa de voz ------------------------------------------------------
//
//   000: 23 C0     MOV A,#0xC0
//   002: 39        OUTL P1,A        ; DAC alto
//   003: 14 0C     CALL 0x00C
//   005: 23 40     MOV A,#0x40
//   007: 39        OUTL P1,A        ; DAC bajo (simetrico respecto a 0x80)
//   008: 14 0C     CALL 0x00C
//   00A: 04 00     JMP 0x000
//
//   00C: B8 DC     MOV R0,#220
//   00E: E8 0E     DJNZ R0,0x00E    ; 2 ciclos por iteracion
//   010: 83        RET
//
// Cada semiciclo son 450 ciclos de maquina; el JMP final anade 2, asi que un
// periodo completo son 902 ciclos. A 6 MHz (15 oscilaciones por ciclo) son
// 400000 / 902 = 443,33 Hz.
//
// El firmware no lee entradas: la puerta y la afinacion son del host. Un
// programa de 16 bytes es facil de auditar, y cualquier cambio en el retardo
// obliga a recalibrar kMonoVoiceBaseFrequencyHz (los tests lo comprueban).

constexpr std::uint8_t kProgram[] = {
    0x23, 0xc0,        // MOV A,#0xC0
    0x39,              // OUTL P1,A
    0x14, 0x0c,        // CALL 0x00C
    0x23, 0x40,        // MOV A,#0x40
    0x39,              // OUTL P1,A
    0x14, 0x0c,        // CALL 0x00C
    0x04, 0x00,        // JMP 0x000
    0xb8, 0xdc,        // MOV R0,#220
    0xe8, 0x0e,        // DJNZ R0,0x00E
    0x83,              // RET
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
    json += "  \"id\": \"mcs48.mono-voice\",\n";
    json += "  \"name\": \"MCS-48 Mono Voice\",\n";
    json += "  \"version\": \"0.1.0\",\n";
    json += "  \"author\": \"ABDMCS48SE\",\n";
    json += "  \"description\": \"" +
            jsonEscape("Onda cuadrada simetrica generada por la CPU: el DAC alterna entre "
                       "0xC0 y 0x40 cada 450 ciclos de maquina. La afinacion la pone el reloj "
                       "de la CPU y la puerta la aplica el host.") + "\",\n";
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
    json += "      \"file\": \"roms/mono-voice.rom\",\n";
    json += "      \"region\": \"program\",\n";
    json += "      \"bank\": 0,\n";
    json += "      \"offset\": 0,\n";
    json += "      \"size\": " + std::to_string(sizeof(kProgram)) + ",\n";
    json += "      \"sha256\": \"" + programHash + "\"\n";
    json += "    }\n";
    json += "  ],\n";
    json += "  \"inputs\": [],\n";
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

const std::vector<MonoVoiceImage>& monoVoiceImages() {
    static const std::vector<MonoVoiceImage> images = [] {
        std::vector<MonoVoiceImage> result;
        result.push_back(MonoVoiceImage{"program-voice", "program", 0, 0,
                                        std::vector<std::uint8_t>(kProgram,
                                                                  kProgram + sizeof(kProgram))});
        return result;
    }();
    return images;
}

std::string monoVoiceDescription() {
    return "Voz monofonica de onda cuadrada escrita a mano para ABDMCS48SE.\n"
           "\n"
           "El DAC (P1) alterna entre 0xC0 y 0x40 cada 450 ciclos de maquina, de modo\n"
           "que el periodo son 902 ciclos. A 6 MHz suena a 443,33 Hz; el host afina\n"
           "cada nota con el parametro engine_clock (escala del reloj de la CPU) y\n"
           "aplica la puerta con una envolvente de amplitud.\n"
           "\n"
           "Escribe el paquete en disco:\n"
           "  bdmcs48se demo <directorio>            (paquete de demostracion)\n"
           "  el plugin usa esta voz sin escribir nada en disco\n";
}

LoadedPackage monoVoicePackage() {
    const std::vector<MonoVoiceImage>& images = monoVoiceImages();

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    const std::string text = buildManifest(Sha256::hexOf(images[0].data));
    if (!FirmwarePackage::parseManifest(text, package.manifest, issues)) {
        return {};
    }

    for (const MonoVoiceImage& image : images) {
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

bool writeMonoVoicePackage(const std::string& directory, std::string& error) {
    std::error_code code;
    const fs::path root = fs::absolute(fs::path(directory), code);
    if (code) {
        error = "no se pudo resolver la ruta '" + directory + "'";
        return false;
    }

    const std::vector<MonoVoiceImage>& images = monoVoiceImages();
    for (const MonoVoiceImage& image : images) {
        const fs::path path = root / "roms" / (image.id + ".rom");
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
