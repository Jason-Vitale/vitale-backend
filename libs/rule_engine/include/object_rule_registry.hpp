#pragma once

#include <memory>

#include "object_rule.hpp"

namespace rule_engine {

using ObjectRuleRegistry = RuleRegistryBase<ObjectState>;

// Factory functions for the built-in SATCAT/objects-row rules, defined
// alongside their ObjectRule subclasses in src/.
std::unique_ptr<ObjectRule> make_decay_detected_rule();
std::unique_ptr<ObjectRule> make_object_renamed_rule();
std::unique_ptr<ObjectRule> make_object_type_changed_rule();
std::unique_ptr<ObjectRule> make_rcs_size_changed_rule();

// Convenience: builds an ObjectRuleRegistry pre-populated with all built-in
// SATCAT rules.
ObjectRuleRegistry make_default_object_rule_registry();

} // namespace rule_engine
