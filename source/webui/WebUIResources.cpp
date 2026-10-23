#include "webui/WebUIResources.h"

#include "BinaryData.h"

namespace bdmcs48::webui {
namespace {

std::string normalize(const std::string& path) {
    std::string result = path;
    const auto query = result.find('?');
    if (query != std::string::npos) {
        result.erase(query);
    }
    while (!result.empty() && result.front() == '/') {
        result.erase(result.begin());
    }
    if (result.empty()) {
        result = "index.html";
    }
    return result;
}

} // namespace

bool resourceFor(const std::string& path, Resource& out) {
    const auto name = normalize(path);

    if (name == "index.html") {
        out = Resource{"text/html; charset=utf-8", BinaryData::index_html,
                       static_cast<std::size_t>(BinaryData::index_htmlSize)};
        return true;
    }
    if (name == "main.js") {
        out = Resource{"text/javascript; charset=utf-8", BinaryData::main_js,
                       static_cast<std::size_t>(BinaryData::main_jsSize)};
        return true;
    }
    if (name == "main.css") {
        out = Resource{"text/css; charset=utf-8", BinaryData::main_css,
                       static_cast<std::size_t>(BinaryData::main_cssSize)};
        return true;
    }

    // Cualquier otra ruta se sirve como el documento: asi una recarga con
    // hash o una navegacion interna no deja la WebView en blanco.
    out = Resource{"text/html; charset=utf-8", BinaryData::index_html,
                   static_cast<std::size_t>(BinaryData::index_htmlSize)};
    return true;
}

} // namespace bdmcs48::webui
