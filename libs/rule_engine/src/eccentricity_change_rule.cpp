#include <cmath>
#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "rule.hpp"
#include "rule_registry.hpp"

namespace rule_engine {
namespace {

// Eccentricity is unitless (0-1 range), so this threshold is proportionally
// meaningful rather than an arbitrary small number.
constexpr double kEccentricityDeltaThreshold = 0.001;

class EccentricityChangeRule : public Rule {
public:
    std::optional<DetectedEvent> evaluate(const Snapshot& prev, const Snapshot& curr) const override {
        const double eccentricity_delta = std::fabs(curr.eccentricity - prev.eccentricity);
        if (eccentricity_delta <= kEccentricityDeltaThreshold) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["eccentricity_delta"] = eccentricity_delta;
        detail["threshold"] = kEccentricityDeltaThreshold;
        detail["prev_epoch"] = prev.epoch;
        detail["curr_epoch"] = curr.epoch;

        return DetectedEvent{"eccentricity_change", detail.dump()};
    }
};

} // namespace

std::unique_ptr<Rule> make_eccentricity_change_rule() {
    return std::make_unique<EccentricityChangeRule>();
}

} // namespace rule_engine
