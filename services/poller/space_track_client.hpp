#pragma once

#include <curl/curl.h>

#include <expected>
#include <string>

namespace vitale::poller {

// Thin client for the Space-Track.org REST API: authenticate once, then
// issue arbitrary GET queries reusing that session. URL construction
// (which query class, which filters) is the caller's concern -- see
// Poller::build_query_url() in poller_base.hpp -- since different ingestion
// sources (gp, satcat, ...) have very different query shapes and rate
// limits.
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

    [[nodiscard]] bool is_logged_in() const { return logged_in_; }

    // Issues a GET against an already-fully-built Space-Track URL and
    // returns the raw response body. Requires a prior successful login().
    [[nodiscard]] std::expected<std::string, std::string> get(const std::string& url);

private:
    [[nodiscard]] std::expected<std::string, std::string> perform_post(
        const std::string& url, const std::string& post_fields);

    CURL* curl_;
    std::string identity_;
    std::string password_;
    bool logged_in_ = false;
};

} // namespace vitale::poller
