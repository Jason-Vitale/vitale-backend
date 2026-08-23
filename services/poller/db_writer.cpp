#include "db_writer.hpp"

namespace vitale::poller {

DbWriter::DbWriter(pqxx::connection& conn) : conn_(conn) {}

std::vector<rule_engine::DetectedEvent> DbWriter::upsert_objects_from_satcat(
    const std::vector<ObjectRecord>& objs, const rule_engine::ObjectRuleRegistry& registry) {
    if (objs.empty()) {
        return {};
    }

    pqxx::work txn(conn_);
    std::vector<rule_engine::DetectedEvent> fired_events;

    for (const auto& obj : objs) {
        const pqxx::result prev_rows = txn.exec(
            "SELECT object_name, object_type, rcs_size, decay_date FROM objects WHERE norad_cat_id = $1",
            pqxx::params{obj.norad_cat_id});

        if (!prev_rows.empty()) {
            const auto prev_row = prev_rows[0];

            rule_engine::ObjectState prev_state;
            prev_state.norad_cat_id = obj.norad_cat_id;
            prev_state.object_name = prev_row["object_name"].is_null() ? "" : prev_row["object_name"].as<std::string>();
            prev_state.object_type = prev_row["object_type"].is_null() ? "" : prev_row["object_type"].as<std::string>();
            prev_state.rcs_size = prev_row["rcs_size"].is_null() ? "" : prev_row["rcs_size"].as<std::string>();
            if (!prev_row["decay_date"].is_null()) {
                prev_state.decay_date = prev_row["decay_date"].as<std::string>();
            }

            rule_engine::ObjectState curr_state;
            curr_state.norad_cat_id = obj.norad_cat_id;
            curr_state.object_name = obj.object_name;
            curr_state.object_type = obj.object_type;
            curr_state.rcs_size = obj.rcs_size;
            curr_state.decay_date = obj.decay_date;

            for (auto& event : registry.evaluate_all(prev_state, curr_state)) {
                // SATCAT-derived events aren't tied to a snapshot -- both
                // snapshot id columns are left null.
                txn.exec(
                    "INSERT INTO audt_events (norad_cat_id, event_type_code, event_time, detail_json) "
                    "VALUES ($1, $2, now(), $3::jsonb)",
                    pqxx::params{obj.norad_cat_id, event.event_type_code, event.detail_json});
                fired_events.push_back(std::move(event));
            }
        }

        txn.exec(
            "INSERT INTO objects "
            "(norad_cat_id, object_name, object_type, country_code, launch_date, site, rcs_size, decay_date, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, now()) "
            "ON CONFLICT (norad_cat_id) DO UPDATE SET "
            "object_name = EXCLUDED.object_name, "
            "object_type = EXCLUDED.object_type, "
            "rcs_size = EXCLUDED.rcs_size, "
            "decay_date = EXCLUDED.decay_date, "
            "updated_at = now()",
            pqxx::params{obj.norad_cat_id, obj.object_name, obj.object_type, obj.country_code,
                         obj.launch_date, obj.site, obj.rcs_size, obj.decay_date});
    }
    txn.commit();

    return fired_events;
}

std::optional<rule_engine::Snapshot> DbWriter::get_last_snapshot(int norad_cat_id) {
    pqxx::work txn(conn_);
    const pqxx::result rows = txn.exec(
        "SELECT s.id, s.gp_id, s.epoch, s.mean_motion, s.eccentricity, s.inclination, "
        "       s.ra_of_asc_node, s.arg_of_pericenter, s.mean_anomaly, s.semimajor_axis, "
        "       s.period, s.apoapsis, s.periapsis, s.bstar, s.mean_motion_dot, "
        "       s.mean_motion_ddot, s.element_set_no, s.rev_at_epoch, s.tle_line0, "
        "       s.tle_line1, s.tle_line2, o.decay_date "
        "FROM snapshots s JOIN objects o ON o.norad_cat_id = s.norad_cat_id "
        "WHERE s.norad_cat_id = $1 ORDER BY s.epoch DESC LIMIT 1",
        pqxx::params{norad_cat_id});
    txn.commit();

    if (rows.empty()) {
        return std::nullopt;
    }

    // auto rather than pqxx::row_ref: that type doesn't exist in libpqxx
    // 7.x (e.g. the version on the EC2 deploy target) -- result::operator[]
    // returns pqxx::row there instead. Both support the same
    // operator[]/.as<T>() surface used below.
    const auto row = rows[0];
    rule_engine::Snapshot snap;
    snap.id = row["id"].as<std::int64_t>();
    snap.norad_cat_id = norad_cat_id;
    snap.gp_id = row["gp_id"].as<std::int64_t>();
    snap.epoch = row["epoch"].as<std::string>();
    snap.mean_motion = row["mean_motion"].as<double>();
    snap.eccentricity = row["eccentricity"].as<double>();
    snap.inclination = row["inclination"].as<double>();
    snap.ra_of_asc_node = row["ra_of_asc_node"].as<double>();
    snap.arg_of_pericenter = row["arg_of_pericenter"].as<double>();
    snap.mean_anomaly = row["mean_anomaly"].as<double>();
    snap.semimajor_axis = row["semimajor_axis"].as<double>();
    snap.period = row["period"].as<double>();
    snap.apoapsis = row["apoapsis"].as<double>();
    snap.periapsis = row["periapsis"].as<double>();
    snap.bstar = row["bstar"].as<double>();
    snap.mean_motion_dot = row["mean_motion_dot"].as<double>();
    snap.mean_motion_ddot = row["mean_motion_ddot"].as<double>();
    snap.element_set_no = row["element_set_no"].as<int>();
    snap.rev_at_epoch = row["rev_at_epoch"].as<int>();
    snap.tle_line0 = row["tle_line0"].as<std::string>();
    snap.tle_line1 = row["tle_line1"].as<std::string>();
    snap.tle_line2 = row["tle_line2"].as<std::string>();
    if (!row["decay_date"].is_null()) {
        snap.decay_date = row["decay_date"].as<std::string>();
    }
    return snap;
}

std::int64_t DbWriter::insert_snapshot(const rule_engine::Snapshot& snap) {
    pqxx::work txn(conn_);
    const pqxx::result inserted = txn.exec(
        "INSERT INTO snapshots "
        "(norad_cat_id, gp_id, epoch, mean_motion, eccentricity, inclination, ra_of_asc_node, "
        " arg_of_pericenter, mean_anomaly, semimajor_axis, period, apoapsis, periapsis, bstar, "
        " mean_motion_dot, mean_motion_ddot, element_set_no, rev_at_epoch, tle_line0, tle_line1, tle_line2) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20, $21) "
        "RETURNING id",
        pqxx::params{snap.norad_cat_id, snap.gp_id, snap.epoch, snap.mean_motion, snap.eccentricity,
                     snap.inclination, snap.ra_of_asc_node, snap.arg_of_pericenter, snap.mean_anomaly,
                     snap.semimajor_axis, snap.period, snap.apoapsis, snap.periapsis, snap.bstar,
                     snap.mean_motion_dot, snap.mean_motion_ddot, snap.element_set_no, snap.rev_at_epoch,
                     snap.tle_line0, snap.tle_line1, snap.tle_line2});
    txn.commit();

    return inserted.one_row()[0].as<std::int64_t>();
}

void DbWriter::insert_event(
    int norad_cat_id,
    const rule_engine::DetectedEvent& event,
    std::optional<std::int64_t> prev_snapshot_id,
    std::int64_t new_snapshot_id) {
    pqxx::work txn(conn_);
    txn.exec(
        "INSERT INTO audt_events "
        "(norad_cat_id, event_type_code, event_time, detail_json, prev_snapshot_id, new_snapshot_id) "
        "VALUES ($1, $2, now(), $3::jsonb, $4, $5)",
        pqxx::params{norad_cat_id, event.event_type_code, event.detail_json, prev_snapshot_id, new_snapshot_id});
    txn.commit();
}

bool DbWriter::is_poller_due(const std::string& poller_name, const std::string& pg_interval) {
    pqxx::work txn(conn_);
    const pqxx::result rows = txn.exec(
        "SELECT (last_run_at <= now() - $2::interval) FROM poller_state WHERE poller_name = $1",
        pqxx::params{poller_name, pg_interval});
    txn.commit();

    if (rows.empty()) {
        return true; // never recorded a run before
    }
    return rows[0][0].as<bool>();
}

void DbWriter::mark_poller_run(const std::string& poller_name) {
    pqxx::work txn(conn_);
    txn.exec(
        "INSERT INTO poller_state (poller_name, last_run_at) VALUES ($1, now()) "
        "ON CONFLICT (poller_name) DO UPDATE SET last_run_at = now()",
        pqxx::params{poller_name});
    txn.commit();
}

} // namespace vitale::poller
