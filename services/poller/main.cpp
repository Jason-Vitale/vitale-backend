#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
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

// Wall-clock timestamp in US Eastern time (EST/EDT, DST-aware), for log
// lines that mark when a run happened. Uses the classic POSIX
// TZ-env-var + localtime_r() approach rather than std::chrono's
// <chrono> timezone support (std::chrono::locate_zone etc.): the latter is
// still incomplete in libc++ as shipped with Apple Clang, which local macOS
// dev builds use, while TZ + tzset() is portable across both that and the
// Linux (EC2) deploy target this actually runs on.
std::string now_in_eastern_time() {
    setenv("TZ", "America/New_York", 1);
    tzset();

    const std::time_t now = std::time(nullptr);
    std::tm local_tm{};
    localtime_r(&now, &local_tm);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &local_tm);
    return std::string(buf);
}

} // namespace

// Deployed as an hourly cron job -- a fresh process each invocation, not a
// long-lived service -- so which pollers are actually due is decided from
// durable state in Postgres (poller_state, via DbWriter::is_poller_due()),
// not from an in-memory "last run" variable that would reset every time
// cron starts a new process.
int main() {
    // Defensive, independent of any particular crash's root cause: stdout
    // is normally fully buffered when redirected to a file/log (as cron
    // does), so a hard abort (SIGABRT, segfault, ...) loses whatever hadn't
    // been flushed yet -- the exact "crash aborts, poller.log shows
    // nothing" gap. unitbuf flushes after every `std::cout <<`, so
    // whatever was logged right up to the crash is actually on disk.
    // std::cerr is already unit-buffered by default per the standard, so
    // only cout needs this.
    std::cout.setf(std::ios::unitbuf);

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

    // Printed every invocation, independent of whether GpPoller is actually
    // due this hour -- so "which objects is this even configured to poll"
    // is answerable from the log even on a cycle that skips it entirely.
    {
        std::ostringstream ids;
        for (std::size_t i = 0; i < gp_targets.size(); ++i) {
            if (i > 0) {
                ids << ", ";
            }
            ids << gp_targets[i];
        }
        std::cout << "[scheduler] GpPoller configured with " << gp_targets.size()
                  << " target norad id(s): " << ids.str() << '\n';
    }

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
            std::cout << "[scheduler] running GpPoller at " << now_in_eastern_time() << '\n';
            gp_poller.run();
            scheduler_state.mark_poller_run("gp");
        } else {
            std::cout << "[scheduler] GpPoller not due yet (checked at " << now_in_eastern_time() << ")\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "poller: fatal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
