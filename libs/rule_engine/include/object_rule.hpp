#pragma once

#include "object_state.hpp"
#include "rule_base.hpp"

namespace rule_engine {

// Compares two consecutive SATCAT-derived states of the same object's
// catalog metadata (the existing `objects` row, fetched immediately before
// an upsert overwrites it, vs. the freshly-parsed incoming row) and returns
// a DetectedEvent if this rule's condition fires.
using ObjectRule = RuleBase<ObjectState>;

} // namespace rule_engine
