#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "rule.hpp"
#include "rule_registry.hpp"

namespace rule_engine {
namespace {

class DecayDetectedRule : public Rule {
public:
    std::optional<DetectedEvent> evaluate(const Snapshot& prev, const Snapshot& curr) const override {
        const bool flipped_to_decayed = !prev.decay_date.has_value() && curr.decay_date.has_value();
        if (!flipped_to_decayed) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["decay_date"] = *curr.decay_date;
        detail["prev_epoch"] = prev.epoch;
        detail["curr_epoch"] = curr.epoch;

        return DetectedEvent{"decay_detected", detail.dump()};
    }
};

} // namespace

std::unique_ptr<Rule> make_decay_detected_rule() {
    return std::make_unique<DecayDetectedRule>();
}

} // namespace rule_engine
