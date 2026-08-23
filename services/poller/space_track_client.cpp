#include "space_track_client.hpp"

#include <sstream>

namespace vitale::poller {

namespace {

constexpr const char* kBaseUrl = "https://www.space-track.org";

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Percent-encodes a query path segment (Space-Track's predicate syntax
// mixes literal characters like `>` into the path, e.g. `epoch/>now-10/`,
// which must be escaped as `%3Enow-10`).
std::string url_encode(CURL* curl, const std::string& value) {
    char* encoded = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result(encoded);
    curl_free(encoded);
    return result;
}

} // namespace

SpaceTrackClient::SpaceTrackClient(std::string identity, std::string password)
    : curl_(curl_easy_init()), identity_(std::move(identity)), password_(std::move(password)) {
    if (curl_ != nullptr) {
        // Empty string enables curl's cookie engine (in-memory jar) without
        // reading cookies from a file -- the session cookie from login()
        // then rides along automatically on every later request made
        // through this same handle.
        curl_easy_setopt(curl_, CURLOPT_COOKIEFILE, "");
    }
}

SpaceTrackClient::~SpaceTrackClient() {
    if (curl_ != nullptr) {
        curl_easy_cleanup(curl_);
    }
}

std::expected<std::string, std::string> SpaceTrackClient::perform_get(const std::string& url) {
    if (curl_ == nullptr) {
        return std::unexpected("curl handle failed to initialize");
    }

    std::string response_body;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode result = curl_easy_perform(curl_);
    if (result != CURLE_OK) {
        return std::unexpected(std::string("curl GET failed: ") + curl_easy_strerror(result));
    }

    long status_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
        std::ostringstream err;
        err << "unexpected HTTP status " << status_code << " for " << url;
        return std::unexpected(err.str());
    }

    return response_body;
}

std::expected<std::string, std::string> SpaceTrackClient::perform_post(
    const std::string& url, const std::string& post_fields) {
    if (curl_ == nullptr) {
        return std::unexpected("curl handle failed to initialize");
    }

    std::string response_body;
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);

    const CURLcode result = curl_easy_perform(curl_);
    if (result != CURLE_OK) {
        return std::unexpected(std::string("curl POST failed: ") + curl_easy_strerror(result));
    }

    long status_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status_code);
    if (status_code < 200 || status_code >= 300) {
        std::ostringstream err;
        err << "unexpected HTTP status " << status_code << " for " << url;
        return std::unexpected(err.str());
    }

    return response_body;
}

std::expected<void, std::string> SpaceTrackClient::login() {
    if (curl_ == nullptr) {
        return std::unexpected("curl handle failed to initialize");
    }

    const std::string post_fields =
        "identity=" + url_encode(curl_, identity_) + "&password=" + url_encode(curl_, password_);

    auto response = perform_post(std::string(kBaseUrl) + "/ajaxauth/login", post_fields);
    if (!response) {
        return std::unexpected(response.error());
    }

    // Space-Track's login endpoint returns HTTP 200 with a JSON error body
    // on bad credentials rather than a 4xx status, so check the body too.
    if (response->find("\"Login\"") != std::string::npos ||
        response->find("Failed") != std::string::npos) {
        return std::unexpected("Space-Track login rejected: " + *response);
    }

    logged_in_ = true;
    return {};
}

std::expected<std::string, std::string> SpaceTrackClient::fetch_gp(
    const std::vector<std::int64_t>& norad_cat_ids) {
    if (!logged_in_) {
        return std::unexpected("fetch_gp called before a successful login()");
    }
    if (norad_cat_ids.empty()) {
        return std::unexpected("fetch_gp requires at least one NORAD catalog ID");
    }

    std::ostringstream ids;
    for (std::size_t i = 0; i < norad_cat_ids.size(); ++i) {
        if (i > 0) {
            ids << ',';
        }
        ids << norad_cat_ids[i];
    }

    // Batches every requested ID into one query and restricts to live,
    // propagable objects, per Space-Track's recommended GP query filter.
    std::ostringstream url;
    url << kBaseUrl << "/basicspacedata/query/class/gp/NORAD_CAT_ID/" << ids.str()
        << "/decay_date/null-val/epoch/%3Enow-10/orderby/NORAD_CAT_ID/format/json";

    return perform_get(url.str());
}

} // namespace vitale::poller
