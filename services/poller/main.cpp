#include <cstdlib>
#include <iostream>
#include <vector>

#include "space_track_client.hpp"

// Current scope (see repo README): prove out Space-Track auth + a single GP
// query end-to-end. The full write path -- db_writer storing snapshots and
// rule_engine::RuleRegistry evaluating them into audt_events -- comes next,
// once this round-trip is confirmed against the real API.
int main() {
    const char* identity = std::getenv("SPACETRACK_IDENTITY");
    const char* password = std::getenv("SPACETRACK_PASSWORD");
    if (identity == nullptr || password == nullptr) {
        std::cerr << "SPACETRACK_IDENTITY and SPACETRACK_PASSWORD must be set\n";
        return 1;
    }

    vitale::poller::SpaceTrackClient client(identity, password);

    if (auto result = client.login(); !result) {
        std::cerr << "login failed: " << result.error() << '\n';
        return 1;
    }
    std::cout << "login succeeded\n";

    // ISS (25544) as a single-object smoke test.
    const std::vector<std::int64_t> test_norad_ids = {25544};

    auto gp_response = client.fetch_gp(test_norad_ids);
    if (!gp_response) {
        std::cerr << "fetch_gp failed: " << gp_response.error() << '\n';
        return 1;
    }

    std::cout << "gp response (" << gp_response->size() << " bytes):\n" << *gp_response << '\n';
    return 0;
}
