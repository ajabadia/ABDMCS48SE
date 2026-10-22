#include "TestFramework.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "firmware/FirmwarePackage.h"
#include "firmware/PackageLoader.h"
#include "firmware/demo/DemoFirmware.h"

namespace fs = std::filesystem;

using bdmcs48::firmware::FirmwareManifest;
using bdmcs48::firmware::FirmwarePackage;
using bdmcs48::firmware::LoadedPackage;
using bdmcs48::firmware::PackageLoader;
using bdmcs48::firmware::ValidationIssue;

namespace {

std::string gTempRoot;

const std::string& tempRoot() {
    if (gTempRoot.empty()) {
        gTempRoot = (fs::temp_directory_path() / "bdmcs48se-tests").string();
        fs::create_directories(gTempRoot);
    }
    return gTempRoot;
}

std::string makeTempDirectory(const std::string& name) {
    const fs::path path = fs::path(tempRoot()) / name;
    std::error_code code;
    fs::remove_all(path, code);
    fs::create_directories(path, code);
    return path.string();
}

std::string validManifest() {
    return R"({
  "format": "bdmcs48se-firmware",
  "formatVersion": 1,
  "id": "test.paquete",
  "name": "Paquete de prueba",
  "version": "1.0.0",
  "machine": {
    "profile": "generic-mcs48",
    "cpu": { "family": "mcs48", "variant": "8049", "clockHz": 6000000 },
    "programMemory": { "addressBits": 12, "bankSizeBytes": 2048, "bankCount": 2 }
  },
  "images": [
    { "id": "bank0", "file": "roms/b0.rom", "region": "program", "bank": 0, "offset": 0, "size": 2 }
  ]
})";
}

bool parseAndValidate(const std::string& text, FirmwareManifest& manifest,
                      std::vector<ValidationIssue>& issues) {
    if (!FirmwarePackage::parseManifest(text, manifest, issues)) {
        return false;
    }
    return FirmwarePackage::validate(manifest, issues);
}

bool hasIssueFor(const std::vector<ValidationIssue>& issues, const std::string& field) {
    for (const ValidationIssue& issue : issues) {
        if (issue.field == field) {
            return true;
        }
    }
    return false;
}

void writeText(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

} // namespace

TEST(FirmwarePackage, AceptaUnManifiestoValido) {
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(parseAndValidate(validManifest(), manifest, issues));
    CHECK_EQ(issues.size(), std::size_t{0});
    CHECK_EQ(manifest.id, std::string("test.paquete"));
    CHECK_EQ(manifest.images.size(), std::size_t{1});
    CHECK_EQ(manifest.machine.cpu.clockHz, std::uint32_t{6'000'000});
}

TEST(FirmwarePackage, RechazaPerfilDesconocido) {
    std::string text = validManifest();
    const std::size_t position = text.find("generic-mcs48");
    text.replace(position, std::string("generic-mcs48").size(), "perfil-inventado");
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!parseAndValidate(text, manifest, issues));
    CHECK(hasIssueFor(issues, "machine.profile"));
}

TEST(FirmwarePackage, RechazaVarianteDeCpuDesconocida) {
    std::string text = validManifest();
    const std::size_t position = text.find("\"8049\"");
    text.replace(position, 6, "\"upi-41\"");
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!parseAndValidate(text, manifest, issues));
    CHECK(hasIssueFor(issues, "machine.cpu.variant"));
}

TEST(FirmwarePackage, RechazaBancoFueraDeRango) {
    std::string text = validManifest();
    const std::size_t position = text.find("\"bank\": 0");
    text.replace(position, 9, "\"bank\": 5");
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!parseAndValidate(text, manifest, issues));
    CHECK(hasIssueFor(issues, "images.bank0.bank"));
}

TEST(FirmwarePackage, RechazaImagenQueNoCabeEnElBanco) {
    std::string text = validManifest();
    const std::size_t position = text.find("\"size\": 2");
    text.replace(position, 8, "\"size\": 99999");
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!parseAndValidate(text, manifest, issues));
    CHECK(hasIssueFor(issues, "images.bank0.size"));
}

TEST(FirmwarePackage, RechazaRutaInsegura) {
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!FirmwarePackage::isSafeRelativePath("../fuera.rom"));
    CHECK(!FirmwarePackage::isSafeRelativePath("roms/../../fuera.rom"));
    CHECK(!FirmwarePackage::isSafeRelativePath("/absoluta.rom"));
    CHECK(!FirmwarePackage::isSafeRelativePath("C:\\absoluta.rom"));
    CHECK(!FirmwarePackage::isSafeRelativePath(""));
    CHECK(FirmwarePackage::isSafeRelativePath("roms/programa.rom"));
}

TEST(FirmwarePackage, RechazaSolapamientoDeImagenes) {
    std::string text = validManifest();
    const std::string extra =
        ",\n    { \"id\": \"bank0b\", \"file\": \"roms/b0b.rom\", \"region\": \"program\", "
        "\"bank\": 0, \"offset\": 1, \"size\": 4 }";
    const std::size_t position = text.find("\n  ]");
    text.insert(position, extra);
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!parseAndValidate(text, manifest, issues));
    CHECK(hasIssueFor(issues, "images.bank0"));
}

TEST(FirmwarePackage, RechazaIdDuplicado) {
    std::string text = validManifest();
    const std::string extra =
        ",\n    { \"id\": \"bank0\", \"file\": \"roms/otro.rom\", \"region\": \"program\", "
        "\"bank\": 1, \"offset\": 0, \"size\": 2 }";
    const std::size_t position = text.find("\n  ]");
    text.insert(position, extra);
    FirmwareManifest manifest;
    std::vector<ValidationIssue> issues;
    CHECK(!parseAndValidate(text, manifest, issues));
    CHECK(hasIssueFor(issues, "images.bank0"));
}

TEST(PackageLoader, CargaElPaqueteDeDemostracion) {
    const std::string directory = makeTempDirectory("demo");
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(directory, error));

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    CHECK(PackageLoader::load(directory, package, issues));
    CHECK_EQ(issues.size(), std::size_t{0});
    CHECK_EQ(package.manifest.id, std::string("mcs48.demo.tone"));
    CHECK_EQ(package.images.size(), std::size_t{2});
    for (const auto& image : package.images) {
        CHECK(!image.data.empty());
    }
    // Banco 1 en su sitio: el fetch de 0x800 tiene que dar el codigo.
    bool foundBank1 = false;
    for (const auto& image : package.images) {
        if (image.bank == 1) {
            foundBank1 = true;
            CHECK_EQ(image.offset, std::uint32_t{0});
            CHECK_EQ(image.data[0], std::uint8_t{0xe8}); // DJNZ R0
        }
    }
    CHECK(foundBank1);
}

TEST(PackageLoader, RechazaImagenManipulada) {
    const std::string directory = makeTempDirectory("manipulado");
    std::string error;
    CHECK(bdmcs48::firmware::demo::writeDemoPackage(directory, error));

    // Se cambia un byte de la imagen del banco 0: el hash ya no cuadra.
    const fs::path image = fs::path(directory) / "roms" / "program-bank0.rom";
    std::vector<std::uint8_t> bytes;
    {
        std::ifstream stream(image, std::ios::binary);
        bytes.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    }
    CHECK(!bytes.empty());
    bytes[0] = static_cast<std::uint8_t>(bytes[0] ^ 0xff);
    {
        std::ofstream stream(image, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    CHECK(!PackageLoader::load(directory, package, issues));
    CHECK(!issues.empty());
}

TEST(PackageLoader, RechazaPaqueteSinManifiesto) {
    const std::string directory = makeTempDirectory("vacio");
    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    CHECK(!PackageLoader::load(directory, package, issues));
    CHECK(hasIssueFor(issues, "manifest"));
}

TEST(PackageLoader, RechazaImagenFueraDelPaquete) {
    const std::string directory = makeTempDirectory("fuera");
    const std::string outside = makeTempDirectory("fuera-de-los-demas");
    writeText(fs::path(outside) / "roto.rom", "contenido");

    std::string manifest = validManifest();
    const std::size_t position = manifest.find("roms/b0.rom");
    manifest.replace(position, std::string("roms/b0.rom").size(), "../fuera-de-los-demas/roto.rom");
    writeText(fs::path(directory) / "firmware.json", manifest);

    LoadedPackage package;
    std::vector<ValidationIssue> issues;
    CHECK(!PackageLoader::load(directory, package, issues));
    CHECK(!issues.empty());
}
