#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "object_rule.hpp"
#include "object_rule_registry.hpp"

namespace rule_engine {
namespace {

// SATCAT-based rather than GP-based: the GP poller's query excludes objects
// that already have a decay_date, so a GP snapshot pair can never observe
// the null -> non-null transition. The SATCAT poller's existing `objects`
// row (fetched before the upsert overwrites it) can.
class DecayDetectedRule : public ObjectRule {
public:
    std::optional<DetectedEvent> evaluate(const ObjectState& prev, const ObjectState& curr) const override {
        const bool flipped_to_decayed = !prev.decay_date.has_value() && curr.decay_date.has_value();
        if (!flipped_to_decayed) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["decay_date"] = *curr.decay_date;

        return DetectedEvent{"decay_detected", detail.dump()};
    }
};

} // namespace

std::unique_ptr<ObjectRule> make_decay_detected_rule() {
    return std::make_unique<DecayDetectedRule>();
}

} // namespace rule_engine
