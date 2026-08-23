#include "routes.hpp"

#include <pqxx/pqxx>

#include "db_connection.hpp"

namespace vitale::api {

namespace {

crow::json::wvalue object_row_to_json(const pqxx::row_ref& row) {
    crow::json::wvalue obj;
    obj["norad_cat_id"] = row["norad_cat_id"].as<int>();
    obj["object_name"] = row["object_name"].is_null() ? "" : row["object_name"].as<std::string>();
    obj["object_id"] = row["object_id"].is_null() ? "" : row["object_id"].as<std::string>();
    obj["object_type"] = row["object_type"].is_null() ? "" : row["object_type"].as<std::string>();
    obj["country_code"] = row["country_code"].is_null() ? "" : row["country_code"].as<std::string>();
    obj["launch_date"] = row["launch_date"].is_null() ? "" : row["launch_date"].as<std::string>();
    obj["site"] = row["site"].is_null() ? "" : row["site"].as<std::string>();
    obj["rcs_size"] = row["rcs_size"].is_null() ? "" : row["rcs_size"].as<std::string>();
    obj["decay_date"] = row["decay_date"].is_null() ? "" : row["decay_date"].as<std::string>();
    return obj;
}

crow::json::wvalue event_row_to_json(const pqxx::row_ref& row) {
    crow::json::wvalue event;
    event["id"] = row["id"].as<std::int64_t>();
    event["event_type_code"] = row["event_type_code"].as<std::string>();
    event["event_time"] = row["event_time"].as<std::string>();
    event["detail_json"] = row["detail_json"].is_null() ? "" : row["detail_json"].as<std::string>();
    return event;
}

} // namespace

void register_routes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/objects")
    ([]() {
        try {
            auto conn = vitale::shared::make_connection();
            pqxx::work txn(conn);
            const pqxx::result rows = txn.exec(
                "SELECT norad_cat_id, object_name, object_id, object_type, country_code, "
                "launch_date, site, rcs_size, decay_date FROM objects ORDER BY norad_cat_id");

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

    CROW_ROUTE(app, "/objects/<int>/events")
    ([](int norad_cat_id) {
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
    });
}

} // namespace vitale::api
