// DiagnosticFirmware.cpp - Implementacion de la ROM de diagnostico.
// Conforme a la especificacion normativa docs/TEST_ROM_SPEC.md.

#include "firmware/demo/DiagnosticFirmware.h"

#include <cstdio>
#include <string>
#include <vector>

#include "firmware/FirmwarePackage.h"
#include "firmware/Sha256.h"

namespace bdmcs48::firmware::demo {
namespace {

constexpr std::size_t kBankSize = 2048;

std::vector<std::uint8_t> buildBank0() {
    std::vector<std::uint8_t> b0(kBankSize, 0x00);
    // 0x0000: JMP 0x0010
    b0[0x00] = 0x04;
    b0[0x01] = 0x10;
    // 0x0003: RETR
    b0[0x03] = 0x93;
    // 0x0007: RETR
    b0[0x07] = 0x93;

    // 0x0010: init sequence (CLR A, MOV PSW,A, MOV R0,#0x7F, MOV @R0,A, DJNZ R0,0x14...)
    const std::uint8_t code10[] = {
        0x27,       // CLR A
        0xD7,       // MOV PSW, A
        0xB8, 0x7F, // MOV R0, #0x7F
        0xA0,       // MOV @R0, A
        0xE8, 0x14, // DJNZ R0, 0x0014
        0x23, 0x80, // MOV A, #0x80
        0x62,       // MOV T, A
        0x55,       // STRT T
        0xF5,       // SEL MB1
        0x04, 0x20  // JMP 0x0820
    };
    for (std::size_t i = 0; i < sizeof(code10); ++i) {
        b0[0x10 + i] = code10[i];
    }

    // 0x0040: bank0 reentry (comprueba R7 == 0xA5)
    const std::uint8_t code40[] = {
        0xFF,       // MOV A, R7
        0xD3, 0xA5, // XRL A, #0xA5
        0x96, 0x60, // JNZ 0x0060 (fault_halt)
        0x27,       // CLR A
        0x04, 0x50  // JMP 0x0050 (audio_loop)
    };
    for (std::size_t i = 0; i < sizeof(code40); ++i) {
        b0[0x40 + i] = code40[i];
    }

    // 0x0050: audio loop (generador de rampa determinista en P1)
    const std::uint8_t code50[] = {
        0x39,       // OUTL P1, A
        0x03, 0x10, // ADD A, #0x10
        0x04, 0x50  // JMP 0x0050
    };
    for (std::size_t i = 0; i < sizeof(code50); ++i) {
        b0[0x50 + i] = code50[i];
    }

    // 0x0060: fault halt
    const std::uint8_t code60[] = {
        0x23, 0xEE, // MOV A, #0xEE
        0x3A,       // OUTL P2, A
        0x04, 0x60  // JMP 0x0060
    };
    for (std::size_t i = 0; i < sizeof(code60); ++i) {
        b0[0x60 + i] = code60[i];
    }

    return b0;
}

std::vector<std::uint8_t> buildBank1() {
    std::vector<std::uint8_t> b1(kBankSize, 0x00);
    // 0x0020 (direccion absoluta 0x0820): Banco 1 entry
    const std::uint8_t code20[] = {
        0xBF, 0xA5, // MOV R7, #0xA5
        0xE5,       // SEL MB0
        0x04, 0x40  // JMP 0x0040
    };
    for (std::size_t i = 0; i < sizeof(code20); ++i) {
        b1[0x20 + i] = code20[i];
    }
    return b1;
}

std::string buildDiagnosticManifest(const std::string& bank0Hash, const std::string& bank1Hash) {
    std::string json;
    json += "{\n";
    json += "  \"format\": \"bdmcs48se-firmware\",\n";
    json += "  \"formatVersion\": 1,\n";
    json += "  \"id\": \"mcs48.test.diagnostic\",\n";
    json += "  \"name\": \"MCS-48 Diagnostic Test ROM\",\n";
    json += "  \"version\": \"1.0.0\",\n";
    json += "  \"author\": \"ABDMCS48SE Clean-Room Team\",\n";
    json += "  \"description\": \"ROM propia de autotest de RAM, timer, salto interbanco A11 y rampa DAC.\",\n";
    json += "  \"license\": \"CC0-1.0\",\n";
    json += "  \"provenance\": \"Programa propio de prueba arquitectonica.\",\n";
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
    json += "      \"size\": 2048,\n";
    json += "      \"sha256\": \"" + bank0Hash + "\"\n";
    json += "    },\n";
    json += "    {\n";
    json += "      \"id\": \"program-bank1\",\n";
    json += "      \"file\": \"roms/program-bank1.rom\",\n";
    json += "      \"region\": \"program\",\n";
    json += "      \"bank\": 1,\n";
    json += "      \"offset\": 0,\n";
    json += "      \"size\": 2048,\n";
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
    json += "    { \"id\": \"engine_clock\", \"type\": \"float\", \"minimum\": 0.0625, \"maximum\": 8.0, \"initial\": 1.0, \"hostAutomatable\": true }\n";
    json += "  ]\n";
    json += "}\n";
    return json;
}

} // namespace

LoadedPackage diagnosticPackage() {
    const std::vector<std::uint8_t> b0 = buildBank0();
    const std::vector<std::uint8_t> b1 = buildBank1();

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    const std::string text = buildDiagnosticManifest(Sha256::hexOf(b0), Sha256::hexOf(b1));
    if (!FirmwarePackage::parseManifest(text, package.manifest, issues)) {
        return {};
    }

    LoadedImage img0;
    img0.id = "program-bank0";
    img0.region = "program";
    img0.bank = 0;
    img0.offset = 0;
    img0.data = b0;

    LoadedImage img1;
    img1.id = "program-bank1";
    img1.region = "program";
    img1.bank = 1;
    img1.offset = 0;
    img1.data = b1;

    package.images.push_back(std::move(img0));
    package.images.push_back(std::move(img1));

    return package;
}

} // namespace bdmcs48::firmware::demo
