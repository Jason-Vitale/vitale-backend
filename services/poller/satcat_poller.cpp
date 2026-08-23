#include "satcat_poller.hpp"

#include <iostream>

#include "satcat_record.hpp"

namespace vitale::poller {

namespace {
constexpr const char* kSatcatUrl =
    "https://www.space-track.org/basicspacedata/query/class/satcat/CURRENT/Y/DECAY/null-val/format/json";
} // namespace

SatcatPoller::SatcatPoller(SpaceTrackClient& client, pqxx::connection& conn)
    : Poller(client), writer_(conn) {}

std::string SatcatPoller::build_query_url() const {
    // Restricts to active, non-decayed objects -- this poller's whole job is
    // keeping `objects` in sync with the current catalog, not history.
    return kSatcatUrl;
}

void SatcatPoller::process_response(const std::string& json_body) {
    const std::vector<ObjectRecord> records = parse_satcat_response(json_body);

    for (const auto& record : records) {
        try {
            writer_.upsert_object_from_satcat(record);
        } catch (const std::exception& e) {
            std::cerr << "satcat poller: failed to upsert norad " << record.norad_cat_id << ": " << e.what()
                      << '\n';
        }
    }

    std::cout << "satcat poller: upserted " << records.size() << " object(s)\n";
}

} // namespace vitale::poller
