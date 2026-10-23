// DemoFirmware.h - Firmware de demostracion ORIGINAL.
//
// El programa de este paquete esta escrito a mano para ABDMCS48SE: no
// contiene codigo de terceros ni partes de ROMs comerciales. Existe para
// poder ejercitar el pipeline completo (paquete -> manifest -> CPU -> audio
// -> diagnostico) sin depender de material con derechos.
//
// Que demuestra:
//   * Que la CPU MCS-48 ejecuta y escribe en el DAC (P1).
//   * Que SEL MB1 + JMP cambia de banco de programa (0x000 -> 0x800).
//   * Que un salto condicional NO cambia de banco: se queda en la pagina
//     actual, que es la diferencia entre el modelo correcto y el "banco" como
//     traduccion plana de direcciones.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "firmware/PackageLoader.h"

namespace bdmcs48::firmware::demo {

struct DemoImage {
    std::string id;
    std::string region;
    std::uint32_t bank = 0;
    std::uint32_t offset = 0;
    std::vector<std::uint8_t> data;
};

// Imagenes del programa de demostracion.
const std::vector<DemoImage>& demoImages();

// Escribe el paquete completo (manifest + imagenes) en `directory`.
// Los hashes SHA-256 del manifiesto se calculan aqui, de modo que nunca
// pueden quedar desincronizados con los bytes.
bool writeDemoPackage(const std::string& directory, std::string& error);

// Mismo paquete, ya cargado en memoria. Un host que no debe escribir en
// disco (el plugin) lo instala directamente; los bytes y los hashes son los
// mismos que produce writeDemoPackage().
LoadedPackage demoPackage();

// Descripcion legible del programa, para la ayuda y la documentacion.
std::string demoDescription();

} // namespace bdmcs48::firmware::demo
