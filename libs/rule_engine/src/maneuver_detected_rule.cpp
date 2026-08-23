#include <cmath>
#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "rule.hpp"
#include "rule_registry.hpp"

namespace rule_engine {
namespace {

constexpr double kInclinationDeltaThresholdDeg = 0.01;
constexpr double kSemimajorAxisDeltaThresholdKm = 1.0;

class ManeuverDetectedRule : public Rule {
public:
    std::optional<DetectedEvent> evaluate(const Snapshot& prev, const Snapshot& curr) const override {
        const double inclination_delta = std::fabs(curr.inclination - prev.inclination);
        const double semimajor_axis_delta = std::fabs(curr.semimajor_axis - prev.semimajor_axis);

        const bool inclination_fired = inclination_delta > kInclinationDeltaThresholdDeg;
        const bool semimajor_axis_fired = semimajor_axis_delta > kSemimajorAxisDeltaThresholdKm;

        if (!inclination_fired && !semimajor_axis_fired) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["inclination_delta_deg"] = inclination_delta;
        detail["semimajor_axis_delta_km"] = semimajor_axis_delta;
        detail["inclination_threshold_deg"] = kInclinationDeltaThresholdDeg;
        detail["semimajor_axis_threshold_km"] = kSemimajorAxisDeltaThresholdKm;
        detail["inclination_threshold_crossed"] = inclination_fired;
        detail["semimajor_axis_threshold_crossed"] = semimajor_axis_fired;
        detail["prev_epoch"] = prev.epoch;
        detail["curr_epoch"] = curr.epoch;

        return DetectedEvent{"maneuver_detected", detail.dump()};
    }
};

} // namespace

std::unique_ptr<Rule> make_maneuver_detected_rule() {
    return std::make_unique<ManeuverDetectedRule>();
}

} // namespace rule_engine
