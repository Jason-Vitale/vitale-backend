#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>

#include <pqxx/pqxx>

namespace vitale::shared {

// Reads the Postgres connection string from $DATABASE_URL (a standard
// libpq keyword/value or URI string, e.g.
// "postgresql://user:pass@host:5432/dbname?sslmode=require") -- this is how
// both the poller and the api service point at the same managed Postgres
// instance (Supabase/Neon) without hardcoding credentials.
inline std::string database_url_from_env() {
    const char* url = std::getenv("DATABASE_URL");
    if (url == nullptr || *url == '\0') {
        throw std::runtime_error("DATABASE_URL environment variable is not set");
    }
    return std::string(url);
}

inline pqxx::connection make_connection() {
    return pqxx::connection(database_url_from_env());
}

} // namespace vitale::shared
