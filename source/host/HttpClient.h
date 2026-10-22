// HttpClient.h - Cliente HTTP minimo contra el bucle local.
//
// Existe para que nadie fuera de host/ tenga que incluir <winsock2.h> ni
// <sys/socket.h>: la diferencia entre Windows y POSIX se queda aqui. Lo usa
// la suite para consultar el documento de diagnostico por un socket real y
// lo podra usar cualquier herramienta que hable con `bdmcs48se serve`.

#pragma once

#include <cstdint>
#include <string>

namespace bdmcs48::host {

// GET http://127.0.0.1:<port><path>. Devuelve la respuesta cruda (cabeceras
// incluidas) en `response`. Devuelve false y rellena `error` si no se puede
// crear el socket, conectar o intercambiar datos.
bool httpGet(std::uint16_t port, const std::string& path, std::string& response,
             std::string& error);

} // namespace bdmcs48::host
