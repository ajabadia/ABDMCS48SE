// MachineFactory.h - Construccion de maquinas a partir de un paquete.
//
// La fabrica es el punto donde el perfil declarado en el manifiesto se
// convierte en un objeto ejecutable. Es el unico sitio que conoce a la vez
// el formato del paquete y los perfiles de maquina.

#pragma once

#include <memory>
#include <string>

#include "core/MachineInstance.h"
#include "firmware/FirmwarePackage.h"

namespace bdmcs48::machines {

// Construye la maquina descrita por el manifiesto. Devuelve nullptr y
// rellena `error` si el perfil no se puede construir.
std::unique_ptr<core::MachineInstance> createMachine(const firmware::FirmwareManifest& manifest,
                                                      const firmware::LoadedPackage& package,
                                                      std::string& error);

} // namespace bdmcs48::machines
