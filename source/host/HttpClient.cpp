#include "host/HttpClient.h"

#include <cstddef>
#include <string>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace bdmcs48::host {
namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;

bool socketsReady() {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void socketsDone() { WSACleanup(); }

void closeSocket(SocketHandle handle) { closesocket(handle); }

int sendBytes(SocketHandle handle, const char* data, std::size_t size) {
    return send(handle, data, static_cast<int>(size), 0);
}

int receiveBytes(SocketHandle handle, char* data, std::size_t size) {
    return recv(handle, data, static_cast<int>(size), 0);
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;

bool socketsReady() { return true; }

void socketsDone() {}

void closeSocket(SocketHandle handle) { ::close(handle); }

int sendBytes(SocketHandle handle, const char* data, std::size_t size) {
    const ssize_t result = ::send(handle, data, size, 0);
    return static_cast<int>(result);
}

int receiveBytes(SocketHandle handle, char* data, std::size_t size) {
    const ssize_t result = ::recv(handle, data, size, 0);
    return static_cast<int>(result);
}
#endif

} // namespace

bool httpGet(std::uint16_t port, const std::string& path, std::string& response,
             std::string& error) {
    response.clear();
    error.clear();

    if (!socketsReady()) {
        error = "no se pudo inicializar la pila de sockets";
        return false;
    }

    const SocketHandle socketHandle = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socketHandle == kInvalidSocket) {
        error = "no se pudo crear el socket";
        socketsDone();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (::connect(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = "no se pudo conectar con 127.0.0.1:" + std::to_string(port);
        closeSocket(socketHandle);
        socketsDone();
        return false;
    }

    const std::string request =
        "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (sendBytes(socketHandle, request.data(), request.size()) < 0) {
        error = "no se pudo enviar la peticion";
        closeSocket(socketHandle);
        socketsDone();
        return false;
    }

    char buffer[4096];
    for (;;) {
        const int received = receiveBytes(socketHandle, buffer, sizeof(buffer));
        if (received <= 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(received));
    }

    closeSocket(socketHandle);
    socketsDone();

    if (response.empty()) {
        error = "el servidor cerro la conexion sin responder";
        return false;
    }
    return true;
}

} // namespace bdmcs48::host
