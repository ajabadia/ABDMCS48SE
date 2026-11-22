#include "TestFramework.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/MachineProfile.h"
#include "core/OpcodeNames.h"

namespace {

using bdmcs48::core::MachineProfile;
using bdmcs48::core::MemoryRegion;
using bdmcs48::core::OpcodeTable;
using bdmcs48::core::RegionType;

struct GoldenInstruction {
    std::uint16_t address = 0;
    std::string hexBytes;
    std::uint8_t opcode = 0;
    std::uint8_t length = 0;
    std::string disassembly;
};

// Forma comparable de una instruccion: minusculas, espacios colapsados y los
// literales numericos (formatos de la tabla como #$%02X o $0%02X, direcciones
// de Ghidra como 0x0A0) sustituidos por <num>. Compara la forma del operando,
// no el valor concreto.
std::string normalizedShape(const std::string& text) {
    std::string folded;
    bool pendingSpace = false;
    for (char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (std::isspace(c) != 0) {
            pendingSpace = !folded.empty();
            continue;
        }
        if (pendingSpace) {
            folded.push_back(' ');
            pendingSpace = false;
        }
        folded.push_back(static_cast<char>(std::tolower(c)));
    }

    std::string out;
    for (std::size_t i = 0; i < folded.size();) {
        if (folded.compare(i, 2, "0x") == 0) {
            i += 2;
            while (i < folded.size() && std::isxdigit(static_cast<unsigned char>(folded[i])) != 0) {
                ++i;
            }
            out += "<num>";
            continue;
        }
        if (folded[i] == '$') {
            ++i;
            while (i < folded.size()) {
                const unsigned char c = static_cast<unsigned char>(folded[i]);
                if (std::isalnum(c) == 0 && c != '%') break;
                ++i;
            }
            out += "<num>";
            continue;
        }
        out.push_back(folded[i++]);
    }
    return out;
}

std::string findGoldenAsmPath() {
    // El listado es material privado y no vive en el repositorio: se toma de
    // BDMCS48SE_JUNO6_LISTING o, si no esta definida, junto al proyecto.
    std::vector<std::string> candidates;
    if (const char* fromEnv = std::getenv("BDMCS48SE_JUNO6_LISTING")) {
        candidates.emplace_back(fromEnv);
    }
    candidates.emplace_back("../ABDJuno6ROMRE/disasm/juno6_8049_238_ghidra.asm");
    candidates.emplace_back("../../ABDJuno6ROMRE/disasm/juno6_8049_238_ghidra.asm");
    for (const auto& path : candidates) {
        std::ifstream file(path);
        if (file.is_open()) {
            return path;
        }
    }
    return "";
}

std::string findJuno6RomPath() {
    // Misma regla: la ROM comercial nunca entra en el repositorio.
    std::vector<std::string> candidates;
    if (const char* fromEnv = std::getenv("BDMCS48SE_JUNO6_ROM")) {
        candidates.emplace_back(fromEnv);
    }
    candidates.emplace_back("workspace/roms/roland-juno6-upd8049c-238.bin");
    candidates.emplace_back("../ABDJuno6ROMRE/juno6/upd8049c-238.ic56");
    candidates.emplace_back("../../ABDJuno6ROMRE/juno6/upd8049c-238.ic56");
    for (const auto& path : candidates) {
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            return path;
        }
    }
    return "";
}

bool parseGoldenListing(const std::string& path,
                        std::vector<GoldenInstruction>& outInstructions,
                        std::unordered_set<std::uint8_t>& outUniqueOpcodes,
                        std::size_t& outDataBytes) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::size_t dataBytes = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';') continue;

        // Formato esperado: CODE:0000: 27         CLR A
        if (line.rfind("CODE:", 0) != 0) continue;

        std::size_t firstColon = 4;
        std::size_t secondColon = line.find(':', firstColon + 1);
        if (secondColon == std::string::npos) continue;

        std::string rest = line.substr(secondColon + 1);
        if (rest.find("; data") != std::string::npos) {
            // Tabla de datos: no es codigo, pero sus bytes cuentan para el
            // recuento del hito (504 B en el Juno-6).
            const std::string bytes = rest.substr(0, rest.find("; data"));
            dataBytes += static_cast<std::size_t>(std::count_if(
                             bytes.begin(), bytes.end(), [](unsigned char c) {
                                 return std::isxdigit(c) != 0;
                             })) / 2;
            continue;
        }
        if (rest.find("; ??") != std::string::npos) continue;

        std::string addrHex = line.substr(firstColon + 1, secondColon - (firstColon + 1));
        std::uint16_t addr = static_cast<std::uint16_t>(std::stoul(addrHex, nullptr, 16));

        std::istringstream stream(rest);
        std::string bytesHex;
        if (!(stream >> bytesHex)) continue;

        std::string disasm;
        std::getline(stream, disasm);
        // Trim leading spaces in disasm
        std::size_t startPos = disasm.find_first_not_of(" \t");
        if (startPos != std::string::npos) {
            disasm = disasm.substr(startPos);
        }

        std::uint8_t len = static_cast<std::uint8_t>(bytesHex.size() / 2);
        std::uint8_t op = static_cast<std::uint8_t>(std::stoul(bytesHex.substr(0, 2), nullptr, 16));

        GoldenInstruction inst;
        inst.address = addr;
        inst.hexBytes = bytesHex;
        inst.opcode = op;
        inst.length = len;
        inst.disassembly = disasm;

        outInstructions.push_back(inst);
        outUniqueOpcodes.insert(op);
    }

    outDataBytes = dataBytes;
    return true;
}

} // namespace

TEST(Juno6Conformidad, MapaFormalDeRegionesJuno6CubreRomAlCienPorCien) {
    const auto profile = MachineProfile::juno6();
    CHECK_EQ(profile.romSizeBytes, 2048u);
    CHECK_EQ(profile.cpuVariant, "8049");

    const auto codeBytes = profile.totalCodeBytes();
    const auto dataBytes = profile.totalDataBytes();

    CHECK_EQ(codeBytes, 1544u);
    CHECK_EQ(dataBytes, 504u);
    CHECK_EQ(codeBytes + dataBytes, 2048u);

    // Verificar vectores arquitectonicos de hardware
    const auto* resetReg = profile.findRegion(0x0000);
    CHECK(resetReg != nullptr);
    CHECK_EQ(static_cast<int>(resetReg->type), static_cast<int>(RegionType::ResetVector));
    CHECK_EQ(resetReg->startAddress, 0x0000u);
    CHECK_EQ(resetReg->endAddress, 0x0002u);

    const auto* extIntReg = profile.findRegion(0x0003);
    CHECK(extIntReg != nullptr);
    CHECK_EQ(static_cast<int>(extIntReg->type), static_cast<int>(RegionType::InterruptVector));

    const auto* timerIntReg = profile.findRegion(0x0007);
    CHECK(timerIntReg != nullptr);
    CHECK_EQ(static_cast<int>(timerIntReg->type), static_cast<int>(RegionType::TimerVector));

    // Verificar las tablas del autotest identificadas en el desensamblado
    const auto* bitmaskReg = profile.findRegion(0x00F8);
    CHECK(bitmaskReg != nullptr);
    CHECK_EQ(static_cast<int>(bitmaskReg->type), static_cast<int>(RegionType::DataTable));
    CHECK(bitmaskReg->isData());
    CHECK(!bitmaskReg->isCode());

    const auto* stepReg = profile.findRegion(0x0100);
    CHECK(stepReg != nullptr);
    CHECK_EQ(static_cast<int>(stepReg->type), static_cast<int>(RegionType::DataTable));
}

TEST(Juno6Conformidad, AuditoriaOpcodeNamesSinArtefactos8051) {
    const auto all = OpcodeTable::all();
    CHECK_EQ(all.size(), 256u);

    int illegalCount = 0;
    int implementedCount = 0;
    int twoBytesCount = 0;

    for (const auto& op : all) {
        if (op.isIllegal) illegalCount++;
        if (op.isImplemented) implementedCount++;
        if (op.length == 2) twoBytesCount++;

        // Asegurar que no quedan nombres o mnemónicos propios de 8051
        std::string name(op.name);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        CHECK(name.find("dptr") == std::string::npos);
        CHECK(name.find("cjne") == std::string::npos);
        CHECK(name.find("setb") == std::string::npos);
        CHECK(name.find("sjmp") == std::string::npos);
        CHECK(name.find("ljmp") == std::string::npos);
    }

    // Los 26 huecos ilegales exactos de la ISA MCS-48
    CHECK_EQ(illegalCount, 26);
    // 256 - 26 ilegales - 7 interrupciones hardware no soportadas = 223 implementados
    CHECK_EQ(implementedCount, 223);

    // Los 10 opcodes clave de la conformidad Juno-6 comprobados en la tabla
    CHECK_EQ(OpcodeTable::nameOf(0x27), std::string("clr  a"));
    CHECK_EQ(OpcodeTable::nameOf(0x36), std::string("jt0  $%03X"));
    CHECK_EQ(OpcodeTable::nameOf(0x37), std::string("cpl  a"));
    CHECK_EQ(OpcodeTable::nameOf(0x3A), std::string("outl p2,a"));
    CHECK_EQ(OpcodeTable::nameOf(0x3C), std::string("movd p4,a"));
    CHECK_EQ(OpcodeTable::nameOf(0x4B), std::string("orl  a,r3"));
    CHECK_EQ(OpcodeTable::nameOf(0x51), std::string("anl  a,@r1"));
    CHECK_EQ(OpcodeTable::nameOf(0x54), std::string("call $2%02X"));
    CHECK_EQ(OpcodeTable::nameOf(0xB2), std::string("jb5  $%03X"));
    CHECK_EQ(OpcodeTable::nameOf(0xB3), std::string("jmpp @a"));
}

TEST(Juno6Conformidad, FixtureConformidadDecodificacionGolden) {
    const std::string goldenPath = findGoldenAsmPath();
    if (goldenPath.empty()) {
        // Entorno sin repositorio hermano; se omite limpiamente
        return;
    }

    std::vector<GoldenInstruction> instructions;
    std::unordered_set<std::uint8_t> uniqueOpcodes;
    std::size_t dataBytes = 0;
    CHECK(parseGoldenListing(goldenPath, instructions, uniqueOpcodes, dataBytes));

    // Validar las cifras exactas del hito
    CHECK_EQ(instructions.size(), 1089u);
    CHECK_EQ(uniqueOpcodes.size(), 155u);
    CHECK_EQ(dataBytes, std::size_t{504});

    // Cargar ROM binaria para comprobar bytes si está disponible
    std::vector<std::uint8_t> rom(2048, 0);
    bool hasRom = false;
    const std::string romPath = findJuno6RomPath();
    if (!romPath.empty()) {
        std::ifstream romFile(romPath, std::ios::binary);
        if (romFile.is_open()) {
            romFile.read(reinterpret_cast<char*>(rom.data()), 2048);
            if (romFile.gcount() == 2048) {
                hasRom = true;
            }
        }
    }

    const auto profile = MachineProfile::juno6();
    std::unordered_map<std::uint8_t, int> discrepanciesByOpcode;
    int totalDiscrepancies = 0;
    int shapeDiscrepancies = 0;

    for (const auto& inst : instructions) {
        const std::uint8_t expectedLen = OpcodeTable::lengthOf(inst.opcode);
        
        // 1. Longitud de instruccion (1 o 2 bytes)
        if (expectedLen != inst.length) {
            discrepanciesByOpcode[inst.opcode]++;
            totalDiscrepancies++;
        }

        // 2. Ningun opcode ejecutado debe ser ilegal
        if (OpcodeTable::isIllegal(inst.opcode)) {
            discrepanciesByOpcode[inst.opcode]++;
            totalDiscrepancies++;
        }

        // 3. La forma del operando debe coincidir con el desensamblado de Ghidra
        if (normalizedShape(OpcodeTable::nameOf(inst.opcode)) != normalizedShape(inst.disassembly)) {
            ++shapeDiscrepancies;
            discrepanciesByOpcode[inst.opcode]++;
            totalDiscrepancies++;
        }

        // 4. La direccion debe pertenecer a una region de codigo del MachineProfile
        CHECK(profile.isCodeAddress(inst.address));

        // 5. Si la ROM binaria esta presente, verificar concordancia de bytes
        if (hasRom) {
            CHECK_EQ(rom[inst.address], inst.opcode);
            if (inst.length == 2 && inst.address + 1 < 2048) {
                std::uint8_t expectedArg = static_cast<std::uint8_t>(
                    std::stoul(inst.hexBytes.substr(2, 2), nullptr, 16));
                CHECK_EQ(rom[inst.address + 1], expectedArg);
            }
        }
    }

    // Cero discrepancias en decodificacion entre el desensamblado propio y el listado golden
    CHECK_EQ(totalDiscrepancies, 0);
    CHECK_EQ(shapeDiscrepancies, 0);
    CHECK(discrepanciesByOpcode.empty());
}
