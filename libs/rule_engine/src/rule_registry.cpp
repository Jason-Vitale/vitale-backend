#include "rule_registry.hpp"

namespace rule_engine {

void RuleRegistry::register_rule(std::unique_ptr<Rule> r) {
    rules_.push_back(std::move(r));
}

std::vector<DetectedEvent> RuleRegistry::evaluate_all(const Snapshot& prev, const Snapshot& curr) const {
    std::vector<DetectedEvent> events;
    for (const auto& rule : rules_) {
        if (auto event = rule->evaluate(prev, curr)) {
            events.push_back(std::move(*event));
        }
    }
    return events;
}

RuleRegistry make_default_rule_registry() {
    RuleRegistry registry;
    registry.register_rule(make_maneuver_detected_rule());
    registry.register_rule(make_decay_detected_rule());
    return registry;
}

} // namespace rule_engine
