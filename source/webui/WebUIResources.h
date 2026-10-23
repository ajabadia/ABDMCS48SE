// WebUIResources.h - Recursos web embebidos en el binario.
//
// Los ficheros de source/webui/ se empaquetan con juce_add_binary_data, asi
// que el plugin funciona sin un directorio WebUI/ al lado del binario. Este
// mapeador no depende de JUCE: convierte una ruta HTTP en mime + bytes, y el
// editor la envuelve en el tipo de WebBrowserComponent::Resource.

#pragma once

#include <cstddef>
#include <string>

namespace bdmcs48::webui {

struct Resource {
    const char* mimeType = "application/octet-stream";
    const char* data = nullptr;
    std::size_t size = 0;
};

// Acepta "/", "/index.html", "index.html", "/main.js" y "/main.css".
bool resourceFor(const std::string& path, Resource& out);

} // namespace bdmcs48::webui
