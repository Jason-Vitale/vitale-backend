#include <cstdlib>
#include <iostream>
#include <vector>

#include "db_connection.hpp"
#include "db_writer.hpp"
#include "gp_record.hpp"
#include "rule_registry.hpp"
#include "space_track_client.hpp"

int main() {
    const char* identity = std::getenv("SPACETRACK_IDENTITY");
    const char* password = std::getenv("SPACETRACK_PASSWORD");
    if (identity == nullptr || password == nullptr) {
        std::cerr << "SPACETRACK_IDENTITY and SPACETRACK_PASSWORD must be set\n";
        return 1;
    }

    vitale::poller::SpaceTrackClient client(identity, password);

    if (auto result = client.login(); !result) {
        std::cerr << "login failed: " << result.error() << '\n';
        return 1;
    }
    std::cout << "login succeeded\n";

    // ISS (25544) as a single-object smoke test.
    const std::vector<std::int64_t> test_norad_ids = {25544};

    auto gp_response = client.fetch_gp(test_norad_ids);
    if (!gp_response) {
        std::cerr << "fetch_gp failed: " << gp_response.error() << '\n';
        return 1;
    }
    std::cout << "fetched " << gp_response->size() << " bytes of GP data\n";

    std::vector<vitale::poller::GpRecord> records;
    try {
        records = vitale::poller::parse_gp_response(*gp_response);
    } catch (const std::exception& e) {
        std::cerr << "failed to parse GP response: " << e.what() << '\n';
        return 1;
    }
    std::cout << "parsed " << records.size() << " GP record(s)\n";

    try {
        auto conn = vitale::shared::make_connection();
        vitale::poller::DbWriter writer(conn);
        const rule_engine::RuleRegistry registry = rule_engine::make_default_rule_registry();

        for (const auto& record : records) {
            // Must run before upsert_object(): the returned snapshot's
            // decay_date reflects the object's pre-upsert state, which is
            // what makes it a valid "prev" to compare the freshly fetched
            // "curr" snapshot against.
            const auto prev = writer.get_last_snapshot(record.object.norad_cat_id);

            writer.upsert_object(record.object);

            if (prev && prev->gp_id == record.snapshot.gp_id) {
                std::cout << "norad " << record.object.norad_cat_id
                          << ": gp_id unchanged, skipping (dedup)\n";
                continue;
            }

            const std::int64_t new_snapshot_id = writer.insert_snapshot(record.snapshot);
            std::cout << "norad " << record.object.norad_cat_id
                      << ": inserted snapshot id=" << new_snapshot_id << '\n';

            if (prev) {
                const auto events = registry.evaluate_all(*prev, record.snapshot);
                for (const auto& event : events) {
                    writer.insert_event(record.object.norad_cat_id, event, prev->id, new_snapshot_id);
                    std::cout << "  -> fired event: " << event.event_type_code << '\n';
                }
            } else {
                std::cout << "  (first observation of this object, no rules evaluated)\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "database write failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
