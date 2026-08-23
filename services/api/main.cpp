#include <cstdlib>

#include <crow.h>

#include "routes.hpp"

int main() {
    vitale::api::ApiApp app;
    vitale::api::register_routes(app);

    const char* port_env = std::getenv("API_PORT");
    const int port = port_env != nullptr ? std::atoi(port_env) : 8080;

    app.port(static_cast<std::uint16_t>(port)).multithreaded().run();
    return 0;
}
