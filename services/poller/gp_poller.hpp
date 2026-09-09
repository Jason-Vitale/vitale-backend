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
    // target_norad_ids is one rotation batch of DbWriter::
    // next_gp_rotation_batch() (see main.cpp), not a fixed watchlist --
    // GP-polls the whole `objects` catalog over successive runs rather
    // than a hand-picked subset. Callers own fetching the batch and
    // constructing a fresh GpPoller with it each run; on a successful
    // batch, process_response() advances the rotation cursor to this
    // batch's last (highest) id.
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
