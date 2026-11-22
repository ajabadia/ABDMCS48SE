// EnhancedMonoVoiceFirmware.h - Firmware mejorado con mayor paleta sonora.
//
// Genera ondas con mayor variedad: puede leer latches para controlar
// el ciclo de trabajo, forma de onda y otros parametros.
//
// Escrito a mano para ABDMCS48SE: no contiene codigo de terceros ni partes
// de ROMs comerciales.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "firmware/PackageLoader.h"

namespace bdmcs48::firmware::demo {

// Frecuencia nativa con engine_clock = 1.0
constexpr double kEnhancedMonoVoiceBaseFrequencyHz = 443.3333;

struct EnhancedMonoVoiceImage {
    std::string id;
    std::string region;
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> data;
};

// Imagen del programa
const std::vector<EnhancedMonoVoiceImage>& enhancedMonoVoiceImages();

// Paquete completo en memoria
LoadedPackage enhancedMonoVoicePackage();

// Escribe el paquete en `directory` (firmware.json + roms/)
bool writeEnhancedMonoVoicePackage(const std::string& directory, std::string& error);

// Descripcion legible
std::string enhancedMonoVoiceDescription();

} // namespace bdmcs48::firmware::demo
