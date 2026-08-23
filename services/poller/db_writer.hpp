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

    // Returns the most recent stored snapshot for this object (joined with
    // the object's current decay_date), or nullopt if this object has never
    // been polled before. Callers must call this BEFORE upsert_object() for
    // the same poll, since the returned decay_date reflects the
    // pre-upsert state -- that's what makes it a valid "prev" snapshot for
    // DecayDetectedRule to compare against the freshly fetched "curr" one.
    //
    // Also used to implement the dedup rule: skip the insert entirely if
    // the freshly fetched gp_id matches this snapshot's gp_id.
    std::optional<rule_engine::Snapshot> get_last_snapshot(int norad_cat_id);

    // Inserts a new snapshot row and returns its generated id. Callers are
    // expected to have already checked get_last_snapshot()'s gp_id to avoid
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
