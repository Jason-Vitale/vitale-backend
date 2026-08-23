#pragma once

#include <string>

#include "space_track_client.hpp"

namespace vitale::poller {

// Shared skeleton for every ingestion poller (SatcatPoller, GpPoller, ...).
// Ingestion sources differ in query construction, response parsing, and
// whether they invoke the rule engine -- but all of them authenticate
// through the same SpaceTrackClient and follow the same
// authenticate -> fetch -> persist shape.
//
// The SpaceTrackClient is shared (by reference) across every poller
// instance so login() happens at most once per process: is_logged_in()
// makes run() a no-op on auth for every call after the first, regardless of
// which poller ran first.
class Poller {
public:
    explicit Poller(SpaceTrackClient& client) : client_(client) {}
    virtual ~Poller() = default;

    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;

    // Template method: login (if needed) -> build_query_url() -> fetch ->
    // process_response(). Errors are logged to stderr and swallowed rather
    // than thrown, so a scheduler can keep running after a failed cycle.
    void run();

protected:
    virtual std::string build_query_url() const = 0;
    virtual void process_response(const std::string& json_body) = 0;

    // Short identifier ("gp", "satcat", ...) used to prefix every log line
    // run() emits, so a shared cron log with both pollers interleaved in it
    // is still unambiguous about which poller produced which line.
    virtual std::string poller_name() const = 0;

    SpaceTrackClient& client_;
};

} // namespace vitale::poller
