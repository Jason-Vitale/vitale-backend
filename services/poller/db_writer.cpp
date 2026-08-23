#include "db_writer.hpp"

namespace vitale::poller {

DbWriter::DbWriter(pqxx::connection& conn) : conn_(conn) {}

void DbWriter::upsert_object(const ObjectRecord& obj) {
    pqxx::work txn(conn_);
    txn.exec(
        "INSERT INTO objects "
        "(norad_cat_id, object_name, object_id, object_type, country_code, launch_date, site, rcs_size, decay_date, updated_at) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, now()) "
        "ON CONFLICT (norad_cat_id) DO UPDATE SET "
        "object_name = EXCLUDED.object_name, "
        "object_id = EXCLUDED.object_id, "
        "object_type = EXCLUDED.object_type, "
        "country_code = EXCLUDED.country_code, "
        "launch_date = EXCLUDED.launch_date, "
        "site = EXCLUDED.site, "
        "rcs_size = EXCLUDED.rcs_size, "
        "decay_date = EXCLUDED.decay_date, "
        "updated_at = now()",
        pqxx::params{obj.norad_cat_id, obj.object_name, obj.object_id, obj.object_type, obj.country_code,
                     obj.launch_date, obj.site, obj.rcs_size, obj.decay_date});
    txn.commit();
}

std::optional<std::int64_t> DbWriter::last_snapshot_gp_id(int norad_cat_id) {
    pqxx::work txn(conn_);
    const pqxx::result rows = txn.exec(
        "SELECT gp_id FROM snapshots WHERE norad_cat_id = $1 ORDER BY epoch DESC LIMIT 1",
        pqxx::params{norad_cat_id});
    txn.commit();

    if (rows.empty()) {
        return std::nullopt;
    }
    return rows[0][0].as<std::int64_t>();
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

} // namespace vitale::poller
