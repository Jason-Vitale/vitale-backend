#include "routes.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <sstream>

#include <pqxx/pqxx>

#include "db_connection.hpp"

namespace vitale::api {

namespace {

constexpr const char* kObjectColumns =
    "norad_cat_id, object_name, object_id, object_type, country_code, "
    "launch_date, site, rcs_size, decay_date, hit_count";

constexpr int kDefaultPopularLimit = 10;
constexpr int kMaxPopularLimit = 100;

// /objects/catalog intentionally omits site/rcs_size (detail-page-only
// fields, see routes.hpp) to keep the full-catalog payload smaller.
constexpr const char* kCatalogColumns =
    "norad_cat_id, object_name, object_id, country_code, object_type, "
    "launch_date, decay_date, hit_count";

// RFC 7231 requires the English weekday/month abbreviations in an HTTP-date
// regardless of process locale -- spelled out here rather than going
// through strftime's locale-dependent %a/%b.
constexpr const char* kWeekdayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr const char* kMonthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

std::string format_http_date(std::time_t t) {
    std::tm tm{};
    gmtime_r(&t, &tm);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT", kWeekdayNames[tm.tm_wday],
                  tm.tm_mday, kMonthNames[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

// Parses the IMF-fixdate form of If-Modified-Since (e.g. "Wed, 23 Aug 2026
// 00:00:00 GMT") -- the format every modern HTTP client/CDN actually sends.
// The two obsolete formats RFC 7231 also allows for historical
// compatibility are not handled; an unparseable header is treated the same
// as a missing one (fail open to a full 200 rather than a wrongly-cached
// 304).
std::optional<std::time_t> parse_http_date(const std::string& value) {
    std::tm tm{};
    char weekday[4] = {};
    char month[4] = {};
    const int parsed = std::sscanf(value.c_str(), "%3s, %2d %3s %4d %2d:%2d:%2d GMT", weekday, &tm.tm_mday,
                                    month, &tm.tm_year, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    if (parsed != 7) {
        return std::nullopt;
    }

    const auto month_it = std::find_if(std::begin(kMonthNames), std::end(kMonthNames),
                                        [&](const char* m) { return std::string(m) == month; });
    if (month_it == std::end(kMonthNames)) {
        return std::nullopt;
    }

    tm.tm_year -= 1900;
    tm.tm_mon = static_cast<int>(month_it - std::begin(kMonthNames));
    return timegm(&tm);
}

// True if `etag` appears (quotes intact, weak-validator "W/" prefix
// stripped) among the comma-separated list in an If-None-Match header, or
// the header is the "*" wildcard.
bool if_none_match_matches(const std::string& header_value, const std::string& etag) {
    if (header_value.empty()) {
        return false;
    }
    if (header_value.find('*') != std::string::npos) {
        return true;
    }

    std::stringstream ss(header_value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const auto begin = token.find_first_not_of(" \t");
        if (begin == std::string::npos) {
            continue;
        }
        const auto end = token.find_last_not_of(" \t");
        std::string trimmed = token.substr(begin, end - begin + 1);
        if (trimmed.rfind("W/", 0) == 0) {
            trimmed = trimmed.substr(2);
        }
        if (trimmed == etag) {
            return true;
        }
    }
    return false;
}

// Sets the caching headers shared by both the 200 and 304 responses for
// /objects/catalog.
void set_catalog_cache_headers(crow::response& res, const std::string& etag, const std::string& last_modified) {
    res.add_header("Cache-Control", "public, max-age=86400");
    res.add_header("ETag", etag);
    res.add_header("Last-Modified", last_modified);
}

// Templated rather than typed to pqxx::row / pqxx::row_ref: libpqxx 8.x's
// result iteration/indexing yields row_ref, but 7.x (e.g. the version
// actually installed on the EC2 deploy target) yields row and has no
// row_ref type at all. Both support the same operator[]/.as<T>() surface,
// so deducing the type keeps this working across both. The `.template`
// disambiguators below are required because `as<T>` is a dependent member
// template call on a template parameter type.
template <typename Row>
crow::json::wvalue object_row_to_json(const Row& row) {
    crow::json::wvalue obj;
    obj["norad_cat_id"] = row["norad_cat_id"].template as<int>();
    obj["object_name"] = row["object_name"].is_null() ? "" : row["object_name"].template as<std::string>();
    obj["object_id"] = row["object_id"].is_null() ? "" : row["object_id"].template as<std::string>();
    obj["object_type"] = row["object_type"].is_null() ? "" : row["object_type"].template as<std::string>();
    obj["country_code"] =
        row["country_code"].is_null() ? "" : row["country_code"].template as<std::string>();
    obj["launch_date"] = row["launch_date"].is_null() ? "" : row["launch_date"].template as<std::string>();
    obj["site"] = row["site"].is_null() ? "" : row["site"].template as<std::string>();
    obj["rcs_size"] = row["rcs_size"].is_null() ? "" : row["rcs_size"].template as<std::string>();
    obj["decay_date"] = row["decay_date"].is_null() ? "" : row["decay_date"].template as<std::string>();
    obj["hit_count"] = row["hit_count"].template as<std::int64_t>();
    return obj;
}

// Like object_row_to_json, but for the /objects/catalog field set (no
// site/rcs_size) -- and, per that endpoint's contract, decay_date is a real
// JSON null when absent rather than "", since a client filtering on "still
// in orbit" wants to check for null directly.
template <typename Row>
crow::json::wvalue catalog_row_to_json(const Row& row) {
    crow::json::wvalue obj;
    obj["norad_cat_id"] = row["norad_cat_id"].template as<int>();
    obj["object_name"] = row["object_name"].is_null() ? "" : row["object_name"].template as<std::string>();
    obj["object_id"] = row["object_id"].is_null() ? "" : row["object_id"].template as<std::string>();
    obj["country_code"] =
        row["country_code"].is_null() ? "" : row["country_code"].template as<std::string>();
    obj["object_type"] = row["object_type"].is_null() ? "" : row["object_type"].template as<std::string>();
    obj["launch_date"] = row["launch_date"].is_null() ? "" : row["launch_date"].template as<std::string>();
    if (row["decay_date"].is_null()) {
        obj["decay_date"] = nullptr;
    } else {
        obj["decay_date"] = row["decay_date"].template as<std::string>();
    }
    obj["hit_count"] = row["hit_count"].template as<std::int64_t>();
    return obj;
}

template <typename Row>
crow::json::wvalue event_row_to_json(const Row& row) {
    crow::json::wvalue event;
    event["id"] = row["id"].template as<std::int64_t>();
    event["event_type_code"] = row["event_type_code"].template as<std::string>();
    event["event_time"] = row["event_time"].template as<std::string>();
    event["detail_json"] = row["detail_json"].is_null() ? "" : row["detail_json"].template as<std::string>();
    return event;
}

// Shared by /objects/<int>/events and /objects/<int>/audt -- same data,
// two route names (the frontend spec uses /audt; /events stays too since
// it's already confirmed working against the deployed instance).
crow::response object_audt_events_response(int norad_cat_id) {
    try {
        auto conn = vitale::shared::make_connection();
        pqxx::work txn(conn);
        const pqxx::result rows = txn.exec(
            "SELECT id, event_type_code, event_time, detail_json FROM audt_events "
            "WHERE norad_cat_id = $1 ORDER BY event_time DESC",
            pqxx::params{norad_cat_id});

        crow::json::wvalue::list events;
        for (const auto& row : rows) {
            events.push_back(event_row_to_json(row));
        }
        crow::json::wvalue response;
        response["norad_cat_id"] = norad_cat_id;
        response["events"] = std::move(events);
        return crow::response(response);
    } catch (const std::exception& e) {
        crow::json::wvalue error;
        error["error"] = e.what();
        return crow::response(500, error);
    }
}

} // namespace

void register_routes(ApiApp& app) {
    CROW_ROUTE(app, "/objects")
    ([]() {
        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);
            const pqxx::result rows = txn.exec(
                "SELECT " + std::string(kObjectColumns) + " FROM objects ORDER BY norad_cat_id");

            crow::json::wvalue::list objects;
            for (const auto& row : rows) {
                objects.push_back(object_row_to_json(row));
            }
            crow::json::wvalue response;
            response["objects"] = std::move(objects);
            return crow::response(response);
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    // Registered before /objects/<int> so a literal "search"/"popular"
    // segment can't be shadowed by the <int> parameter route, even though
    // Crow's <int> matcher wouldn't accept a non-numeric segment anyway --
    // keep it explicit rather than relying on that.
    CROW_ROUTE(app, "/objects/search")
    ([](const crow::request& req) {
        const char* q = req.url_params.get("q");
        if (q == nullptr || *q == '\0') {
            crow::json::wvalue error;
            error["error"] = "missing required query parameter 'q'";
            return crow::response(400, error);
        }

        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);
            // Requires db/migrations/002_add_search_index.sql (pg_trgm +
            // the trigram index on object_name) to already be applied --
            // without it this query still runs, just as an unindexed
            // sequential scan (or fails outright if pg_trgm isn't enabled,
            // since `%` and similarity() aren't defined without it).
            //
            // COUNT(*) OVER() rides along in the same query/round-trip as
            // the LIMIT 20 page -- it's the count of every matching row
            // before the limit is applied, not just the 20 returned. Short
            // or broad queries (a single letter, say) can match thousands
            // via the ILIKE prefix clause alone, so the frontend needs this
            // to show "20 of 4,213" rather than silently truncating.
            const pqxx::result rows = txn.exec(
                "SELECT " + std::string(kObjectColumns) +
                    ", COUNT(*) OVER() AS total_matches "
                    "FROM objects "
                    "WHERE object_name ILIKE $1 || '%' "
                    "   OR object_name % $1 "
                    "   OR norad_cat_id::text = $1 "
                    "ORDER BY similarity(object_name, $1) DESC LIMIT 20",
                pqxx::params{std::string(q)});

            crow::json::wvalue::list objects;
            for (const auto& row : rows) {
                objects.push_back(object_row_to_json(row));
            }
            crow::json::wvalue response;
            response["objects"] = std::move(objects);
            response["total_matches"] = rows.empty() ? 0 : rows[0]["total_matches"].as<std::int64_t>();
            return crow::response(response);
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    CROW_ROUTE(app, "/objects/popular")
    ([](const crow::request& req) {
        int limit = kDefaultPopularLimit;
        if (const char* limit_param = req.url_params.get("limit"); limit_param != nullptr) {
            limit = std::atoi(limit_param);
            if (limit <= 0) {
                crow::json::wvalue error;
                error["error"] = "'limit' must be a positive integer";
                return crow::response(400, error);
            }
            limit = std::min(limit, kMaxPopularLimit);
        }

        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);
            // hit_count DESC is the ranking; norad_cat_id ASC is only a
            // deterministic tie-break, not a meaningful secondary sort --
            // with ~35k objects starting at hit_count 0, this list will be
            // padded past however many have real hits with the
            // lowest-numbered (oldest-catalogued) never-viewed objects,
            // not anything actually "popular". Accepted tradeoff: always
            // returns exactly `limit` results rather than fewer while
            // traffic is still low.
            const pqxx::result rows = txn.exec(
                "SELECT " + std::string(kObjectColumns) +
                    " FROM objects ORDER BY hit_count DESC, norad_cat_id ASC LIMIT $1",
                pqxx::params{limit});

            crow::json::wvalue::list objects;
            for (const auto& row : rows) {
                objects.push_back(object_row_to_json(row));
            }
            crow::json::wvalue response;
            response["objects"] = std::move(objects);
            return crow::response(response);
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    // Registered before /objects/<int> for the same reason as /search and
    // /popular above.
    CROW_ROUTE(app, "/objects/catalog")
    ([](const crow::request& req) {
        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);

            // Cheap first: just the generation timestamp (the satcat
            // poller's last recorded run, from poller_state -- this is a
            // "when did the catalog import last run" snapshot, not a
            // per-row last-write time) and the row count, not the full
            // catalog. If the client's cached copy is still current per
            // If-None-Match/If-Modified-Since, this is all that's needed
            // to answer with a 304 and skip building/serializing the
            // entire payload below.
            const pqxx::result meta_rows = txn.exec(
                "SELECT to_char(gen_ts AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS generated_at, "
                "       EXTRACT(EPOCH FROM gen_ts)::bigint AS generated_at_epoch, "
                "       (SELECT count(*) FROM objects) AS object_count "
                "FROM (SELECT COALESCE((SELECT last_run_at FROM poller_state WHERE poller_name = 'satcat'), "
                "                       now()) AS gen_ts) sub");

            const auto meta = meta_rows[0];
            const std::string generated_at = meta["generated_at"].as<std::string>();
            const std::time_t generated_at_epoch =
                static_cast<std::time_t>(meta["generated_at_epoch"].as<std::int64_t>());
            const std::int64_t object_count = meta["object_count"].as<std::int64_t>();

            // Strong validator: changes exactly when generated_at or the
            // row count changes, both of which only happen on a SatcatPoller
            // run (see services/poller/db_writer.cpp).
            std::ostringstream etag_stream;
            etag_stream << '"' << generated_at << '-' << object_count << '"';
            const std::string etag = etag_stream.str();
            const std::string last_modified = format_http_date(generated_at_epoch);

            bool not_modified = false;
            const std::string if_none_match = req.get_header_value("If-None-Match");
            if (!if_none_match.empty()) {
                // If-None-Match takes precedence over If-Modified-Since
                // when both are present, per RFC 7232 §3.3.
                not_modified = if_none_match_matches(if_none_match, etag);
            } else if (const std::string if_modified_since = req.get_header_value("If-Modified-Since");
                       !if_modified_since.empty()) {
                if (const auto since = parse_http_date(if_modified_since)) {
                    not_modified = generated_at_epoch <= *since;
                }
            }

            if (not_modified) {
                crow::response res(304);
                set_catalog_cache_headers(res, etag, last_modified);
                return res;
            }

            const pqxx::result rows =
                txn.exec("SELECT " + std::string(kCatalogColumns) + " FROM objects ORDER BY norad_cat_id");

            crow::json::wvalue::list objects;
            objects.reserve(rows.size());
            for (const auto& row : rows) {
                objects.push_back(catalog_row_to_json(row));
            }

            crow::json::wvalue response;
            response["generated_at"] = generated_at;
            response["count"] = object_count;
            response["objects"] = std::move(objects);

            crow::response res(response);
            set_catalog_cache_headers(res, etag, last_modified);
            return res;
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    CROW_ROUTE(app, "/objects/<int>")
    ([](int norad_cat_id) {
        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);
            // Increment and fetch atomically in one round trip, rather than
            // a plain SELECT -- this is the one endpoint that counts as a
            // "hit" toward an object's popularity.
            const pqxx::result rows = txn.exec(
                "UPDATE objects SET hit_count = hit_count + 1 "
                "WHERE norad_cat_id = $1 RETURNING " +
                    std::string(kObjectColumns),
                pqxx::params{norad_cat_id});

            if (rows.empty()) {
                txn.commit();
                crow::json::wvalue error;
                error["error"] = "object not found";
                return crow::response(404, error);
            }

            auto json = object_row_to_json(rows[0]);
            txn.commit();
            return crow::response(json);
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    CROW_ROUTE(app, "/stats")
    ([]() {
        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);
            const pqxx::result rows = txn.exec("SELECT count(*) FROM objects");

            crow::json::wvalue response;
            response["tracked_objects"] = rows[0][0].as<std::int64_t>();
            return crow::response(response);
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    CROW_ROUTE(app, "/objects/<int>/events")
    ([](int norad_cat_id) { return object_audt_events_response(norad_cat_id); });

    CROW_ROUTE(app, "/objects/<int>/audt")
    ([](int norad_cat_id) { return object_audt_events_response(norad_cat_id); });
}

} // namespace vitale::api
