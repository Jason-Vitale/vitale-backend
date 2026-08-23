#pragma once

#include <curl/curl.h>

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace vitale::poller {

// Thin client for the Space-Track.org REST API, scoped to what the poller
// needs: authenticate once, then pull `gp` (General Perturbations / current
// TLE) records for a batch of objects.
//
// Space-Track's `gp` class is rate-limited to 1 request/hour, so callers
// MUST batch every NORAD ID they care about into a single fetch_gp() call --
// never loop one request per satellite.
class SpaceTrackClient {
public:
    SpaceTrackClient(std::string identity, std::string password);
    ~SpaceTrackClient();

    SpaceTrackClient(const SpaceTrackClient&) = delete;
    SpaceTrackClient& operator=(const SpaceTrackClient&) = delete;

    // POSTs identity/password to /ajaxauth/login. The session cookie is
    // captured by curl's in-memory cookie engine and reused automatically by
    // every subsequent request made through this instance.
    [[nodiscard]] std::expected<void, std::string> login();

    // Fetches current GP records for the given NORAD catalog IDs in one
    // request (comma-delimited), filtered to live/propagable objects
    // (decay_date null, epoch within the last 10 days). Returns the raw JSON
    // response body on success.
    [[nodiscard]] std::expected<std::string, std::string> fetch_gp(
        const std::vector<std::int64_t>& norad_cat_ids);

private:
    [[nodiscard]] std::expected<std::string, std::string> perform_get(const std::string& url);
    [[nodiscard]] std::expected<std::string, std::string> perform_post(
        const std::string& url, const std::string& post_fields);

    CURL* curl_;
    std::string identity_;
    std::string password_;
    bool logged_in_ = false;
};

} // namespace vitale::poller
