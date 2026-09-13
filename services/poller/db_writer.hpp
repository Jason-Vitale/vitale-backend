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
    //
    // Applies a small fixed tolerance (5 minutes) below `pg_interval` internally (see
    // .cpp) so that ordinary per-run overhead -- the DB round trips for
    // this check and mark_poller_run() itself, which land last_run_at some
    // tens to hundreds of ms after cron actually fired -- can't push a
    // legitimately-due poller just past the threshold. Without it, an
    // hourly poller reliably fires every OTHER hour instead of every hour:
    // each run's last_run_at drifts slightly later (in wall-clock terms)
    // than the previous one, so the very next hourly check always misses
    // by that same small margin, then the one after that is two hours
    // overdue and clears it easily -- observed in production.
    bool is_poller_due(const std::string& poller_name, const std::string& pg_interval);

    // Records that `poller_name` just ran, right now. Call this
    // unconditionally after run() returns, even if that run failed
    // internally (Poller::run() logs and swallows its own errors) --
    // otherwise a persistent auth/network failure would retry every cron
    // tick instead of waiting out its normal interval like a healthy run
    // would, which risks the same rate-limit problem this table exists to
    // prevent.
    void mark_poller_run(const std::string& poller_name);

    // Returns the next `batch_size` NORAD IDs for GpPoller's rotating sweep
    // through the full `objects` catalog, continuing from wherever the last
    // successfully-processed batch left off (poller_state.rotation_cursor
    // for poller_name = 'gp'; NULL means start from the beginning).
    // Excludes already-decayed objects (decay_date IS NULL) -- polling GP
    // data for something that no longer orbits can't return anything
    // useful. Wraps around to the start of the catalog if fewer than
    // batch_size objects remain past the cursor, so this is a continuous
    // cycle rather than something that stops at the end of the table.
    //
    // Read-only: does not itself advance the cursor. Call
    // advance_gp_rotation_cursor() only after the batch this returns has
    // actually been successfully processed, so a failed request doesn't
    // silently skip a chunk of the catalog forever.
    //
    // `exclude_ids` (typically the batch just returned by
    // top_gp_hot_targets()) is subtracted from both the primary and
    // wraparound selects, so an object already covered by the hot slice
    // this run doesn't also consume a rotation slot -- every requested id
    // stays unique without the caller needing to dedup the two lists
    // itself.
    std::vector<std::int64_t> next_gp_rotation_batch(int batch_size, const std::vector<std::int64_t>& exclude_ids);

    // Persists `new_cursor` (the highest norad_cat_id covered by the batch
    // just processed, in rotation order -- i.e. the last element of what
    // next_gp_rotation_batch() returned) as the rotation's starting point
    // for next time.
    void advance_gp_rotation_cursor(std::int64_t new_cursor);

    // Returns up to `limit` NORAD IDs to always include in GpPoller's
    // hourly "hot" slice: objects marked `featured` (a curated, hand-
    // verified seed list -- see db/migrations/007_add_featured_objects.sql)
    // or with real hit_count > 0 (actual site traffic), ranked featured
    // first, then by hit_count descending.
    //
    // Deliberately NOT padded to `limit` with never-viewed, non-featured
    // objects the way /objects/popular's ranking is (see that endpoint's
    // comment in routes.cpp for why padding with arbitrary low-numbered
    // catalog entries isn't real "hot") -- returning fewer than `limit`
    // just means next_gp_rotation_batch() below fills the rest of the
    // request from the ordinary rotation instead.
    std::vector<std::int64_t> top_gp_hot_targets(int limit);

private:
    pqxx::connection& conn_;
};

} // namespace vitale::poller
