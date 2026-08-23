#pragma once

#include <string>
#include <vector>

#include <crow.h>

namespace vitale::api {

// Global CORS middleware. Crow's built-in crow::CORSHandler only supports a
// single fixed Access-Control-Allow-Origin value (crow/middlewares/cors.h
// itself notes "TODO: support multiple origins that are dynamically
// selected"), which doesn't fit this service's need to allow the production
// frontend, Vercel preview deployments, and local dev all at once. Instead,
// this reflects the request's own Origin header back verbatim, but only
// when it's in the allowlist -- the standard pattern for "allow exactly
// these N origins" rather than allowing anything ("*").
class CorsMiddleware {
public:
    struct context {};

    CorsMiddleware();

    void before_handle(crow::request& req, crow::response& res, context& ctx);
    void after_handle(crow::request& req, crow::response& res, context& ctx);

private:
    std::vector<std::string> allowed_origins_;
};

// Used by both main.cpp (to declare the app type) and routes.cpp (to accept
// it as a parameter) -- crow::App<CorsMiddleware> rather than
// crow::SimpleApp, now that this service has one global middleware.
using ApiApp = crow::App<CorsMiddleware>;

} // namespace vitale::api
