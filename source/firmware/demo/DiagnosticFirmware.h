// DiagnosticFirmware.h - ROM propia de diagnostico arquitectonico MCS-48.
//
// Conforme a la especificacion normativa docs/TEST_ROM_SPEC.md:
//   * Reset formal y vectores arquitectonicos.
//   * Autotest de memoria RAM interna (0x08..0x7F).
//   * Inicializacion y avance del temporizador/contador (MOV T,A y STRT T).
//   * Salto interbanco (SEL MB1 -> JMP 0x820 -> SEL MB0 -> retorno 0x040).
//   * Generador determinista de audio en P1 (DAC) con rampa periodica de 16 niveles.

#pragma once

#include "firmware/PackageLoader.h"

namespace bdmcs48::firmware::demo {

// Devuelve el paquete cargado en memoria de la ROM de diagnostico.
LoadedPackage diagnosticPackage();

} // namespace bdmcs48::firmware::demo
