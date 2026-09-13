#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <streambuf>
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

// GpPoller requests this many objects total per hourly run. Not confirmed
// against Space-Track's own undocumented ceiling -- support hasn't
// answered on it -- but matches the empirically-found reliable batch size
// other Space-Track consumers (e.g. IBM's spacetech-ssa) have published.
constexpr int kGpBatchSize = 500;

// Of each hourly batch, up to this many slots go to DbWriter::
// top_gp_hot_targets() -- featured/high-traffic objects re-requested every
// run instead of waiting for the rotation to reach them -- with whatever's
// left going to DbWriter::next_gp_rotation_batch()'s ordinary sweep through
// the rest of the ~35k-object catalog. top_gp_hot_targets() isn't padded
// out to this number (see its declaration), so early on -- before real
// site traffic and the featured seed list add up to 200 -- most of the
// batch still goes to rotation; this is a ceiling, not a guarantee.
constexpr int kGpMaxHotSlots = 200;
static_assert(kGpMaxHotSlots <= kGpBatchSize,
              "hot slice can't be larger than the whole batch it's carved out of");

// Wall-clock timestamp in US Eastern time (EST/EDT, DST-aware), down to the
// millisecond, for log lines that mark when a run happened. Uses the
// classic POSIX TZ-env-var + localtime_r() approach rather than
// std::chrono's <chrono> timezone support (std::chrono::locate_zone etc.):
// the latter is still incomplete in libc++ as shipped with Apple Clang,
// which local macOS dev builds use, while TZ + tzset() is portable across
// both that and the Linux (EC2) deploy target this actually runs on.
// localtime_r() itself only has second resolution, so the millisecond
// component is pulled separately from system_clock and appended by hand.
std::string now_in_eastern_time() {
    setenv("TZ", "America/New_York", 1);
    tzset();

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) %
                    std::chrono::seconds(1);

    std::tm local_tm{};
    localtime_r(&now_time_t, &local_tm);

    char date_buf[24];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    char tz_buf[8];
    std::strftime(tz_buf, sizeof(tz_buf), "%Z", &local_tm);

    char full_buf[48];
    std::snprintf(full_buf, sizeof(full_buf), "%s.%03lld %s", date_buf, static_cast<long long>(ms.count()), tz_buf);
    return std::string(full_buf);
}

// Bounds each cron invocation's output in poller.log so individual runs are
// easy to pick out visually when scrolling/grepping a log that accumulates
// many runs over time -- printed regardless of whether the run did any real
// polling work or just hit due-checks. No embedded timestamp of its own --
// TimestampedStreambuf below already prefixes every line with one.
void print_run_separator() {
    std::cout << "==================\n";
}

// Wraps another streambuf (std::cout's or std::cerr's real one) so every
// line gets a "[<timestamp>] - " prefix automatically -- added so every log
// line, including ones from poller_base.cpp, is timestamped without every
// individual log call site needing to remember to print its own.
class TimestampedStreambuf : public std::streambuf {
public:
    explicit TimestampedStreambuf(std::streambuf* dest) : dest_(dest) {}

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) {
            return ch;
        }
        if (at_line_start_) {
            const std::string prefix = "[" + now_in_eastern_time() + "] - ";
            dest_->sputn(prefix.data(), static_cast<std::streamsize>(prefix.size()));
        }
        at_line_start_ = (ch == '\n');
        return dest_->sputc(static_cast<char>(ch));
    }

    int sync() override { return dest_->pubsync(); }

private:
    std::streambuf* dest_;
    bool at_line_start_ = true;
};

} // namespace

// Deployed as an hourly cron job -- a fresh process each invocation, not a
// long-lived service -- so which pollers are actually due is decided from
// durable state in Postgres (poller_state, via DbWriter::is_poller_due()),
// not from an in-memory "last run" variable that would reset every time
// cron starts a new process.
int main() {
    // Installed before anything else logs a single line, so every line for
    // the rest of the process's life -- including the opening separator
    // right below -- gets a timestamp prefix automatically. These live for
    // main()'s whole lifetime (including past quick_exit(), same as every
    // other object here -- see the comment on quick_exit() below).
    TimestampedStreambuf cout_buf(std::cout.rdbuf());
    std::cout.rdbuf(&cout_buf);
    TimestampedStreambuf cerr_buf(std::cerr.rdbuf());
    std::cerr.rdbuf(&cerr_buf);

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

        // Both due-checks are decided and stamped up front, before either
        // poller does any real work, rather than interleaved with the
        // satcat/gp run() calls below. satcat and gp run sequentially in
        // this one process, so if gp's is_poller_due()/mark_poller_run()
        // only happened after satcat_poller.run() returned, satcat's own
        // runtime (minutes, when its 24-hour interval comes due and it
        // diffs/upserts the full ~35k-object catalog) would delay gp's
        // scheduling decision by that same amount -- large enough to blow
        // through the 5-minute tolerance in is_poller_due() and reintroduce
        // the every-other-hour skip that tolerance was added to fix, just
        // via a different poller's runtime instead of gp's own. Deciding
        // and stamping both here, back-to-back, keeps gp's last_run_at
        // anchored to this tick's actual decision instant regardless of
        // what satcat does afterward.
        const bool satcat_due = scheduler_state.is_poller_due("satcat", kSatcatInterval);
        const bool gp_due = scheduler_state.is_poller_due("gp", kGpInterval);
        if (satcat_due) {
            scheduler_state.mark_poller_run("satcat");
        }
        if (gp_due) {
            // Marking here, before next_gp_rotation_batch()/GpPoller's
            // constructor below, also means a throw from either still gets
            // recorded, rather than leaving a persistent failure free to
            // retry on every single cron tick (see the rationale on
            // mark_poller_run's declaration).
            scheduler_state.mark_poller_run("gp");
        }

        if (satcat_due) {
            std::cout << "[scheduler] running SatcatPoller\n";
            satcat_poller.run();
        } else {
            std::cout << "[scheduler] SatcatPoller not due yet\n";
        }

        if (gp_due) {
            // Fetched fresh each run, not at process startup. Hot targets
            // come first so the rotation only has to fill whatever
            // capacity is left in kGpBatchSize -- see the comments on both
            // constants above for why the split isn't a fixed 200/300.
            const std::vector<std::int64_t> hot_targets = scheduler_state.top_gp_hot_targets(kGpMaxHotSlots);
            const std::vector<std::int64_t> rotation_targets =
                scheduler_state.next_gp_rotation_batch(kGpBatchSize - static_cast<int>(hot_targets.size()), hot_targets);

            std::cout << "[scheduler] GpPoller batch: " << hot_targets.size() << " hot object(s), "
                      << rotation_targets.size() << " rotation object(s)";
            if (!rotation_targets.empty()) {
                std::cout << " spanning norad_cat_id " << rotation_targets.front() << ".."
                          << rotation_targets.back();
            }
            std::cout << '\n';

            vitale::poller::GpPoller gp_poller(client, conn, hot_targets, rotation_targets);
            std::cout << "[scheduler] running GpPoller\n";
            gp_poller.run();
        } else {
            std::cout << "[scheduler] GpPoller not due yet\n";
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
