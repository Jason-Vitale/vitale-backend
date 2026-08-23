#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rule_engine {

// Emitted by a Rule/ObjectRule when its condition fires; written verbatim
// into `audt_events` by the poller layer (see services/poller/db_writer.cpp).
struct DetectedEvent {
    std::string event_type_code; // must match a row in `event_types.code`
    std::string detail_json;     // serialized JSON, stored as-is into audt_events.detail_json
};

// Shared shape behind both `Rule` (GP snapshots, see rule.hpp) and
// `ObjectRule` (SATCAT/objects rows, see object_rule.hpp): compare two
// consecutive states of the same object and optionally report a fired
// event. Templated on the state type rather than duplicated -- GP and
// SATCAT data live in genuinely different shapes (Snapshot vs ObjectState)
// but the detection pattern is identical.
template <typename State>
class RuleBase {
public:
    virtual ~RuleBase() = default;
    virtual std::optional<DetectedEvent> evaluate(const State& prev, const State& curr) const = 0;
};

template <typename State>
class RuleRegistryBase {
public:
    void register_rule(std::unique_ptr<RuleBase<State>> r) { rules_.push_back(std::move(r)); }

    // Runs every registered rule against the pair and collects every firing
    // result -- unlike a single-match dispatcher, more than one rule can fire
    // for the same (prev, curr) pair.
    std::vector<DetectedEvent> evaluate_all(const State& prev, const State& curr) const {
        std::vector<DetectedEvent> events;
        for (const auto& rule : rules_) {
            if (auto event = rule->evaluate(prev, curr)) {
                events.push_back(std::move(*event));
            }
        }
        return events;
    }

private:
    std::vector<std::unique_ptr<RuleBase<State>>> rules_;
};

} // namespace rule_engine
