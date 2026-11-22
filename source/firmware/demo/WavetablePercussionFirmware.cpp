// WavetablePercussionFirmware.cpp - Implementacion de la ROM musical para MCS-48.
//
// Sintesis de percusion lo-fi (Kick, Snare, Hi-Hat) y oscilador wavetable
// multimodo mediante MOVP3 sobre tablas en ROM (pagina 3).
//
// Codigo original y limpio disenado para ABDMCS48SE.

#include "firmware/demo/WavetablePercussionFirmware.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

#include "firmware/FirmwarePackage.h"
#include "firmware/Sha256.h"

namespace bdmcs48::firmware::demo {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t kBankSize = 2048;

std::vector<std::uint8_t> buildProgramBank() {
    std::vector<std::uint8_t> b0(kBankSize, 0x00);

    // 0x0000: Reset vector
    b0[0x00] = 0x04;
    b0[0x01] = 0x10; // JMP 0x0010

    // 0x0003, 0x0007: Interrupt vectors
    b0[0x03] = 0x93; // RETR
    b0[0x07] = 0x93; // RETR

    // 0x0010: Secuencia de inicializacion
    const std::uint8_t codeInit[] = {
        0x27,             // CLR A
        0xD7,             // MOV PSW, A
        0xB8, 0x00,       // MOV R0, #0x00 (Acumulador de fase)
        0xB9, 0x02,       // MOV R1, #0x02 (Pitch step)
        0xBA, 0x5A,       // MOV R2, #0x5A (Semilla LFSR ruido)
        0xBB, 0x80,       // MOV R3, #0x80 (Nivel envolvente)
        0xBC, 0x00,       // MOV R4, #0x00 (Modo / control)
        0xBD, 0x10,       // MOV R5, #0x10 (Pitch contador Kick)
        0xBE, 0x00,       // MOV R6, #0x00 (Divisor / temporal)
        0xBF, 0x00,       // MOV R7, #0x00 (Selector tabla wavetable)
        0x23, 0x80,       // MOV A, #0x80
        0x62,             // MOV T, A
        0x55,             // STRT T
        0x04, 0x30        // JMP 0x0030
    };
    for (std::size_t i = 0; i < sizeof(codeInit); ++i) {
        b0[0x10 + i] = codeInit[i];
    }

    // 0x0030: Despachador de modos y lectura de controles
    const std::uint8_t codeDispatcher[] = {
        0x0A,             // IN A, P2
        0xAC,             // MOV R4, A
        0x53, 0xC0,       // ANL A, #0xC0
        0xC6, 0x60,       // JZ 0x0060 (Modo Wavetable)
        0xD3, 0x40,       // XRL A, #0x40
        0xC6, 0x90,       // JZ 0x0090 (Modo Kick)
        0xD3, 0xC0,       // XRL A, #0xC0 (0x40 ^ 0x80 = 0xC0)
        0xC6, 0xC0,       // JZ 0x00C0 (Modo Snare)
        0x04, 0xF0        // JMP 0x00F0 (Modo Hi-Hat)
    };
    for (std::size_t i = 0; i < sizeof(codeDispatcher); ++i) {
        b0[0x30 + i] = codeDispatcher[i];
    }

    // 0x0060: Motor Wavetable Oscillator (MOVP3 sobre Pagina 3)
    const std::uint8_t codeWavetable[] = {
        0x09,             // IN A, P1 (selector tabla bits 0-1)
        0x53, 0x03,       // ANL A, #0x03
        0x77,             // RR A
        0x77,             // RR A ((table & 3) << 6)
        0xAF,             // MOV R7, A
        0xFC,             // MOV A, R4 (tono desde P2)
        0x53, 0x1F,       // ANL A, #0x1F
        0x17,             // INC A (pitch step 1..32)
        0xA9,             // MOV R1, A
        0xF8,             // MOV A, R0 (fase actual)
        0x69,             // ADD A, R1
        0x53, 0x3F,       // ANL A, #0x3F (fase en 0..63)
        0xA8,             // MOV R0, A
        0x4F,             // ORL A, R7 (offset tabla | fase)
        0xE3,             // MOVP3 A, @A (lee muestra en Pagina 3 = 0x0300)
        0x39,             // OUTL P1, A (DAC)
        0x04, 0x30        // JMP 0x0030
    };
    for (std::size_t i = 0; i < sizeof(codeWavetable); ++i) {
        b0[0x60 + i] = codeWavetable[i];
    }

    // 0x0090: Motor Lo-Fi Kick Drum (Bombo con barrido de tono descendente)
    const std::uint8_t codeKick[] = {
        0x1E,             // INC R6
        0xFE,             // MOV A, R6
        0x6D,             // ADD A, R5
        0xAE,             // MOV R6, A
        0x53, 0x80,       // ANL A, #0x80
        0xC6, 0x9D,       // JZ 0x009D
        0x23, 0xD8,       // MOV A, #0xD8
        0x04, 0x9F,       // JMP 0x009F
        0x00,             // NOP / pad
        0x23, 0x28,       // MOV A, #0x28 (0x009D)
        0x39,             // OUTL P1, A   (0x009F)
        0x1D,             // INC R5
        0xFD,             // MOV A, R5
        0x53, 0x3F,       // ANL A, #0x3F
        0xAD,             // MOV R5, A
        0x04, 0x30        // JMP 0x0030
    };
    for (std::size_t i = 0; i < sizeof(codeKick); ++i) {
        b0[0x90 + i] = codeKick[i];
    }

    // 0x00C0: Motor Lo-Fi Snare Drum (Caja con LFSR de ruido + cuerpo tonal)
    const std::uint8_t codeSnare[] = {
        0xFA,             // MOV A, R2
        0x67,             // RRC A
        0xE6, 0xC6,       // JNC 0x00C6
        0xD3, 0xB4,       // XRL A, #0xB4 (polinomio LFSR)
        0xAA,             // MOV R2, A   (0x00C6)
        0x1E,             // INC R6
        0xDE,             // XRL A, R6
        0x39,             // OUTL P1, A
        0x04, 0x30        // JMP 0x0030
    };
    for (std::size_t i = 0; i < sizeof(codeSnare); ++i) {
        b0[0xC0 + i] = codeSnare[i];
    }

    // 0x00F0: Motor Lo-Fi Hi-Hat (Platillo con modulacion metalica)
    const std::uint8_t codeHiHat[] = {
        0xFA,             // MOV A, R2
        0xF7,             // RLC A
        0xE6, 0xF6,       // JNC 0x00F6
        0xD3, 0xD5,       // XRL A, #0xD5 (polinomio metalico)
        0xAA,             // MOV R2, A   (0x00F6)
        0xD3, 0xAA,       // XRL A, #0xAA (modulacion en anillo)
        0x39,             // OUTL P1, A
        0x04, 0x30        // JMP 0x0030
    };
    for (std::size_t i = 0; i < sizeof(codeHiHat); ++i) {
        b0[0xF0 + i] = codeHiHat[i];
    }

    // ------------------------------------------------------------------------
    // PAGINA 3 (0x0300..0x03FF): Tablas de onda accesibles por MOVP3 (256 B)
    // ------------------------------------------------------------------------
    // Tabla 0 (0x0300..0x033F): Onda Senoidal suave (64 muestras)
    for (int i = 0; i < 64; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * i / 64.0;
        const long val = std::lround(128.0 + 124.0 * std::sin(angle));
        b0[0x0300 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::clamp(val, 0L, 255L));
    }

    // Tabla 1 (0x0340..0x037F): Onda Triangular (64 muestras)
    for (int i = 0; i < 64; ++i) {
        const int val = (i < 32) ? (i * 8) : (255 - (i - 32) * 8);
        b0[0x0340 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::clamp(val, 0, 255));
    }

    // Tabla 2 (0x0380..0x03BF): Onda Diente de sierra ascendente (64 muestras)
    for (int i = 0; i < 64; ++i) {
        b0[0x0380 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::clamp(i * 4, 0, 255));
    }

    // Tabla 3 (0x03C0..0x03FF): Onda Stepped Pulse Chime (64 muestras)
    for (int i = 0; i < 64; ++i) {
        const int val = (i < 16) ? 230 : ((i < 32) ? 175 : ((i < 48) ? 80 : 25));
        b0[0x03C0 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(val);
    }

    return b0;
}

std::string buildManifest(const std::string& bank0Hash) {
    std::string json;
    json += "{\n";
    json += "  \"format\": \"bdmcs48se-firmware\",\n";
    json += "  \"formatVersion\": 1,\n";
    json += "  \"id\": \"mcs48.musical.wavetable_percussion\",\n";
    json += "  \"name\": \"MCS-48 Lo-Fi Percussion & Wavetable Synth\",\n";
    json += "  \"version\": \"1.0.0\",\n";
    json += "  \"author\": \"ABDMCS48SE Clean-Room Team\",\n";
    json += "  \"description\": \"Sintetizador musical para MCS-48 con percusion lo-fi (Kick, Snare, HiHat) y oscilador wavetable multimodo mediante MOVP3.\",\n";
    json += "  \"license\": \"CC0-1.0\",\n";
    json += "  \"provenance\": \"Microcodigo propio original escrito a mano.\",\n";
    json += "  \"machine\": {\n";
    json += "    \"profile\": \"generic-mcs48\",\n";
    json += "    \"cpu\": { \"family\": \"mcs48\", \"variant\": \"8049\", \"clockHz\": 6000000 },\n";
    json += "    \"programMemory\": { \"addressBits\": 12, \"bankSizeBytes\": 2048, \"bankCount\": 2 }\n";
    json += "  },\n";
    json += "  \"images\": [\n";
    json += "    {\n";
    json += "      \"id\": \"program-bank0\",\n";
    json += "      \"file\": \"roms/wavetable_percussion.rom\",\n";
    json += "      \"region\": \"program\",\n";
    json += "      \"bank\": 0,\n";
    json += "      \"offset\": 0,\n";
    json += "      \"size\": 2048,\n";
    json += "      \"sha256\": \"" + bank0Hash + "\"\n";
    json += "    }\n";
    json += "  ],\n";
    json += "  \"inputs\": [\n";
    json += "    { \"id\": \"latch0\", \"type\": \"latch\", \"index\": 0, \"bits\": 8 },\n";
    json += "    { \"id\": \"latch1\", \"type\": \"latch\", \"index\": 1, \"bits\": 8 }\n";
    json += "  ],\n";
    json += "  \"outputs\": [\n";
    json += "    { \"id\": \"audio\", \"type\": \"mono_stream\" }\n";
    json += "  ],\n";
    json += "  \"parameters\": [\n";
    json += "    { \"id\": \"engine_clock\", \"type\": \"float\", \"minimum\": 0.0625, \"maximum\": 8.0, \"initial\": 1.0, \"hostAutomatable\": true }\n";
    json += "  ]\n";
    json += "}\n";
    return json;
}

bool writeBinaryFile(const fs::path& path, const std::vector<std::uint8_t>& data, std::string& error) {
    std::error_code code;
    fs::create_directories(path.parent_path(), code);
    if (code) {
        error = "no se pudo crear directorio '" + path.parent_path().string() + "'";
        return false;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "no se pudo abrir '" + path.string() + "'";
        return false;
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
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
        error = "no se pudo abrir '" + path.string() + "'";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return true;
}

} // namespace

const std::vector<WavetablePercussionImage>& wavetablePercussionImages() {
    static const std::vector<WavetablePercussionImage> images = [] {
        std::vector<WavetablePercussionImage> result;
        const std::vector<std::uint8_t> b0 = buildProgramBank();
        result.push_back(WavetablePercussionImage{"program-bank0", "program", 0, 0, b0});
        return result;
    }();
    return images;
}

std::string wavetablePercussionDescription() {
    return "Sintetizador musical MCS-48 con percusion lo-fi y oscilador wavetable.\n"
           "\n"
           "Canales de control:\n"
           "  Puerto 2 (Latch 1): Modo de sonido (bits 7-6) y Pitch (bits 0-4)\n"
           "    00xxxxxx: Wavetable Synth (Senoidal, Triangular, Sierra, Chime)\n"
           "    01xxxxxx: Lo-Fi Kick Drum (Bombo descendente)\n"
           "    10xxxxxx: Lo-Fi Snare Drum (Caja con LFSR de ruido)\n"
           "    11xxxxxx: Lo-Fi Hi-Hat (Platillo metalico con modulacion de anillo)\n"
           "  Puerto 1 (Latch 0): Selector de tabla de onda (bits 0-1) para modo Wavetable\n"
           "\n"
           "Tablas de onda en ROM (Pagina 3, direccion 0x0300) leidas mediante instruccion nativa MOVP3.\n";
}

LoadedPackage wavetablePercussionPackage() {
    const std::vector<WavetablePercussionImage>& images = wavetablePercussionImages();

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    const std::string text = buildManifest(Sha256::hexOf(images[0].data));
    if (!FirmwarePackage::parseManifest(text, package.manifest, issues)) {
        return {};
    }

    LoadedImage img;
    img.id = images[0].id;
    img.region = images[0].region;
    img.bank = images[0].bank;
    img.offset = images[0].offset;
    img.data = images[0].data;
    package.images.push_back(std::move(img));

    return package;
}

bool writeWavetablePercussionPackage(const std::string& directory, std::string& error) {
    const std::vector<WavetablePercussionImage>& images = wavetablePercussionImages();
    const fs::path root(directory);
    const fs::path roms = root / "roms";

    for (const auto& image : images) {
        const fs::path path = roms / (image.id == "program-bank0" ? "wavetable_percussion.rom" : (image.id + ".rom"));
        if (!writeBinaryFile(path, image.data, error)) {
            return false;
        }
    }

    const std::string manifest = buildManifest(Sha256::hexOf(images[0].data));
    return writeTextFile(root / "firmware.json", manifest, error);
}

} // namespace bdmcs48::firmware::demo
