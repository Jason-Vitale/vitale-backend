#pragma once

#include <pqxx/pqxx>

#include "db_writer.hpp"
#include "object_rule_registry.hpp"
#include "poller_base.hpp"

namespace vitale::poller {

// Populates/refreshes the objects table -- never touches snapshots (that's
// GpPoller's job). A row appearing here for the first time is catalog
// bookkeeping (a newly-catalogued object), not an AUDT-worthy event, so no
// rule fires on first sighting. On every subsequent sighting, the existing
// row is diffed against the incoming one via ObjectRuleRegistry before
// being overwritten, and any fired events are written to audt_events.
class SatcatPoller : public Poller {
public:
    SatcatPoller(SpaceTrackClient& client, pqxx::connection& conn);

protected:
    std::string build_query_url() const override;
    void process_response(const std::string& json_body) override;
    std::string poller_name() const override { return "satcat"; }

private:
    DbWriter writer_;
    rule_engine::ObjectRuleRegistry registry_;
};

} // namespace vitale::poller
