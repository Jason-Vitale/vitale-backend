#include "poller_base.hpp"

#include <iostream>

namespace vitale::poller {

void Poller::run() {
    const std::string name = poller_name();

    if (!client_.is_logged_in()) {
        std::cout << name << " poller: not yet authenticated, logging in to Space-Track...\n";
        auto login_result = client_.login();
        if (!login_result) {
            std::cerr << name << " poller: login failed: " << login_result.error() << '\n';
            return;
        }
        std::cout << name << " poller: authenticated\n";
    }

    const std::string url = build_query_url();
    std::cout << name << " poller: querying " << url << '\n';

    auto response = client_.get(url);
    if (!response) {
        std::cerr << name << " poller: fetch failed: " << response.error() << '\n';
        return;
    }
    std::cout << name << " poller: received response (" << response->size() << " bytes)\n";

    try {
        process_response(*response);
    } catch (const std::exception& e) {
        std::cerr << name << " poller: processing response failed: " << e.what() << '\n';
    }
}

} // namespace vitale::poller
