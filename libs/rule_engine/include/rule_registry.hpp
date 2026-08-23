#pragma once

#include <memory>

#include "rule.hpp"

namespace rule_engine {

using RuleRegistry = RuleRegistryBase<Snapshot>;

// Factory functions for the built-in GP-snapshot rules, defined alongside
// their Rule subclasses in src/. Kept as free functions (rather than
// exposing the concrete classes in headers) since callers only ever need a
// Rule handle.
std::unique_ptr<Rule> make_maneuver_detected_rule();
std::unique_ptr<Rule> make_raan_shift_rule();
std::unique_ptr<Rule> make_eccentricity_change_rule();
std::unique_ptr<Rule> make_drag_change_rule();

// Convenience: builds a RuleRegistry pre-populated with all built-in
// GP-snapshot rules.
RuleRegistry make_default_rule_registry();

} // namespace rule_engine
