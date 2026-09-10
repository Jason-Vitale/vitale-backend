#include <cstdlib>
#include <ctime>
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

// GpPoller rotates through the whole `objects` catalog (~35k rows) rather
// than a fixed watchlist, 500 at a time per hourly request (see
// DbWriter::next_gp_rotation_batch). 500 isn't confirmed against
// Space-Track's own undocumented ceiling -- support hasn't answered on
// it -- but matches the empirically-found reliable batch size other
// Space-Track consumers (e.g. IBM's spacetech-ssa) have published.
constexpr int kGpRotationBatchSize = 500;

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

// Bounds each cron invocation's output in poller.log so individual runs are
// easy to pick out visually when scrolling/grepping a log that accumulates
// many runs over time -- printed regardless of whether the run did any real
// polling work or just hit due-checks.
void print_run_separator() {
    std::cout << "================== " << now_in_eastern_time() << " ==================\n";
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

    print_run_separator();

    const char* identity = std::getenv("SPACETRACK_IDENTITY");
    const char* password = std::getenv("SPACETRACK_PASSWORD");
    if (identity == nullptr || password == nullptr) {
        std::cerr << "SPACETRACK_IDENTITY and SPACETRACK_PASSWORD must be set\n";
        print_run_separator();
        return 1;
    }

    try {
        vitale::poller::SpaceTrackClient client(identity, password);
        auto conn = vitale::shared::make_connection();
        vitale::poller::DbWriter scheduler_state(conn);

        vitale::poller::SatcatPoller satcat_poller(client, conn);

        if (scheduler_state.is_poller_due("satcat", kSatcatInterval)) {
            // Marked due *before* doing any work, not after run() returns --
            // see the identical comment on the gp branch below for why.
            scheduler_state.mark_poller_run("satcat");
            std::cout << "[scheduler] running SatcatPoller\n";
            satcat_poller.run();
        } else {
            std::cout << "[scheduler] SatcatPoller not due yet\n";
        }

        if (scheduler_state.is_poller_due("gp", kGpInterval)) {
            // Marked due *before* doing any work: is_poller_due() compares
            // against wall-clock "now" (last_run_at <= now() - interval), so
            // if last_run_at instead reflected when the run *finished*,
            // every hour actually spent processing a batch (tens of seconds
            // for 500 objects) would push the next hourly check's "now() -
            // last_run_at" just under the 1-hour threshold -- causing GP to
            // fire every OTHER hour forever instead of every hour, which is
            // exactly what was observed in production. Marking it here also
            // means a throw from next_gp_rotation_batch() or GpPoller's
            // constructor below still gets recorded, rather than leaving a
            // persistent failure free to retry on every single cron tick
            // (see the rationale on mark_poller_run's declaration).
            scheduler_state.mark_poller_run("gp");

            // Fetched fresh each run, not at process startup: the batch is
            // this run's slice of the ongoing rotation, computed from
            // wherever the last successful run's cursor left off.
            const std::vector<std::int64_t> gp_targets =
                scheduler_state.next_gp_rotation_batch(kGpRotationBatchSize);

            std::cout << "[scheduler] GpPoller rotation batch: " << gp_targets.size() << " object(s)";
            if (!gp_targets.empty()) {
                std::cout << " spanning norad_cat_id " << gp_targets.front() << ".." << gp_targets.back();
            }
            std::cout << '\n';

            vitale::poller::GpPoller gp_poller(client, conn, gp_targets);
            std::cout << "[scheduler] running GpPoller at " << now_in_eastern_time() << '\n';
            gp_poller.run();
        } else {
            std::cout << "[scheduler] GpPoller not due yet (checked at " << now_in_eastern_time() << ")\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "poller: fatal error: " << e.what() << '\n';
        print_run_separator();
        // quick_exit, not return -- see the comment below. All real work is
        // done; only the catch block's own already-flushed cerr write and
        // this exit code need to survive.
        std::quick_exit(1);
    }

    // quick_exit rather than a normal return: works around a known libpqxx
    // bug (jtv/libpqxx#1007, "Double free() in global object destruction")
    // -- a static-storage variable in libpqxx's header (pqxx::internal::
    // type_name) gets external linkage under some GCC versions, an ODR
    // violation that leads to the same underlying object being destroyed
    // twice as shared libraries unload during normal exit's
    // __cxa_finalize/_dl_fini sequence -- reproduced on EC2 (Debian/Ubuntu,
    // libpqxx 7.x) as "double free or corruption (!prev)" independent of
    // which code path ran beforehand; confirmed absent under ASan locally,
    // where libpqxx 8.x (no longer has the offending variable) is used.
    // quick_exit skips the destructor/static-teardown sequence entirely
    // rather than relying on a libpqxx version we don't control in
    // production. Safe here: every DB write in this program is already
    // committed synchronously inside DbWriter's methods (nothing relies on
    // a destructor running for correctness), stdout is already flushed
    // unconditionally per write (see the unitbuf setting above), and
    // nothing in this program registers an std::at_quick_exit handler that
    // would need to run.
    print_run_separator();
    std::quick_exit(0);
}
