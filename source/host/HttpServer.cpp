#include "host/HttpServer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace bdmcs48::host {
namespace {

namespace fs = std::filesystem;

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void closeSocket(SocketHandle handle) { closesocket(handle); }
int lastError() { return WSAGetLastError(); }
constexpr int kWouldBlock = WSAEWOULDBLOCK;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void closeSocket(SocketHandle handle) { close(handle); }
int lastError() { return errno; }
constexpr int kWouldBlock = EWOULDBLOCK;
#endif

bool socketsReady(std::string& error) {
#if defined(_WIN32)
    WSADATA data;
    const int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        error = "WSAStartup fallo con codigo " + std::to_string(result);
        return false;
    }
#endif
    return true;
}

std::string lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string contentTypeFor(const fs::path& path) {
    const std::string extension = lowercase(path.extension().string());
    if (extension == ".html" || extension == ".htm") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".js") {
        return "text/javascript; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    if (extension == ".json") {
        return "application/json; charset=utf-8";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

const char* statusText(int status) {
    switch (status) {
    case 200: return "OK";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    default: return "OK";
    }
}

} // namespace

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start(std::uint16_t port, const std::string& staticRoot, Handler handler, std::string& error) {
    if (running_.load(std::memory_order_acquire)) {
        error = "el servidor ya esta arrancado";
        return false;
    }
    if (!socketsReady(error)) {
        return false;
    }

    listenSocket_ = static_cast<SocketHandle>(socket(AF_INET, SOCK_STREAM, 0));
    if (listenSocket_ == kInvalidSocket) {
        error = "no se pudo crear el socket (error " + std::to_string(lastError()) + ")";
        return false;
    }

    int reuse = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // solo el bucle local
    address.sin_port = htons(port);

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        error = "no se pudo enlazar al puerto " + std::to_string(port) + " (error " +
                std::to_string(lastError()) + ")";
        closeSocket(listenSocket_);
        listenSocket_ = kInvalidSocket;
        return false;
    }
    if (listen(listenSocket_, 8) != 0) {
        error = "no se pudo escuchar en el puerto " + std::to_string(port);
        closeSocket(listenSocket_);
        listenSocket_ = kInvalidSocket;
        return false;
    }

    // Si se pidio el puerto 0, el sistema elige uno: se lee del socket.
    sockaddr_in bound{};
    socklen_t length = sizeof(bound);
    if (getsockname(listenSocket_, reinterpret_cast<sockaddr*>(&bound), &length) == 0) {
        port_ = ntohs(bound.sin_port);
    } else {
        port_ = port;
    }

    staticRoot_ = staticRoot;
    handler_ = std::move(handler);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { serveLoop(); });
    return true;
}

void HttpServer::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    if (listenSocket_ != kInvalidSocket) {
        closeSocket(listenSocket_);
        listenSocket_ = kInvalidSocket;
    }
}

void HttpServer::serveLoop() {
    while (running_.load(std::memory_order_acquire)) {
        // select con tiempo de espera: el hilo puede comprobar la bandera
        // de parada sin depender de que el cierre del socket despierte al
        // accept (eso no esta garantizado en todas las plataformas).
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket_, &readSet);
        timeval timeout{};
        timeout.tv_sec = 0;
        timeout.tv_usec = 200'000;

#if defined(_WIN32)
        const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
#else
        const int ready = select(listenSocket_ + 1, &readSet, nullptr, nullptr, &timeout);
#endif
        if (ready <= 0) {
            continue;
        }
        const SocketHandle client = accept(listenSocket_, nullptr, nullptr);
        if (client == kInvalidSocket) {
            continue;
        }
        handleConnection(static_cast<int>(client));
        closeSocket(client);
    }
}

void HttpServer::handleConnection(int socket) {
    std::string request;
    char buffer[2048];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16 * 1024) {
#if defined(_WIN32)
        const int received = recv(socket, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        const ssize_t received = recv(socket, buffer, sizeof(buffer), 0);
#endif
        if (received <= 0) {
            break;
        }
        request.append(buffer, static_cast<std::size_t>(received));
    }
    if (request.empty()) {
        return;
    }

    // Linea de peticion: METODO RUTA HTTP/1.1
    const std::size_t lineEnd = request.find("\r\n");
    const std::string requestLine = request.substr(0, lineEnd);
    const std::size_t firstSpace = requestLine.find(' ');
    const std::size_t secondSpace = requestLine.find(' ', firstSpace == std::string::npos ? 0 : firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
        return;
    }

    HttpRequest httpRequest;
    httpRequest.method = requestLine.substr(0, firstSpace);
    std::string target = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const std::size_t queryStart = target.find('?');
    if (queryStart != std::string::npos) {
        httpRequest.query = target.substr(queryStart + 1);
        target = target.substr(0, queryStart);
    }
    httpRequest.path = target;

    HttpResponse response;
    bool handled = false;
    if (handler_) {
        handled = handler_(httpRequest, response);
    }
    if (!handled) {
        serveStatic(httpRequest, response);
    }

    std::string head = "HTTP/1.1 " + std::to_string(response.status) + " " +
                       statusText(response.status) + "\r\n";
    head += "Content-Type: " + response.contentType + "\r\n";
    head += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    head += "Cache-Control: no-store\r\n";
    head += "Connection: close\r\n\r\n";

    std::string out = head + response.body;
    std::size_t sent = 0;
    while (sent < out.size()) {
#if defined(_WIN32)
        const int written = send(socket, out.data() + sent, static_cast<int>(out.size() - sent), 0);
#else
        const ssize_t written = send(socket, out.data() + sent, out.size() - sent, 0);
#endif
        if (written <= 0) {
            break;
        }
        sent += static_cast<std::size_t>(written);
    }
}

bool HttpServer::serveStatic(const HttpRequest& request, HttpResponse& response) const {
    if (request.method != "GET") {
        response.status = 405;
        response.contentType = "text/plain; charset=utf-8";
        response.body = "solo se admite GET\n";
        return true;
    }
    if (staticRoot_.empty()) {
        response.status = 404;
        response.contentType = "text/plain; charset=utf-8";
        response.body = "no hay directorio estatico configurado\n";
        return true;
    }

    std::string relative = request.path;
    if (relative.empty() || relative == "/") {
        relative = "/index.html";
    }
    // Seguridad: la ruta no puede salirse del directorio servido.
    if (relative.find("..") != std::string::npos) {
        response.status = 404;
        response.contentType = "text/plain; charset=utf-8";
        response.body = "ruta no valida\n";
        return true;
    }

    std::error_code code;
    const fs::path root = fs::weakly_canonical(fs::path(staticRoot_), code);
    if (code) {
        response.status = 404;
        response.body = "no se pudo resolver el directorio estatico\n";
        return true;
    }
    const fs::path target = fs::weakly_canonical(root / fs::path(relative).relative_path(), code);
    if (code || !fs::is_regular_file(target, code) || code) {
        response.status = 404;
        response.contentType = "text/plain; charset=utf-8";
        response.body = "no encontrado\n";
        return true;
    }
    const fs::path relativeTarget = target.lexically_relative(root);
    for (const auto& part : relativeTarget) {
        if (part == "..") {
            return true;
        }
    }


    std::ifstream stream(target, std::ios::binary);
    if (!stream) {
        response.status = 404;
        response.body = "no se pudo abrir el fichero\n";
        return true;
    }
    response.body.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    response.status = 200;
    response.contentType = contentTypeFor(target);
    return true;
}

} // namespace bdmcs48::host
