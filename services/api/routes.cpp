#include "routes.hpp"

#include <algorithm>
#include <cstdlib>

#include <pqxx/pqxx>

#include "db_connection.hpp"

namespace vitale::api {

namespace {

constexpr const char* kObjectColumns =
    "norad_cat_id, object_name, object_id, object_type, country_code, "
    "launch_date, site, rcs_size, decay_date, hit_count";

constexpr int kDefaultPopularLimit = 10;
constexpr int kMaxPopularLimit = 100;

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
