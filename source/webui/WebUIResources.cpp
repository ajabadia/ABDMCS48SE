#include "webui/WebUIResources.h"

#include "BinaryData.h"

#include <cctype>

namespace bdmcs48::webui {
namespace {

std::string withForwardSlashes(std::string value) {
    for (auto& character : value) {
        if (character == '\\') {
            character = '/';
        }
    }
    return value;
}

// Deja solo la ruta relativa al bundle: quita esquema y host de las formas que
// entrega el provider, el query string (cache-busting) y las barras iniciales.
// La raiz y la cadena vacia apuntan al documento.
std::string normalize(const std::string& path) {
    std::string result = path;

    const auto query = result.find_first_of("?#");
    if (query != std::string::npos) {
        result.erase(query);
    }

    const auto dropHost = [](std::string& value, std::size_t hostStart) {
        const auto slash = value.find('/', hostStart);
        value = slash == std::string::npos ? std::string{} : value.substr(slash + 1);
    };

    if (result.rfind("juce://", 0) == 0) {
        dropHost(result, 7);
    } else if (result.rfind("https://juce.backend", 0) == 0) {
        dropHost(result, 20);
    } else if (result.rfind("https://localhost", 0) == 0) {
        dropHost(result, 17);
    } else if (result.rfind("http://localhost", 0) == 0) {
        dropHost(result, 16);
    }

    while (!result.empty() && result.front() == '/') {
        result.erase(result.begin());
    }
    if (result.empty()) {
        result = "index.html";
    }
    return result;
}

// juce_add_binary_data no guarda la ruta del proyecto, solo el nombre del
// fichero o una ruta parcial. Por eso: coincidencia exacta primero, y despues
// sufijo en los dos sentidos, que es lo que resuelve "assets/index.js" tanto si
// el nombre generado es "index.js" como si es "WebUI/dist/assets/index.js".
const char* findResourceName(const std::string& normalized) {
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        const std::string original = withForwardSlashes(BinaryData::originalFilenames[i]);
        if (original == normalized || original.ends_with("/" + normalized)) {
            return BinaryData::namedResourceList[i];
        }
    }

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        const std::string original = withForwardSlashes(BinaryData::originalFilenames[i]);
        if (original.empty() || normalized.size() <= original.size()) {
            continue;
        }
        const auto offset = normalized.size() - original.size();
        if (normalized.compare(offset, original.size(), original) == 0 && normalized[offset - 1] == '/') {
            return BinaryData::namedResourceList[i];
        }
    }

    return nullptr;
}

} // namespace

const char* mimeTypeFor(const std::string& path) {
    const auto slash = path.find_last_of('/');
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return "application/octet-stream";
    }

    std::string extension = path.substr(dot + 1);
    for (auto& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    if (extension == "html" || extension == "htm") return "text/html; charset=utf-8";
    if (extension == "js" || extension == "mjs") return "text/javascript; charset=utf-8";
    if (extension == "css") return "text/css; charset=utf-8";
    if (extension == "json" || extension == "map") return "application/json";
    if (extension == "svg") return "image/svg+xml";
    if (extension == "png") return "image/png";
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "gif") return "image/gif";
    if (extension == "webp") return "image/webp";
    if (extension == "ico") return "image/x-icon";
    if (extension == "woff") return "font/woff";
    if (extension == "woff2") return "font/woff2";
    if (extension == "ttf") return "font/ttf";
    if (extension == "otf") return "font/otf";
    if (extension == "wasm") return "application/wasm";
    if (extension == "txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool resourceFor(const std::string& path, Resource& out) {
    const auto name = normalize(path);

    const char* resourceName = findResourceName(name);
    if (resourceName == nullptr) {
        // Sin recurso no se inventa una pagina: quien llama devuelve 404.
        return false;
    }

    int size = 0;
    const char* data = BinaryData::getNamedResource(resourceName, size);
    if (data == nullptr || size <= 0) {
        return false;
    }

    out.mimeType = mimeTypeFor(name);
    out.data = data;
    out.size = static_cast<std::size_t>(size);
    return true;
}

} // namespace bdmcs48::webui
