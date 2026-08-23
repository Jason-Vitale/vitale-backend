#include <cmath>
#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include "rule.hpp"
#include "rule_registry.hpp"

namespace rule_engine {
namespace {

// Below this, prev.bstar is treated as "zero" and a relative comparison
// would blow up (or divide by zero outright) -- fall back to the absolute
// threshold instead.
constexpr double kBstarNearZeroThreshold = 1e-6;
constexpr double kBstarPercentChangeThreshold = 0.5; // 50%
constexpr double kBstarAbsoluteDeltaThreshold = 0.0001;

class DragChangeRule : public Rule {
public:
    std::optional<DetectedEvent> evaluate(const Snapshot& prev, const Snapshot& curr) const override {
        const double delta = curr.bstar - prev.bstar;
        const bool prev_near_zero = std::fabs(prev.bstar) < kBstarNearZeroThreshold;

        bool fired;
        std::optional<double> percent_change;
        if (prev_near_zero) {
            fired = std::fabs(delta) > kBstarAbsoluteDeltaThreshold;
        } else {
            percent_change = delta / prev.bstar;
            fired = std::fabs(*percent_change) > kBstarPercentChangeThreshold;
        }

        if (!fired) {
            return std::nullopt;
        }

        nlohmann::json detail;
        detail["prev_bstar"] = prev.bstar;
        detail["curr_bstar"] = curr.bstar;
        detail["delta"] = delta;
        detail["used_absolute_fallback"] = prev_near_zero;
        detail["percent_change"] = percent_change ? nlohmann::json(*percent_change) : nlohmann::json(nullptr);
        detail["prev_epoch"] = prev.epoch;
        detail["curr_epoch"] = curr.epoch;

        return DetectedEvent{"drag_change", detail.dump()};
    }
};

} // namespace

std::unique_ptr<Rule> make_drag_change_rule() {
    return std::make_unique<DragChangeRule>();
}

} // namespace rule_engine
