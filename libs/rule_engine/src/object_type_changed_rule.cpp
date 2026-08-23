#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "object_rule.hpp"
#include "object_rule_registry.hpp"

namespace rule_engine {
namespace {

// Higher-severity than the other SATCAT rules (see event_types seed
// migration): a reclassification, e.g. PAYLOAD -> DEBRIS after a breakup,
// is significant.
class ObjectTypeChangedRule : public ObjectRule {
public:
    std::optional<DetectedEvent> evaluate(const ObjectState& prev, const ObjectState& curr) const override {
        if (prev.object_type.empty() || curr.object_type.empty()) {
            return std::nullopt;
        }
        if (prev.object_type == curr.object_type) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["old_type"] = prev.object_type;
        detail["new_type"] = curr.object_type;

        return DetectedEvent{"object_type_changed", detail.dump()};
    }
};

} // namespace

std::unique_ptr<ObjectRule> make_object_type_changed_rule() {
    return std::make_unique<ObjectTypeChangedRule>();
}

} // namespace rule_engine
