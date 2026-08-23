#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <pqxx/pqxx>

#include "rule.hpp"
#include "snapshot.hpp"

namespace vitale::poller {

// Mirrors one row of the static `objects` table.
struct ObjectRecord {
    int norad_cat_id = 0;
    std::string object_name;
    std::string object_id;
    std::string object_type;
    std::string country_code;
    std::optional<std::string> launch_date;
    std::string site;
    std::string rcs_size;
    std::optional<std::string> decay_date;
};

// Owns the write path into Postgres for the poller: upserting object
// metadata, inserting new snapshots (with dedup on gp_id), and recording
// AUDT events produced by the rule engine. The api service never uses this
// class -- it is read-only and talks to Postgres directly.
class DbWriter {
public:
    explicit DbWriter(pqxx::connection& conn);

    void upsert_object(const ObjectRecord& obj);

    // Returns the gp_id of the most recent stored snapshot for this object,
    // or nullopt if no snapshot exists yet. Used to implement the dedup
    // rule: skip the insert entirely if the fetched gp_id matches.
    std::optional<std::int64_t> last_snapshot_gp_id(int norad_cat_id);

    // Inserts a new snapshot row and returns its generated id. Callers are
    // expected to have already checked last_snapshot_gp_id() to avoid
    // inserting a duplicate.
    std::int64_t insert_snapshot(const rule_engine::Snapshot& snap);

    void insert_event(
        int norad_cat_id,
        const rule_engine::DetectedEvent& event,
        std::optional<std::int64_t> prev_snapshot_id,
        std::int64_t new_snapshot_id);

private:
    pqxx::connection& conn_;
};

} // namespace vitale::poller
