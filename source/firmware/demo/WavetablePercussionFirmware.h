// WavetablePercussionFirmware.h - Firmware musical para MCS-48.
//
// Sintesis de percusion lo-fi (Kick, Snare, HiHat) y oscilador wavetable
// multimodo mediante MOVP3 sobre tablas en ROM (pagina 3).
//
// Escrito a mano para ABDMCS48SE: codigo limpio y original sin dependencias
// de ROMs comerciales protegidas.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "firmware/PackageLoader.h"

namespace bdmcs48::firmware::demo {

struct WavetablePercussionImage {
    std::string id;
    std::string region;
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> data;
};

// Imagenes del programa
const std::vector<WavetablePercussionImage>& wavetablePercussionImages();

// Paquete completo en memoria
LoadedPackage wavetablePercussionPackage();

// Escribe el paquete en directory (firmware.json + roms/)
bool writeWavetablePercussionPackage(const std::string& directory, std::string& error);

// Descripcion legible
std::string wavetablePercussionDescription();

} // namespace bdmcs48::firmware::demo
