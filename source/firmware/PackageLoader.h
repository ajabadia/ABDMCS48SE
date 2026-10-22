// PackageLoader.h - Lectura de un paquete de firmware desde disco.
//
// Reglas que se aplican aqui (fuera del hilo de audio, siempre):
//   * La raiz del paquete se canoniza y todas las imagenes se resuelven
//     DENTRO de ella. Un enlace simbolico que apunte fuera se rechaza.
//   * Hay limites de tamano antes de reservar memoria.
//   * Cada imagen se verifica contra el hash declarado en el manifiesto.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "firmware/FirmwareTypes.h"

namespace bdmcs48::firmware {

struct LoadedImage {
    std::string id;
    std::string region;
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> data;
};

struct LoadedPackage {
    FirmwareManifest manifest;
    std::vector<LoadedImage> images;
};

class PackageLoader {
public:
    static constexpr const char* kManifestFileName = "firmware.json";
    static constexpr std::size_t kMaxManifestBytes = 512u * 1024u;
    static constexpr std::size_t kMaxImageBytes = 4u * 1024u * 1024u;

    // Carga y valida el paquete. Devuelve false y rellena `issues` si algo
    // falla; en ese caso `out` no debe usarse.
    static bool load(const std::string& packageRoot, LoadedPackage& out,
                     std::vector<ValidationIssue>& issues);
};

} // namespace bdmcs48::firmware
