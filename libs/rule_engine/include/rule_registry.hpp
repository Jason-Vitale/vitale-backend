#pragma once

#include <memory>
#include <vector>

#include "rule.hpp"

namespace rule_engine {

class RuleRegistry {
public:
    void register_rule(std::unique_ptr<Rule> r);

    // Runs every registered rule against the pair and collects every firing
    // result -- unlike a single-match dispatcher, more than one rule can fire
    // for the same (prev, curr) pair.
    std::vector<DetectedEvent> evaluate_all(const Snapshot& prev, const Snapshot& curr) const;

private:
    std::vector<std::unique_ptr<Rule>> rules_;
};

// Factory functions for the built-in rules, defined alongside their Rule
// subclasses in src/. Kept as free functions (rather than exposing the
// concrete classes in headers) since callers only ever need a Rule handle.
std::unique_ptr<Rule> make_maneuver_detected_rule();
std::unique_ptr<Rule> make_decay_detected_rule();

// Convenience: builds a RuleRegistry pre-populated with all built-in rules.
RuleRegistry make_default_rule_registry();

} // namespace rule_engine
