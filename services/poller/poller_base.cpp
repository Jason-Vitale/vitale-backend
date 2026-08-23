#include "poller_base.hpp"

#include <iostream>

namespace vitale::poller {

void Poller::run() {
    if (!client_.is_logged_in()) {
        auto login_result = client_.login();
        if (!login_result) {
            std::cerr << "poller: login failed: " << login_result.error() << '\n';
            return;
        }
    }

    const std::string url = build_query_url();
    auto response = client_.get(url);
    if (!response) {
        std::cerr << "poller: fetch failed: " << response.error() << '\n';
        return;
    }

    try {
        process_response(*response);
    } catch (const std::exception& e) {
        std::cerr << "poller: processing response failed: " << e.what() << '\n';
    }
}

} // namespace vitale::poller
