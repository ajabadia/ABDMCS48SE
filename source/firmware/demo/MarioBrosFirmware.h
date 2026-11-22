// MarioBrosFirmware.h - Firmware ORIGINAL de efectos sonoros estilo Mario Bros.
//
// Genera efectos sonoros simples usando el DAC y control por reloj.
// Escrito a mano para ABDMCS48SE: no contiene codigo de terceros ni partes
// de ROMs comerciales.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "firmware/PackageLoader.h"

namespace bdmcs48::firmware::demo {

struct MarioBrosImage {
    std::string id;
    std::string region;
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> data;
};

// Imagen del programa de efectos Mario.
const std::vector<MarioBrosImage>& marioBrosImages();

// Paquete completo en memoria.
LoadedPackage marioBrosPackage();

// Escribe el paquete en `directory` (firmware.json + roms/).
bool writeMarioBrosPackage(const std::string& directory, std::string& error);

// Descripcion legible del programa.
std::string marioBrosDescription();

} // namespace bdmcs48::firmware::demo
