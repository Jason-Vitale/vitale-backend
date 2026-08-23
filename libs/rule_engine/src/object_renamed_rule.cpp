#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "object_rule.hpp"
#include "object_rule_registry.hpp"

namespace rule_engine {
namespace {

class ObjectRenamedRule : public ObjectRule {
public:
    std::optional<DetectedEvent> evaluate(const ObjectState& prev, const ObjectState& curr) const override {
        // Guard against firing on missing-data artifacts: SATCAT parsing
        // maps an absent/null field to "", so an empty name on either side
        // isn't a real rename, just incomplete data.
        if (prev.object_name.empty() || curr.object_name.empty()) {
            return std::nullopt;
        }
        if (prev.object_name == curr.object_name) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["old_name"] = prev.object_name;
        detail["new_name"] = curr.object_name;

        return DetectedEvent{"object_renamed", detail.dump()};
    }
};

} // namespace

std::unique_ptr<ObjectRule> make_object_renamed_rule() {
    return std::make_unique<ObjectRenamedRule>();
}

} // namespace rule_engine
