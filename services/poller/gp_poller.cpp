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
    // prohibited. If the target list ever grows large enough to hit URL
    // length limits, this needs to chunk into a small number of batched
    // requests -- no documented max batch size exists, so treat that
    // threshold as untested/TBD rather than guessing a number now.
    std::ostringstream url;
    url << "https://www.space-track.org/basicspacedata/query/class/gp/NORAD_CAT_ID/" << ids.str()
        << "/decay_date/null-val/epoch/%3Enow-10/orderby/NORAD_CAT_ID/format/json";
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

    for (const auto& record : records) {
        std::cout << "gp poller: norad " << record.object.norad_cat_id << ": checking\n";
        try {
            const auto prev = writer_.get_last_snapshot(record.object.norad_cat_id);

            if (prev && prev->gp_id == record.snapshot.gp_id) {
                std::cout << "gp poller: norad " << record.object.norad_cat_id
                          << ": gp_id unchanged, skipping (dedup)\n";
                continue;
            }

            const std::int64_t new_snapshot_id = writer_.insert_snapshot(record.snapshot);
            std::cout << "gp poller: norad " << record.object.norad_cat_id
                      << ": inserted snapshot id=" << new_snapshot_id << '\n';

            if (prev) {
                const auto events = registry_.evaluate_all(*prev, record.snapshot);
                for (const auto& event : events) {
                    writer_.insert_event(record.object.norad_cat_id, event, prev->id, new_snapshot_id);
                    std::cout << "  -> fired event: " << event.event_type_code << '\n';
                }
            } else {
                std::cout << "  (first observation of this object, no rules evaluated)\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "gp poller: failed to process norad " << record.object.norad_cat_id << ": "
                      << e.what() << '\n';
        }
    }
}

} // namespace vitale::poller
