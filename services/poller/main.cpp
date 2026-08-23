#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "db_connection.hpp"
#include "gp_poller.hpp"
#include "satcat_poller.hpp"
#include "space_track_client.hpp"

using namespace std::chrono_literals;

namespace {

// Poll cadence per ingestion source: gp is rate-limited by Space-Track to
// 1 request/hour; satcat has no documented hard limit, but daily is more
// than sufficient for catalog bookkeeping.
constexpr auto kGpInterval = std::chrono::hours(1);
constexpr auto kSatcatInterval = std::chrono::hours(24);

} // namespace

int main(int argc, char** argv) {
    const char* identity = std::getenv("SPACETRACK_IDENTITY");
    const char* password = std::getenv("SPACETRACK_PASSWORD");
    if (identity == nullptr || password == nullptr) {
        std::cerr << "SPACETRACK_IDENTITY and SPACETRACK_PASSWORD must be set\n";
        return 1;
    }

    // Stubbed target list for GpPoller -- see gp_poller.hpp. Which objects
    // get actively GP-polled (vs merely catalogued via SatcatPoller) is an
    // open product decision; this is a placeholder, not the final answer.
    const std::vector<std::int64_t> gp_targets = {25544};

    const bool run_once = argc > 1 && std::string(argv[1]) == "--once";

    try {
        vitale::poller::SpaceTrackClient client(identity, password);
        auto conn = vitale::shared::make_connection();

        vitale::poller::SatcatPoller satcat_poller(client, conn);
        vitale::poller::GpPoller gp_poller(client, conn, gp_targets);

        // Both pollers use the same SpaceTrackClient (one shared session)
        // and the same connection; scheduling which runs when lives here
        // rather than inside either poller. nullopt means "never run yet,
        // due immediately" -- steady_clock::time_point::min() looks like it
        // would express the same thing, but `now - time_point::min()`
        // overflows the duration's representable range and silently
        // corrupts the >= comparison, so don't use that trick.
        std::optional<std::chrono::steady_clock::time_point> last_gp_run;
        std::optional<std::chrono::steady_clock::time_point> last_satcat_run;

        for (;;) {
            const auto now = std::chrono::steady_clock::now();

            if (!last_satcat_run || now - *last_satcat_run >= kSatcatInterval) {
                std::cout << "[scheduler] running SatcatPoller\n";
                satcat_poller.run();
                last_satcat_run = now;
            }

            if (!last_gp_run || now - *last_gp_run >= kGpInterval) {
                std::cout << "[scheduler] running GpPoller\n";
                gp_poller.run();
                last_gp_run = now;
            }

            if (run_once) {
                break;
            }

            std::this_thread::sleep_for(1min);
        }
    } catch (const std::exception& e) {
        std::cerr << "poller: fatal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
