#include "object_rule_registry.hpp"

namespace rule_engine {

ObjectRuleRegistry make_default_object_rule_registry() {
    ObjectRuleRegistry registry;
    registry.register_rule(make_decay_detected_rule());
    registry.register_rule(make_object_renamed_rule());
    registry.register_rule(make_object_type_changed_rule());
    registry.register_rule(make_rcs_size_changed_rule());
    return registry;
}

} // namespace rule_engine
