#include "firmware/demo/DemoFirmware.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "firmware/FirmwarePackage.h"
#include "firmware/Sha256.h"

namespace bdmcs48::firmware::demo {
namespace {

namespace fs = std::filesystem;

// --- Programa de demostracion ---------------------------------------------
//
// Banco 0 (direcciones 0x000-0x0FF):
//
//   000: 23 5A     MOV A, #0x5A
//   002: 39        OUTL P1, A        ; DAC: nivel bajo
//   003: 23 00     MOV A, #0x00
//   005: 3A        OUTL P2, A        ; registro de control
//   006: B8 20     MOV R0, #0x20     ; contador del retardo
//   008: F5        SEL MB1           ; el flip-flop A11 pasa a 1
//   009: 04 00     JMP 0x000         ; -> 0x800 (banco 1)
//
//   020: 23 C0     MOV A, #0xC0
//   022: 39        OUTL P1, A        ; DAC: nivel alto
//   023: 23 80     MOV A, #0x80
//   025: 3A        OUTL P2, A
//   026: 04 00     JMP 0x000         -> 0x000 (banco 0)
//
// Banco 1 (direcciones 0x800-0x8FF):
//
//   800: E8 00     DJNZ R0, 0x800    ; espera: NO cambia de pagina
//   802: E5        SEL MB0           ; el flip-flop A11 vuelve a 0
//   803: 04 20     JMP 0x020         -> 0x020 (banco 0)
//
// El retardo usa DJNZ, un salto condicional: aunque el flip-flop de banco
// valga 1, el salto se queda en la pagina actual. Esa es exactamente la
// diferencia entre el modelo correcto y un "banco" entendido como traduccion
// plana de direcciones, y por eso el firmware de demostracion la usa.

constexpr std::uint8_t kBank0Code[] = {
    // 0x000
    0x23, 0x5A,        // MOV A, #0x5A
    0x39,               // OUTL P1, A
    0x23, 0x00,        // MOV A, #0x00
    0x3a,               // OUTL P2, A
    0xb8, 0x20,        // MOV R0, #0x20
    0xf5,               // SEL MB1
    0x04, 0x00,        // JMP 0x000  -> 0x800
    // 0x020
    0x23, 0xc0,        // MOV A, #0xC0
    0x39,               // OUTL P1, A
    0x23, 0x80,        // MOV A, #0x80
    0x3a,               // OUTL P2, A
    0x04, 0x00,        // JMP 0x000  -> 0x000
};

constexpr std::uint8_t kBank1Code[] = {
    // 0x800
    0xe8, 0x00,        // DJNZ R0, 0x800
    0xe5,               // SEL MB0
    0x04, 0x20,        // JMP 0x020  -> 0x020
};

constexpr std::uint32_t kBank0Offset = 0x00;
constexpr std::uint32_t kBank0Size = 0x0b; // hasta 0x00A
constexpr std::uint32_t kBank0SecondOffset = 0x20;
constexpr std::uint32_t kBank0SecondSize = 0x08; // hasta 0x027
constexpr std::uint32_t kBank1Offset = 0x00;
constexpr std::uint32_t kBank1Size = 0x06; // hasta 0x005

std::vector<std::uint8_t> makeBank0Image() {
    std::vector<std::uint8_t> data(kBank0SecondOffset + kBank0SecondSize, 0x00);
    for (std::size_t i = 0; i < kBank0Size; ++i) {
        data[kBank0Offset + i] = kBank0Code[i];
    }
    for (std::size_t i = 0; i < kBank0SecondSize; ++i) {
        data[kBank0SecondOffset + i] = kBank0Code[kBank0Size + i];
    }
    return data;
}

std::vector<std::uint8_t> makeBank1Image() {
    return std::vector<std::uint8_t>(kBank1Code, kBank1Code + kBank1Size);
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

std::string buildManifest(const std::string& bank0Hash, const std::string& bank1Hash) {
    std::string json;
    json += "{\n";
    json += "  \"format\": \"bdmcs48se-firmware\",\n";
    json += "  \"formatVersion\": 1,\n";
    json += "  \"id\": \"mcs48.demo.tone\",\n";
    json += "  \"name\": \"MCS-48 Demo Tone\",\n";
    json += "  \"version\": \"0.1.0\",\n";
    json += "  \"author\": \"ABDMCS48SE\",\n";
    json += "  \"description\": \"" +
            jsonEscape("Tono de demostracion: el programa escribe en el DAC (P1) y usa "
                       "SEL MB1 + JMP para visitar el banco 1 del programa.") + "\",\n";
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
    json += "      \"id\": \"program-bank0\",\n";
    json += "      \"file\": \"roms/program-bank0.rom\",\n";
    json += "      \"region\": \"program\",\n";
    json += "      \"bank\": 0,\n";
    json += "      \"offset\": 0,\n";
    json += "      \"size\": " + std::to_string(kBank0SecondOffset + kBank0SecondSize) + ",\n";
    json += "      \"sha256\": \"" + bank0Hash + "\"\n";
    json += "    },\n";
    json += "    {\n";
    json += "      \"id\": \"program-bank1\",\n";
    json += "      \"file\": \"roms/program-bank1.rom\",\n";
    json += "      \"region\": \"program\",\n";
    json += "      \"bank\": 1,\n";
    json += "      \"offset\": 0,\n";
    json += "      \"size\": " + std::to_string(kBank1Size) + ",\n";
    json += "      \"sha256\": \"" + bank1Hash + "\"\n";
    json += "    }\n";
    json += "  ],\n";
    json += "  \"inputs\": [\n";
    json += "    { \"id\": \"latch0\", \"type\": \"latch\", \"index\": 0, \"bits\": 8 },\n";
    json += "    { \"id\": \"latch1\", \"type\": \"latch\", \"index\": 1, \"bits\": 8 }\n";
    json += "  ],\n";
    json += "  \"outputs\": [\n";
    json += "    { \"id\": \"audio\", \"type\": \"mono_stream\" },\n";
    json += "    { \"id\": \"control\", \"type\": \"latch_out\" }\n";
    json += "  ],\n";
    json += "  \"parameters\": [\n";
    json += "    { \"id\": \"engine_clock\", \"type\": \"float\", \"minimum\": 0.0625, \"maximum\": 8.0, ";
    json += "\"initial\": 1.0, \"hostAutomatable\": true }\n";
    json += "  ]\n";
    json += "}\n";
    return json;
}

} // namespace

const std::vector<DemoImage>& demoImages() {
    static const std::vector<DemoImage> images = [] {
        std::vector<DemoImage> result;
        result.push_back(DemoImage{"program-bank0", "program", 0, kBank0Offset, makeBank0Image()});
        result.push_back(DemoImage{"program-bank1", "program", 1, kBank1Offset, makeBank1Image()});
        return result;
    }();
    return images;
}

LoadedPackage demoPackage() {
    const std::vector<DemoImage>& images = demoImages();

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    const std::string text = buildManifest(Sha256::hexOf(images[0].data), Sha256::hexOf(images[1].data));
    if (!FirmwarePackage::parseManifest(text, package.manifest, issues)) {
        return {};
    }

    for (const DemoImage& image : images) {
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

bool writeDemoPackage(const std::string& directory, std::string& error) {
    std::error_code code;
    const fs::path root = fs::absolute(fs::path(directory), code);
    if (code) {
        error = "no se pudo resolver la ruta '" + directory + "'";
        return false;
    }
    fs::create_directories(root / "roms", code);
    if (code) {
        error = "no se pudo crear '" + (root / "roms").string() + "'";
        return false;
    }

    const std::vector<DemoImage>& images = demoImages();
    for (const DemoImage& image : images) {
        const fs::path path = root / "roms" / (image.id + ".rom");
        if (!writeBinaryFile(path, image.data, error)) {
            return false;
        }
    }

    const std::string bank0Hash = Sha256::hexOf(images[0].data);
    const std::string bank1Hash = Sha256::hexOf(images[1].data);
    const std::string manifest = buildManifest(bank0Hash, bank1Hash);
    if (!writeTextFile(root / "firmware.json", manifest, error)) {
        return false;
    }
    return true;
}

std::string demoDescription() {
    return "Tono de demostracion escrito a mano para ABDMCS48SE.\n"
           "\n"
           "El programa escribe en el DAC (P1) y alterna entre el banco 0 y el banco 1\n"
           "del programa usando SEL MB1 + JMP. El retardo del banco 1 usa DJNZ, un\n"
           "salto condicional que NO cambia de pagina: es el caso que distingue un\n"
           "modelo de bancos correcto de una traduccion plana de direcciones.\n"
           "\n"
           "Escribe un paquete en disco con el manifiesto y las imagenes ya hasheadas:\n"
           "  bdmcs48se demo <directorio>\n";
}

} // namespace bdmcs48::firmware::demo
