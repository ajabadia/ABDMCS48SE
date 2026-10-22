#include "firmware/PackageLoader.h"

#include <filesystem>
#include <fstream>
#include <system_error>

#include "firmware/FirmwarePackage.h"
#include "firmware/Sha256.h"

namespace bdmcs48::firmware {
namespace {

namespace fs = std::filesystem;

void addIssue(std::vector<ValidationIssue>& issues, std::string field, std::string message) {
    issues.push_back(ValidationIssue{std::move(field), std::move(message)});
}

// Devuelve true si `path` esta dentro de `root`. Se comparan las rutas
// canonicas, asi que un enlace simbolico que salga del paquete se rechaza.
bool isInside(const fs::path& root, const fs::path& path) {
    const fs::path relative = path.lexically_relative(root);
    if (relative.empty()) {
        return false;
    }
    // El check de extrapolacion se hace sobre el bytes, no sobre la
    // string: la ruta no sale del paquete si nunca lleva "..".
    // La ruta puede contener ".." que apunta fuera del directorio del
    // paquete: se detecta comparando las rutas canonicas en isInside, y
    // aqui se comprueba que los bytes no sean la secuencia de punto-punto.
    for (const auto& part : relative) {
        if (part == "..") {
            return false;
        }
    }
    for (const auto& part : relative) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

bool readFileLimited(const fs::path& path, std::size_t maxBytes, std::vector<std::uint8_t>& out,
                     std::string& error) {
    std::error_code code;
    if (!fs::is_regular_file(path, code) || code) {
        error = "no es un fichero regular";
        return false;
    }
    const auto size = fs::file_size(path, code);
    if (code) {
        error = "no se pudo leer el tamano del fichero";
        return false;
    }
    if (size > maxBytes) {
        error = "el fichero supera el tamano maximo permitido (" + std::to_string(maxBytes) + " bytes)";
        return false;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "no se pudo abrir el fichero";
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    if (size != 0) {
        stream.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
        if (!stream) {
            error = "lectura incompleta";
            return false;
        }
    }
    return true;
}

} // namespace

bool PackageLoader::load(const std::string& packageRoot, LoadedPackage& out,
                         std::vector<ValidationIssue>& issues) {
    std::error_code code;
    fs::path root = fs::absolute(fs::path(packageRoot), code);
    if (code) {
        addIssue(issues, "package", "no se pudo resolver la ruta del paquete");
        return false;
    }
    root = fs::weakly_canonical(root, code);
    if (code) {
        addIssue(issues, "package", "la ruta del paquete no existe o no se puede canonizar");
        return false;
    }
    if (!fs::is_directory(root, code) || code) {
        addIssue(issues, "package", "la raiz del paquete no es un directorio");
        return false;
    }

    const fs::path manifestPath = root / kManifestFileName;
    std::vector<std::uint8_t> manifestBytes;
    std::string error;
    if (!readFileLimited(manifestPath, kMaxManifestBytes, manifestBytes, error)) {
        addIssue(issues, "manifest", std::string("no se pudo leer '") + kManifestFileName + "': " + error);
        return false;
    }

    const std::string manifestText(reinterpret_cast<const char*>(manifestBytes.data()), manifestBytes.size());
    if (!FirmwarePackage::parseManifest(manifestText, out.manifest, issues)) {
        return false;
    }
    if (!FirmwarePackage::validate(out.manifest, issues)) {
        return false;
    }

    for (const ImageDescription& description : out.manifest.images) {
        if (!FirmwarePackage::isSafeRelativePath(description.file)) {
            addIssue(issues, "images." + description.id, "ruta no segura");
            return false;
        }

        const fs::path candidate = root / fs::path(description.file);
        const fs::path resolved = fs::weakly_canonical(candidate, code);
        if (code) {
            addIssue(issues, "images." + description.id, "no se pudo resolver la ruta de la imagen");
            return false;
        }
        if (!isInside(root, resolved)) {
            addIssue(issues, "images." + description.id,
                     "la imagen '" + description.file + "' se sale del paquete");
            return false;
        }

        LoadedImage image;
        image.id = description.id;
        image.region = description.region;
        image.bank = description.bank;
        image.offset = description.offset;

        if (!readFileLimited(resolved, kMaxImageBytes, image.data, error)) {
            addIssue(issues, "images." + description.id, "no se pudo leer la imagen: " + error);
            return false;
        }

        if (description.size != 0 && image.data.size() < description.size) {
            addIssue(issues, "images." + description.id,
                     "el manifiesto declara " + std::to_string(description.size) +
                         " bytes y el fichero tiene " + std::to_string(image.data.size()));
            return false;
        }
        if (description.size != 0) {
            image.data.resize(description.size);
        }
        if (!description.sha256.empty() && !Sha256::matches(image.data, description.sha256)) {
            addIssue(issues, "images." + description.id,
                     "el hash SHA-256 no coincide con el declarado en el manifiesto");
            return false;
        }

        out.images.push_back(std::move(image));
    }

    return true;
}

} // namespace bdmcs48::firmware
