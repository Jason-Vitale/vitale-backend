#pragma once

#include <optional>
#include <string>

namespace rule_engine {

// Mirrors one row of the `objects` table (see db/migrations/001_init.sql).
// Populated by the poller from a Space-Track SATCAT record; consumed
// read-only by ObjectRule implementations to compare consecutive states of
// one object's catalog metadata.
struct ObjectState {
    int norad_cat_id = 0;
    std::string object_name;
    std::string object_id;
    std::string object_type;
    std::string country_code;
    std::optional<std::string> launch_date;
    std::string site;
    std::string rcs_size;
    std::optional<std::string> decay_date;
};

} // namespace rule_engine
