#include <cstdlib>

#include <crow.h>

#include "routes.hpp"

int main() {
    vitale::api::ApiApp app;

    // Applies to every route, not just /objects/catalog -- Crow's
    // compression is an app-wide setting with per-response opt-out
    // (crow::response::compressed, default true), not a per-route opt-in.
    // Only gzip/deflate: Crow's compression support is zlib-based and has
    // no brotli codec, so a "br" Content-Encoding would have to come from a
    // CDN/reverse proxy in front of this origin, not from here.
    app.use_compression(crow::compression::algorithm::GZIP);

    vitale::api::register_routes(app);

    const char* port_env = std::getenv("API_PORT");
    const int port = port_env != nullptr ? std::atoi(port_env) : 8080;

    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
    return 0;
}
