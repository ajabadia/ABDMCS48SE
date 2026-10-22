// HttpServer.h - Servidor HTTP minimo para el cliente WebView.
//
// Es un servidor de archivo estatico mas un endpoint JSON. No es un
// servidor de produccion: escucha solo en el bucle local, no hace TLS y
// no implementa nada del protocolo que no necesite el cliente de
// diagnostico. La razon de escribirlo aqui (y no traer una dependencia) es
// que el host de referencia tiene que poder arrancar sin red.
//
// Endpoints:
//   GET /                 -> webview/index.html
//   GET /app.js, /style.css -> cliente WebView
//   GET /api/diagnostics  -> documento de diagnostico (JSON)
//   GET /api/logs         -> registros pendientes del bus de log (JSON)

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace bdmcs48::host {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
};

class HttpServer {
public:
    // Devuelve false si la peticion la resuelve el manejador (rutas dinamicas).
    using Handler = std::function<bool(const HttpRequest&, HttpResponse&)>;

    HttpServer() = default;
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Arranca el servidor en 127.0.0.1. `staticRoot` es el directorio del
    // cliente WebView; si esta vacio no se sirven archivos estaticos.
    bool start(std::uint16_t port, const std::string& staticRoot, Handler handler, std::string& error);

    void stop();

    std::uint16_t port() const { return port_; }
    bool running() const { return running_.load(std::memory_order_acquire); }

private:
    void serveLoop();
    void handleConnection(int socket);
    bool serveStatic(const HttpRequest& request, HttpResponse& response) const;

    std::uint16_t port_ = 0;
    std::string staticRoot_;
    Handler handler_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    int listenSocket_ = -1;
};

} // namespace bdmcs48::host
