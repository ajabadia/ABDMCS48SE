// FirmwarePackage.h - Parseo y validacion del manifiesto.
//
// El JSON Schema NO alcanza para validar un paquete: comprueba la forma, no
// que un banco quepa en la memoria de programa, que el perfil de maquina
// exista o que dos imagenes no se solapen. Eso se comprueba aqui.

#pragma once

#include <string>
#include <vector>

#include "firmware/FirmwareTypes.h"

namespace bdmcs48::firmware {

class FirmwarePackage {
public:
    // Formato soportado por esta version del loader.
    static constexpr const char* kFormatId = "bdmcs48se-firmware";
    static constexpr std::uint32_t kSupportedFormatVersion = 1;

    // Perfiles de maquina que el host sabe construir.
    static bool isKnownProfile(const std::string& profile);

    // Variantes de CPU conocidas. La equivalencia de opcode NO es completa
    // entre familias: UPI-41/42 no se dan por equivalentes.
    static bool isKnownCpuVariant(const std::string& variant);

    // Parsea el texto del manifiesto. Los problemas de forma van a `issues`.
    static bool parseManifest(const std::string& text, FirmwareManifest& out,
                              std::vector<ValidationIssue>& issues);

    // Validacion semantica: perfiles, bancos, solapamientos, IDs, rangos y
    // rutas. No toca el disco.
    static bool validate(const FirmwareManifest& manifest, std::vector<ValidationIssue>& issues);

    // Comprueba que una ruta de imagen es segura: relativa, sin traversal y
    // sin raiz absoluta.
    static bool isSafeRelativePath(const std::string& path);

    // Formatea los problemas en un texto legible.
    static std::string formatIssues(const std::vector<ValidationIssue>& issues);
};

} // namespace bdmcs48::firmware
