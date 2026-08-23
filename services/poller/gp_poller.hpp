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
    // target_norad_ids is deliberately injected rather than derived from
    // "all objects": which subset of the catalog gets actively GP-polled
    // (vs merely known via SatcatPoller) is an open product decision.
    // Callers own the list; swap it for a real watchlist query once that's
    // settled -- for now this is a stub, not the final answer.
    GpPoller(SpaceTrackClient& client, pqxx::connection& conn, std::vector<std::int64_t> target_norad_ids);

protected:
    std::string build_query_url() const override;
    void process_response(const std::string& json_body) override;
    std::string poller_name() const override { return "gp"; }

private:
    DbWriter writer_;
    rule_engine::RuleRegistry registry_;
    std::vector<std::int64_t> target_norad_ids_;
};

} // namespace vitale::poller
