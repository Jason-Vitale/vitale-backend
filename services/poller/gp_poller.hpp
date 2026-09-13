#pragma once

#include <cstdint>
#include <vector>

#include <pqxx/pqxx>

#include "db_writer.hpp"
#include "poller_base.hpp"
#include "rule_registry.hpp"

namespace vitale::poller {

// The only poller that touches the rule engine: writes new snapshots
// (deduped on gp_id), diffs each against the last known snapshot for that
// object, and writes any resulting audt_events. Never writes to `objects`
// -- that's SatcatPoller's job; GpPoller assumes the object row already
// exists (FK on snapshots.norad_cat_id).
class GpPoller : public Poller {
public:
    // hot_target_ids is DbWriter::top_gp_hot_targets() -- a fixed
    // watchlist of featured/high-traffic objects re-requested every run --
    // and rotation_target_ids is one batch of DbWriter::
    // next_gp_rotation_batch() (see main.cpp), continuing the sweep
    // through the rest of the `objects` catalog. Kept as two separate
    // vectors, not merged into one: process_response() advances the
    // rotation cursor using only rotation_target_ids's last (highest) id,
    // which would be wrong if hot ids (scattered arbitrarily across the
    // catalog by popularity/curation, not by norad_cat_id order) were
    // mixed in. Callers own fetching both batches and constructing a fresh
    // GpPoller with them each run.
    GpPoller(SpaceTrackClient& client, pqxx::connection& conn, std::vector<std::int64_t> hot_target_ids,
             std::vector<std::int64_t> rotation_target_ids);

protected:
    std::string build_query_url() const override;
    void process_response(const std::string& json_body) override;
    std::string poller_name() const override { return "gp"; }
    std::string request_description() const override {
        return std::to_string(hot_target_ids_.size() + rotation_target_ids_.size()) + " object(s) (" +
               std::to_string(hot_target_ids_.size()) + " hot, " +
               std::to_string(rotation_target_ids_.size()) + " rotation)";
    }

private:
    DbWriter writer_;
    rule_engine::RuleRegistry registry_;
    std::vector<std::int64_t> hot_target_ids_;
    std::vector<std::int64_t> rotation_target_ids_;
};

} // namespace vitale::poller
