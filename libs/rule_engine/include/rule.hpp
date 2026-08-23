#pragma once

#include "rule_base.hpp"
#include "snapshot.hpp"

namespace rule_engine {

// Compares two consecutive GP snapshots of the same object (prev.epoch <
// curr.epoch) and returns a DetectedEvent if this rule's condition fires.
using Rule = RuleBase<Snapshot>;

} // namespace rule_engine
