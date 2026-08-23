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

    // TEMPORARY, pending two open items: (1) which objects get actively
    // GP-polled vs merely catalogued via SatcatPoller is still an
    // unsettled product decision (see gp_poller.hpp), and (2) we have not
    // empirically confirmed a maximum comma-delimited NORAD_CAT_ID batch
    // size for the gp class -- Space-Track's docs don't state one. Capped
    // to a small, hand-picked, verified-real set of well-known active
    // objects (looked up directly against our own objects table, not
    // guessed) rather than deriving from the full ~35k-row catalog, so an
    // hourly cron-driven GpPoller run can't hit an unknown batch-size or
    // URL-length limit against the live account. Revisit both before
    // expanding this list.
    const std::vector<std::int64_t> gp_targets = {
        25544,  // ISS (ZARYA)
        20580,  // HST (Hubble Space Telescope)
        25994,  // TERRA
        31698,  // TERRA SAR X
        37218,  // SKYTERRA 1
        43013,  // NOAA 20
        43491,  // FENGYUN 2H
        49260,  // LANDSAT 9
        66514,  // SENTINEL-6B
        44714,  // STARLINK-1008
        44718,  // STARLINK-1012
        44723,  // STARLINK-1017
    };

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
