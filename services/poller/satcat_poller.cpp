#include "satcat_poller.hpp"

#include <iostream>

#include "satcat_record.hpp"

namespace vitale::poller {

namespace {
constexpr const char* kSatcatUrl =
    "https://www.space-track.org/basicspacedata/query/class/satcat/CURRENT/Y/DECAY/null-val/format/json";
} // namespace

SatcatPoller::SatcatPoller(SpaceTrackClient& client, pqxx::connection& conn)
    : Poller(client), writer_(conn), registry_(rule_engine::make_default_object_rule_registry()) {}

std::string SatcatPoller::build_query_url() const {
    // Restricts to active, non-decayed objects -- this poller's whole job is
    // keeping `objects` in sync with the current catalog, not history.
    return kSatcatUrl;
}

void SatcatPoller::process_response(const std::string& json_body) {
    const std::vector<ObjectRecord> records = parse_satcat_response(json_body);
    std::cout << "satcat poller: parsed " << records.size() << " record(s), diffing and upserting...\n";

    try {
        const auto events = writer_.upsert_objects_from_satcat(records, registry_);
        std::cout << "satcat poller: upserted " << records.size() << " object(s)\n";
        for (const auto& event : events) {
            std::cout << "  -> fired event: " << event.event_type_code << '\n';
        }
    } catch (const std::exception& e) {
        std::cerr << "satcat poller: batch upsert failed, nothing committed: " << e.what() << '\n';
    }
}

} // namespace vitale::poller
