// WebUIResources.h - Recursos web embebidos en el binario.
//
// Lo que se empaqueta con juce_add_binary_data es WebUI/dist, el bundle que
// genera Vite (`node Scripts/build_webui.js`): el plugin funciona sin un
// directorio WebUI/ al lado del binario. Este mapeador no depende de JUCE:
// convierte una ruta HTTP en mime + bytes, y el editor la envuelve en el tipo
// de WebBrowserComponent::Resource.
//
// Las rutas del bundle no llevan hash (assets/index.js, assets/index.css), asi
// que se resuelven por coincidencia exacta o por sufijo dentro de los nombres
// que juce_add_binary_data genero. Una ruta desconocida NO devuelve la pagina:
// devuelve false, para que un asset roto se vea como un fallo y no como una UI
// medio cargada.

#pragma once

#include <cstddef>
#include <string>

namespace bdmcs48::webui {

struct Resource {
    const char* mimeType = "application/octet-stream";
    const char* data = nullptr;
    std::size_t size = 0;
};

// Acepta "/", "/index.html", "/assets/index.js", "assets/index.css" y las
// formas absolutas que usa el provider (juce://..., https://juce.backend/...).
// Devuelve false si la ruta no existe en el bundle.
bool resourceFor(const std::string& path, Resource& out);

// Mime type por extension del fichero; octet-stream si no se reconoce.
const char* mimeTypeFor(const std::string& path);

} // namespace bdmcs48::webui
