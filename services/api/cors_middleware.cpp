#include "cors_middleware.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace vitale::api {

namespace {

// CORS_ALLOWED_ORIGINS entries may have spaces after commas depending on
// how the env var was written; trim before comparing.
std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> parse_origin_list(const std::string& csv) {
    std::vector<std::string> origins;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string trimmed = trim(item);
        if (!trimmed.empty()) {
            origins.push_back(trimmed);
        }
    }
    return origins;
}

std::vector<std::string> allowed_origins_from_env() {
    if (const char* env = std::getenv("CORS_ALLOWED_ORIGINS"); env != nullptr && *env != '\0') {
        return parse_origin_list(env);
    }

    // Defaults if CORS_ALLOWED_ORIGINS isn't set: production frontend,
    // Vercel preview deployments, and the typical Vite dev server port.
    return {
        "https://vitaleaerospace.com",
        "https://vitale-frontend.vercel.app",
        "http://localhost:5173",
    };
}

} // namespace

CorsMiddleware::CorsMiddleware() : allowed_origins_(allowed_origins_from_env()) {}

// Deliberately a no-op: Crow's route dispatch does `res = response(f(...))`
// for every matched route (a full assignment that replaces res.headers
// wholesale, per crow::response's copy/move assignment), which wipes
// anything set here before the client ever sees it. See after_handle for
// where this actually has to happen instead.
void CorsMiddleware::before_handle(crow::request&, crow::response&, context&) {}

void CorsMiddleware::after_handle(crow::request& req, crow::response& res, context&) {
    const std::string& origin = req.get_header_value("Origin");
    const bool origin_allowed =
        !origin.empty() &&
        std::find(allowed_origins_.begin(), allowed_origins_.end(), origin) != allowed_origins_.end();

    if (origin_allowed) {
        // Reflecting the specific matched origin, not "*" -- required to
        // actually allow more than one explicit origin.
        res.add_header("Access-Control-Allow-Origin", origin);
        res.add_header("Vary", "Origin");
    }
    res.add_header("Access-Control-Allow-Methods", "GET, OPTIONS");
    res.add_header("Access-Control-Allow-Headers", "Content-Type");

    // Nothing else to do for OPTIONS here: Crow auto-answers preflight
    // requests to any registered path with a 204 (or 200, if
    // CROW_RETURNS_OK_ON_HTTP_OPTIONS_REQUEST) plus an Allow header, fully
    // inside routing before middleware runs at all -- before_handle never
    // even sees these requests. This just layers the CORS-specific headers
    // on top of that already-complete response rather than building one
    // from scratch.
}

} // namespace vitale::api
