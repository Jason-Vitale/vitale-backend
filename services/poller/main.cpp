#include <cstdlib>
#include <iostream>
#include <vector>

#include "db_connection.hpp"
#include "db_writer.hpp"
#include "gp_poller.hpp"
#include "satcat_poller.hpp"
#include "space_track_client.hpp"

namespace {

// Poll cadence per ingestion source, expressed as Postgres interval
// literals since DbWriter::is_poller_due() compares against these
// server-side. gp is rate-limited by Space-Track to 1 request/hour; satcat
// has no documented hard limit, but daily is more than sufficient for
// catalog bookkeeping.
constexpr const char* kGpInterval = "1 hour";
constexpr const char* kSatcatInterval = "24 hours";

} // namespace

// Deployed as an hourly cron job -- a fresh process each invocation, not a
// long-lived service -- so which pollers are actually due is decided from
// durable state in Postgres (poller_state, via DbWriter::is_poller_due()),
// not from an in-memory "last run" variable that would reset every time
// cron starts a new process.
int main() {
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

    try {
        vitale::poller::SpaceTrackClient client(identity, password);
        auto conn = vitale::shared::make_connection();
        vitale::poller::DbWriter scheduler_state(conn);

        vitale::poller::SatcatPoller satcat_poller(client, conn);
        vitale::poller::GpPoller gp_poller(client, conn, gp_targets);

        if (scheduler_state.is_poller_due("satcat", kSatcatInterval)) {
            std::cout << "[scheduler] running SatcatPoller\n";
            satcat_poller.run();
            scheduler_state.mark_poller_run("satcat");
        } else {
            std::cout << "[scheduler] SatcatPoller not due yet\n";
        }

        if (scheduler_state.is_poller_due("gp", kGpInterval)) {
            std::cout << "[scheduler] running GpPoller\n";
            gp_poller.run();
            scheduler_state.mark_poller_run("gp");
        } else {
            std::cout << "[scheduler] GpPoller not due yet\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "poller: fatal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
