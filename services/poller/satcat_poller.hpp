#pragma once

#include <pqxx/pqxx>

#include "db_writer.hpp"
#include "poller_base.hpp"

namespace vitale::poller {

// Populates/refreshes the objects table only -- never touches snapshots,
// the rule engine, or audt_events. A row appearing here for the first time
// is catalog bookkeeping (a newly-catalogued object), not an AUDT-worthy
// event, so it is deliberately kept out of RuleRegistry entirely.
class SatcatPoller : public Poller {
public:
    SatcatPoller(SpaceTrackClient& client, pqxx::connection& conn);

protected:
    std::string build_query_url() const override;
    void process_response(const std::string& json_body) override;

private:
    DbWriter writer_;
};

} // namespace vitale::poller
