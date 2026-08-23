#pragma once

#include <crow.h>

#include "cors_middleware.hpp"

namespace vitale::api {

// Registers every read-only route (objects list, per-object AUDT event
// history, ...) on the given app. Each handler opens its own short-lived
// Postgres connection via shared::make_connection() -- simple and safe for
// a low-traffic internal API; revisit with a connection pool if this
// becomes a bottleneck.
void register_routes(ApiApp& app);

} // namespace vitale::api
