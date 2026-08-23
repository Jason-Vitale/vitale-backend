#pragma once

#include <string>
#include <vector>

#include "db_writer.hpp"

namespace vitale::poller {

// Parses a raw JSON array response from Space-Track's satcat query into one
// ObjectRecord per element.
//
// NOTE: field names below (OBJECT_NAME, OBJECT_ID, OBJECT_TYPE, COUNTRY,
// LAUNCH, SITE, RCS_SIZE, DECAY, NORAD_CAT_ID) are Space-Track's documented
// satcat class fields, but unlike gp_record.cpp's field mapping, this has
// NOT yet been checked against a real satcat response -- confirm on first
// live SatcatPoller run and adjust if any names are off.
std::vector<ObjectRecord> parse_satcat_response(const std::string& json_body);

} // namespace vitale::poller
