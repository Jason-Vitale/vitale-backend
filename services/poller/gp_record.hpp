#pragma once

#include <string>
#include <vector>

#include "db_writer.hpp"
#include "snapshot.hpp"

namespace vitale::poller {

struct GpRecord {
    ObjectRecord object;
    rule_engine::Snapshot snapshot;
};

// Parses a raw JSON array response from Space-Track's gp query (as returned
// by SpaceTrackClient::fetch_gp) into one GpRecord per element. Every GP
// field comes back as a JSON string (or null), matching the schema
// confirmed against /basicspacedata/modeldef/class/gp/format/json.
std::vector<GpRecord> parse_gp_response(const std::string& json_body);

} // namespace vitale::poller
