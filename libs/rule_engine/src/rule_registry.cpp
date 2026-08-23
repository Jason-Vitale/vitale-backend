#include "rule_registry.hpp"

namespace rule_engine {

RuleRegistry make_default_rule_registry() {
    RuleRegistry registry;
    registry.register_rule(make_maneuver_detected_rule());
    registry.register_rule(make_raan_shift_rule());
    registry.register_rule(make_eccentricity_change_rule());
    registry.register_rule(make_drag_change_rule());
    return registry;
}

} // namespace rule_engine
