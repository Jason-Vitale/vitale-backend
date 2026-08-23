#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "object_rule.hpp"
#include "object_rule_registry.hpp"

namespace rule_engine {
namespace {

class RcsSizeChangedRule : public ObjectRule {
public:
    std::optional<DetectedEvent> evaluate(const ObjectState& prev, const ObjectState& curr) const override {
        if (prev.rcs_size.empty() || curr.rcs_size.empty()) {
            return std::nullopt;
        }
        if (prev.rcs_size == curr.rcs_size) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["old_rcs_size"] = prev.rcs_size;
        detail["new_rcs_size"] = curr.rcs_size;

        return DetectedEvent{"rcs_size_changed", detail.dump()};
    }
};

} // namespace

std::unique_ptr<ObjectRule> make_rcs_size_changed_rule() {
    return std::make_unique<RcsSizeChangedRule>();
}

} // namespace rule_engine
