#include <iostream>
#include <optional>
#include <string>

#include "object_rule_registry.hpp"
#include "rule_registry.hpp"

// No test framework dependency (none is set up in this repo yet) -- plain
// asserts collected into a failure count, run via CTest.

using namespace rule_engine;

namespace {

int g_failures = 0;

void expect(bool cond, const std::string& what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << '\n';
        ++g_failures;
    }
}

void expect_fired(const std::optional<DetectedEvent>& ev, const std::string& code, const std::string& what) {
    expect(ev.has_value(), what + " (expected to fire)");
    if (ev) {
        expect(ev->event_type_code == code, what + " (wrong event_type_code: " + ev->event_type_code + ")");
    }
}

void expect_not_fired(const std::optional<DetectedEvent>& ev, const std::string& what) {
    expect(!ev.has_value(), what + " (expected NOT to fire)");
}

Snapshot base_snapshot() {
    Snapshot s;
    s.norad_cat_id = 12345;
    s.epoch = "2026-08-20T00:00:00Z";
    s.inclination = 51.6;
    s.semimajor_axis = 6800.0;
    s.ra_of_asc_node = 120.0;
    s.eccentricity = 0.001;
    s.bstar = 0.0001;
    return s;
}

ObjectState base_object() {
    ObjectState o;
    o.norad_cat_id = 12345;
    o.object_name = "STARLINK-1234";
    o.object_type = "PAYLOAD";
    o.rcs_size = "MEDIUM";
    return o;
}

void test_maneuver_detected_rule() {
    const auto rule = make_maneuver_detected_rule();
    const Snapshot prev = base_snapshot();

    Snapshot curr = prev;
    expect_not_fired(rule->evaluate(prev, curr), "maneuver: identical snapshots");

    curr = prev;
    curr.inclination = prev.inclination + 0.02; // > 0.01 deg threshold
    expect_fired(rule->evaluate(prev, curr), "maneuver_detected", "maneuver: inclination delta over threshold");

    curr = prev;
    curr.semimajor_axis = prev.semimajor_axis + 1.5; // > 1.0 km threshold
    expect_fired(rule->evaluate(prev, curr), "maneuver_detected", "maneuver: semimajor axis delta over threshold");

    curr = prev;
    curr.inclination = prev.inclination + 0.005;    // under threshold
    curr.semimajor_axis = prev.semimajor_axis + 0.5; // under threshold
    expect_not_fired(rule->evaluate(prev, curr), "maneuver: both deltas under threshold");
}

void test_raan_shift_rule() {
    const auto rule = make_raan_shift_rule();
    const Snapshot prev = base_snapshot();

    Snapshot curr = prev;
    curr.ra_of_asc_node = prev.ra_of_asc_node + 0.02;
    expect_fired(rule->evaluate(prev, curr), "raan_shift", "raan: delta over threshold");

    curr = prev;
    curr.ra_of_asc_node = prev.ra_of_asc_node + 0.001;
    expect_not_fired(rule->evaluate(prev, curr), "raan: delta under threshold");
}

void test_eccentricity_change_rule() {
    const auto rule = make_eccentricity_change_rule();
    const Snapshot prev = base_snapshot();

    Snapshot curr = prev;
    curr.eccentricity = prev.eccentricity + 0.002;
    expect_fired(rule->evaluate(prev, curr), "eccentricity_change", "eccentricity: delta over threshold");

    curr = prev;
    curr.eccentricity = prev.eccentricity + 0.0001;
    expect_not_fired(rule->evaluate(prev, curr), "eccentricity: delta under threshold");
}

void test_drag_change_rule() {
    const auto rule = make_drag_change_rule();

    Snapshot prev = base_snapshot();
    prev.bstar = 0.0002;
    Snapshot curr = prev;
    curr.bstar = prev.bstar * 1.6; // +60%, over 50% threshold
    expect_fired(rule->evaluate(prev, curr), "drag_change", "drag: percent change over threshold");

    curr.bstar = prev.bstar * 1.1; // +10%, under threshold
    expect_not_fired(rule->evaluate(prev, curr), "drag: percent change under threshold");

    // Near-zero prev.bstar falls back to the absolute threshold.
    prev.bstar = 0.0;
    curr = prev;
    curr.bstar = 0.0002; // > 0.0001 absolute threshold
    expect_fired(rule->evaluate(prev, curr), "drag_change", "drag: absolute fallback over threshold");

    curr.bstar = 0.00005; // < 0.0001 absolute threshold
    expect_not_fired(rule->evaluate(prev, curr), "drag: absolute fallback under threshold");
}

void test_decay_detected_rule() {
    const auto rule = make_decay_detected_rule();
    const ObjectState prev = base_object();

    ObjectState curr = prev;
    curr.decay_date = "2026-08-22";
    expect_fired(rule->evaluate(prev, curr), "decay_detected", "decay: null -> non-null");

    ObjectState already_decayed_prev = prev;
    already_decayed_prev.decay_date = "2026-08-20";
    curr.decay_date = "2026-08-22"; // already decayed, not a new transition
    expect_not_fired(rule->evaluate(already_decayed_prev, curr), "decay: no re-fire once already decayed");
}

void test_object_renamed_rule() {
    const auto rule = make_object_renamed_rule();
    const ObjectState prev = base_object();

    ObjectState curr = prev;
    curr.object_name = "STARLINK-9999";
    expect_fired(rule->evaluate(prev, curr), "object_renamed", "renamed: name changed");

    curr = prev;
    expect_not_fired(rule->evaluate(prev, curr), "renamed: name unchanged");
}

void test_object_type_changed_rule() {
    const auto rule = make_object_type_changed_rule();
    const ObjectState prev = base_object();

    ObjectState curr = prev;
    curr.object_type = "DEBRIS";
    expect_fired(rule->evaluate(prev, curr), "object_type_changed", "type: PAYLOAD -> DEBRIS");

    curr = prev;
    expect_not_fired(rule->evaluate(prev, curr), "type: unchanged");
}

void test_rcs_size_changed_rule() {
    const auto rule = make_rcs_size_changed_rule();
    const ObjectState prev = base_object();

    ObjectState curr = prev;
    curr.rcs_size = "LARGE";
    expect_fired(rule->evaluate(prev, curr), "rcs_size_changed", "rcs: MEDIUM -> LARGE");

    curr = prev;
    expect_not_fired(rule->evaluate(prev, curr), "rcs: unchanged");
}

void test_default_registries_fire_together() {
    RuleRegistry gp_registry = make_default_rule_registry();
    const Snapshot prev = base_snapshot();
    Snapshot curr = prev;
    curr.inclination = prev.inclination + 0.02;
    curr.ra_of_asc_node = prev.ra_of_asc_node + 0.02;
    curr.eccentricity = prev.eccentricity + 0.002;
    curr.bstar = prev.bstar * 2.0;
    const auto events = gp_registry.evaluate_all(prev, curr);
    expect(events.size() == 4, "gp registry: expected all 4 rules to fire together");

    ObjectRuleRegistry satcat_registry = make_default_object_rule_registry();
    const ObjectState oprev = base_object();
    ObjectState ocurr = oprev;
    ocurr.decay_date = "2026-08-22";
    ocurr.object_name = "RENAMED";
    ocurr.object_type = "DEBRIS";
    ocurr.rcs_size = "LARGE";
    const auto oevents = satcat_registry.evaluate_all(oprev, ocurr);
    expect(oevents.size() == 4, "satcat registry: expected all 4 rules to fire together");
}

} // namespace

int main() {
    test_maneuver_detected_rule();
    test_raan_shift_rule();
    test_eccentricity_change_rule();
    test_drag_change_rule();
    test_decay_detected_rule();
    test_object_renamed_rule();
    test_object_type_changed_rule();
    test_rcs_size_changed_rule();
    test_default_registries_fire_together();

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "all rule_engine tests passed\n";
    return 0;
}
