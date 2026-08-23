#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <pqxx/pqxx>

#include "object_rule_registry.hpp"
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

// Owns the write path into Postgres shared by both pollers: SatcatPoller
// calls upsert_object_from_satcat(); GpPoller calls get_last_snapshot(),
// insert_snapshot(), and insert_event(). Only GpPoller touches snapshots/
// audt_events; only SatcatPoller touches objects. The api service never
// uses this class -- it is read-only and talks to Postgres directly.
//
// Also backs the scheduler in main.cpp via is_poller_due()/mark_poller_run()
// -- the poller is deployed as an hourly cron job (a fresh process each
// time), not one long-lived process, so "when did each poller last run"
// has to live in Postgres, not in an in-memory variable that resets every
// invocation.
class DbWriter {
public:
    explicit DbWriter(pqxx::connection& conn);

    // Upserts catalog metadata from a full SATCAT response in ONE
    // transaction (one commit / WAL flush for the whole batch, not one per
    // row) -- a live run against ~20k+ active objects with a
    // commit-per-row implementation took over 20 minutes; this is the fix.
    // Trade-off: if any row fails, the whole batch rolls back rather than
    // partially applying -- acceptable given SatcatPoller runs daily and
    // will simply retry the full catalog next cycle.
    //
    // Before each row is upserted, the existing `objects` row (if any) is
    // fetched and diffed against the incoming one via `registry`; any fired
    // events are written to audt_events in the same transaction as the
    // upsert batch. Refreshes object_name/object_type/rcs_size/decay_date/
    // updated_at on conflict -- object_id, country_code, launch_date, site
    // are still treated as fixed facts set on first sighting, since no rule
    // tracks changes to them.
    //
    // Returns every event fired across the whole batch (for logging by the
    // caller); on failure, no events fired and nothing else in the batch is
    // written either, since the whole thing is one transaction.
    std::vector<rule_engine::DetectedEvent> upsert_objects_from_satcat(
        const std::vector<ObjectRecord>& objs, const rule_engine::ObjectRuleRegistry& registry);

    // Returns the most recent stored snapshot for this object (joined with
    // the object's current decay_date), or nullopt if this object has never
    // been polled before. Used both as the "prev" snapshot for rule
    // evaluation and, via its gp_id, to implement the dedup rule: skip the
    // insert entirely if the freshly fetched gp_id matches.
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

    // Returns true if `poller_name` (e.g. "satcat", "gp") has never
    // recorded a run, or if at least `pg_interval` -- a Postgres interval
    // literal such as "1 hour" or "24 hours" -- has elapsed since its last
    // recorded run. The interval math happens server-side in Postgres
    // rather than by parsing a timestamp back into C++, since that's what
    // Postgres is actually good at and it sidesteps any cross-platform
    // chrono-parsing portability questions.
    bool is_poller_due(const std::string& poller_name, const std::string& pg_interval);

    // Records that `poller_name` just ran, right now. Call this
    // unconditionally after run() returns, even if that run failed
    // internally (Poller::run() logs and swallows its own errors) --
    // otherwise a persistent auth/network failure would retry every cron
    // tick instead of waiting out its normal interval like a healthy run
    // would, which risks the same rate-limit problem this table exists to
    // prevent.
    void mark_poller_run(const std::string& poller_name);

private:
    pqxx::connection& conn_;
};

} // namespace vitale::poller
