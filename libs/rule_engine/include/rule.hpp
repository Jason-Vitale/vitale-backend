#pragma once

#include <optional>
#include <string>

#include "snapshot.hpp"

namespace rule_engine {

struct DetectedEvent {
    std::string event_type_code; // must match a row in `event_types.code`
    std::string detail_json;     // serialized JSON, stored as-is into audt_events.detail_json
};

class Rule {
public:
    virtual ~Rule() = default;

    // Compares two consecutive snapshots of the same object (prev.epoch < curr.epoch)
    // and returns a DetectedEvent if this rule's condition fires.
    virtual std::optional<DetectedEvent> evaluate(const Snapshot& prev, const Snapshot& curr) const = 0;
};

} // namespace rule_engine
