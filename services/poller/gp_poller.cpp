#include "gp_poller.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "gp_record.hpp"

namespace vitale::poller {

GpPoller::GpPoller(SpaceTrackClient& client, pqxx::connection& conn, std::vector<std::int64_t> target_norad_ids)
    : Poller(client), writer_(conn), registry_(rule_engine::make_default_rule_registry()),
      target_norad_ids_(std::move(target_norad_ids)) {}

std::string GpPoller::build_query_url() const {
    if (target_norad_ids_.empty()) {
        throw std::runtime_error("GpPoller has no target NORAD IDs configured");
    }

    std::ostringstream ids;
    for (std::size_t i = 0; i < target_norad_ids_.size(); ++i) {
        if (i > 0) {
            ids << ',';
        }
        ids << target_norad_ids_[i];
    }

    // Confirmed against Space-Track docs: every target ID is batched into
    // this ONE query -- the gp class's 1 request/hour limit is per request,
    // not per object, and per-satellite request loops are explicitly
    // prohibited. Batch size (see main.cpp's kGpRotationBatchSize) is 500,
    // matching the empirically-found reliable ceiling other Space-Track
    // consumers (e.g. IBM's spacetech-ssa) have documented -- Space-Track's
    // own docs still don't state a hard max, and support hasn't answered on
    // it either, so this is evidence-based rather than confirmed.
    //
    // No epoch filter: unlike a hand-picked always-actively-tracked
    // watchlist, a rotation batch can contain objects whose latest elements
    // are weeks old, and Space-Track's epoch predicate doesn't return
    // "old data" for those -- it returns nothing at all for that id. The
    // existing gp_id dedup in process_response() already makes re-seeing
    // unchanged old elements a no-op, so there's no downside to just taking
    // whatever the latest elements are, however old.
    std::ostringstream url;
    url << "https://www.space-track.org/basicspacedata/query/class/gp/NORAD_CAT_ID/" << ids.str()
        << "/decay_date/null-val/orderby/NORAD_CAT_ID/format/json";
    return url.str();
}

void GpPoller::process_response(const std::string& json_body) {
    const std::vector<GpRecord> records = parse_gp_response(json_body);

    // Logged unconditionally, before touching any individual record: this is
    // the one line that distinguishes "the request only asked for N objects"
    // from "Space-Track's response silently dropped some of the N we asked
    // for" (e.g. objects whose most recent TLE falls outside the epoch
    // filter) from "the per-record loop below never ran".
    std::cout << "gp poller: requested " << target_norad_ids_.size() << " target(s), received "
              << records.size() << " record(s) from Space-Track\n";

    // Per-object status (checking/unchanged/inserted) is deliberately NOT
    // logged individually here, unlike the old 12-object watchlist -- at up
    // to 500 objects/hour that would be 500+ routine lines every run.
    // Fired events and per-record errors still get their own line each:
    // both are rare and worth seeing individually. Routine outcomes are
    // rolled into the one summary line after the loop instead.
    int inserted_count = 0;
    int unchanged_count = 0;
    int error_count = 0;

    for (const auto& record : records) {
        try {
            const auto prev = writer_.get_last_snapshot(record.object.norad_cat_id);

            if (prev && prev->gp_id == record.snapshot.gp_id) {
                ++unchanged_count;
                continue;
            }

            const std::int64_t new_snapshot_id = writer_.insert_snapshot(record.snapshot);
            ++inserted_count;

            if (prev) {
                const auto events = registry_.evaluate_all(*prev, record.snapshot);
                for (const auto& event : events) {
                    writer_.insert_event(record.object.norad_cat_id, event, prev->id, new_snapshot_id);
                    std::cout << "gp poller: norad " << record.object.norad_cat_id
                              << " -> fired event: " << event.event_type_code << '\n';
                }
            }
        } catch (const std::exception& e) {
            ++error_count;
            std::cerr << "gp poller: failed to process norad " << record.object.norad_cat_id << ": "
                      << e.what() << '\n';
        }
    }

    std::cout << "gp poller: processed " << records.size() << " record(s): " << inserted_count
              << " inserted, " << unchanged_count << " unchanged, " << error_count << " error(s)\n";

    // Advances the rotation cursor only once we've actually heard back from
    // Space-Track for this batch (reaching here requires build_query_url()
    // and the fetch to have both succeeded, so target_norad_ids_ is
    // guaranteed non-empty) -- a failed request never reaches this line, so
    // it can't silently skip a chunk of the catalog. A per-record exception
    // above doesn't block this either: that's an isolated bad record, not a
    // reason to keep re-requesting the same batch forever.
    const std::int64_t new_cursor = target_norad_ids_.back();
    writer_.advance_gp_rotation_cursor(new_cursor);
    std::cout << "gp poller: rotation cursor advanced to norad " << new_cursor << '\n';
}

} // namespace vitale::poller
