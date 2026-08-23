#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rule_engine {

// Mirrors one row of the `snapshots` table (see db/migrations/001_init.sql).
// Populated by the poller from a Space-Track GP record; consumed read-only
// by Rule implementations to compare consecutive states of one object.
struct Snapshot {
    std::int64_t id = 0;
    int norad_cat_id = 0;
    std::int64_t gp_id = 0;

    std::string epoch;      // ISO-8601 timestamp, as returned by Space-Track
    std::string fetched_at; // ISO-8601 timestamp

    double mean_motion = 0.0;
    double eccentricity = 0.0;
    double inclination = 0.0;
    double ra_of_asc_node = 0.0;
    double arg_of_pericenter = 0.0;
    double mean_anomaly = 0.0;
    double semimajor_axis = 0.0;
    double period = 0.0;
    double apoapsis = 0.0;
    double periapsis = 0.0;
    double bstar = 0.0;
    double mean_motion_dot = 0.0;
    double mean_motion_ddot = 0.0;

    int element_set_no = 0;
    int rev_at_epoch = 0;

    std::string tle_line0;
    std::string tle_line1;
    std::string tle_line2;

    // From the parent `objects` row, not `snapshots` itself -- carried along
    // so rules like DecayDetectedRule don't need a second lookup.
    std::optional<std::string> decay_date;
};

} // namespace rule_engine
