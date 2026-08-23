#include <cmath>
#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "rule.hpp"
#include "rule_registry.hpp"

namespace rule_engine {
namespace {

constexpr double kRaanDeltaThresholdDeg = 0.01;

// Separate event type from ManeuverDetectedRule: a plane-change/RAAN shift
// is meaningfully different from an inclination/altitude burn.
class RaanShiftRule : public Rule {
public:
    std::optional<DetectedEvent> evaluate(const Snapshot& prev, const Snapshot& curr) const override {
        const double raan_delta = std::fabs(curr.ra_of_asc_node - prev.ra_of_asc_node);
        if (raan_delta <= kRaanDeltaThresholdDeg) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["ra_of_asc_node_delta_deg"] = raan_delta;
        detail["threshold_deg"] = kRaanDeltaThresholdDeg;
        detail["prev_epoch"] = prev.epoch;
        detail["curr_epoch"] = curr.epoch;

        return DetectedEvent{"raan_shift", detail.dump()};
    }
};

} // namespace

std::unique_ptr<Rule> make_raan_shift_rule() {
    return std::make_unique<RaanShiftRule>();
}

} // namespace rule_engine
