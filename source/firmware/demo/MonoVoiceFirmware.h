// MonoVoiceFirmware.h - Firmware ORIGINAL de voz monofonica.
//
// A diferencia del firmware de demostracion (que mantiene el DAC alto durante
// unas pocas instrucciones y produce un pulso muy estrecho), este programa
// genera una onda cuadrada simetrica de 50 % de ciclo de trabajo:
//
//     DAC alto (0xC0) -> retardo -> DAC bajo (0x40) -> retardo -> repetir
//
// La frecuencia no la fija el programa: el retardo son 220 iteraciones de
// DJNZ y el periodo completo son 902 ciclos de maquina. Con la CPU a 6 MHz
// (400000 ciclos/s) son 443,33 Hz. El host afina cada nota cambiando el reloj
// de la CPU (parametro engine_clock): la frecuencia es proporcional al reloj,
// de modo que una escala de 0,5 baja una octava exacta.
//
// La puerta (note on/off) la aplica el host con una envolvente de amplitud:
// el firmware no necesita leer ningun latch y suena igual mientras la CPU
// corre. Es el reparto mas simple para el primer instrumento y deja el
// programa en 16 bytes.
//
// Escrito a mano para ABDMCS48SE: no contiene codigo de terceros ni partes
// de ROMs comerciales.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "firmware/PackageLoader.h"

namespace bdmcs48::firmware::demo {

// Frecuencia nativa del programa con engine_clock = 1.0 (CPU a 6 MHz).
// Medida sobre el audio renderizado: 400000 ciclos/s de CPU / 902 ciclos por
// periodo = 443,33 Hz. Si cambia el retardo hay que recalibrar este valor
// (TestInstrument.LaFrecuenciaNativaCoincideConLaConstante lo comprueba).
constexpr double kMonoVoiceBaseFrequencyHz = 443.3333;

struct MonoVoiceImage {
    std::string id;
    std::string region;
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> data;
};

// Imagen del programa de voz.
const std::vector<MonoVoiceImage>& monoVoiceImages();

// Paquete completo en memoria (manifest ya parseado y hashes calculados).
LoadedPackage monoVoicePackage();

// Escribe el paquete en `directory` (firmware.json + roms/).
bool writeMonoVoicePackage(const std::string& directory, std::string& error);

// Descripcion legible del programa, para ayuda y documentacion.
std::string monoVoiceDescription();

} // namespace bdmcs48::firmware::demo
