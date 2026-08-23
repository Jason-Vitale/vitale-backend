#include "gp_record.hpp"

#include <nlohmann/json.hpp>

namespace vitale::poller {

namespace {

std::string str_field(const nlohmann::json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) {
        return "";
    }
    return j.at(key).get<std::string>();
}

std::optional<std::string> opt_str_field(const nlohmann::json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) {
        return std::nullopt;
    }
    return j.at(key).get<std::string>();
}

double double_field(const nlohmann::json& j, const char* key) {
    const std::string s = str_field(j, key);
    return s.empty() ? 0.0 : std::stod(s);
}

int int_field(const nlohmann::json& j, const char* key) {
    const std::string s = str_field(j, key);
    return s.empty() ? 0 : std::stoi(s);
}

std::int64_t int64_field(const nlohmann::json& j, const char* key) {
    const std::string s = str_field(j, key);
    return s.empty() ? 0 : std::stoll(s);
}

GpRecord parse_one(const nlohmann::json& j) {
    GpRecord rec;

    const int norad_cat_id = int_field(j, "NORAD_CAT_ID");
    const std::optional<std::string> decay_date = opt_str_field(j, "DECAY_DATE");

    rec.object.norad_cat_id = norad_cat_id;
    rec.object.object_name = str_field(j, "OBJECT_NAME");
    rec.object.object_id = str_field(j, "OBJECT_ID");
    rec.object.object_type = str_field(j, "OBJECT_TYPE");
    rec.object.country_code = str_field(j, "COUNTRY_CODE");
    rec.object.launch_date = opt_str_field(j, "LAUNCH_DATE");
    rec.object.site = str_field(j, "SITE");
    rec.object.rcs_size = str_field(j, "RCS_SIZE");
    rec.object.decay_date = decay_date;

    rec.snapshot.norad_cat_id = norad_cat_id;
    rec.snapshot.gp_id = int64_field(j, "GP_ID");
    rec.snapshot.epoch = str_field(j, "EPOCH");
    rec.snapshot.mean_motion = double_field(j, "MEAN_MOTION");
    rec.snapshot.eccentricity = double_field(j, "ECCENTRICITY");
    rec.snapshot.inclination = double_field(j, "INCLINATION");
    rec.snapshot.ra_of_asc_node = double_field(j, "RA_OF_ASC_NODE");
    rec.snapshot.arg_of_pericenter = double_field(j, "ARG_OF_PERICENTER");
    rec.snapshot.mean_anomaly = double_field(j, "MEAN_ANOMALY");
    rec.snapshot.semimajor_axis = double_field(j, "SEMIMAJOR_AXIS");
    rec.snapshot.period = double_field(j, "PERIOD");
    rec.snapshot.apoapsis = double_field(j, "APOAPSIS");
    rec.snapshot.periapsis = double_field(j, "PERIAPSIS");
    rec.snapshot.bstar = double_field(j, "BSTAR");
    rec.snapshot.mean_motion_dot = double_field(j, "MEAN_MOTION_DOT");
    rec.snapshot.mean_motion_ddot = double_field(j, "MEAN_MOTION_DDOT");
    rec.snapshot.element_set_no = int_field(j, "ELEMENT_SET_NO");
    rec.snapshot.rev_at_epoch = int_field(j, "REV_AT_EPOCH");
    rec.snapshot.tle_line0 = str_field(j, "TLE_LINE0");
    rec.snapshot.tle_line1 = str_field(j, "TLE_LINE1");
    rec.snapshot.tle_line2 = str_field(j, "TLE_LINE2");
    rec.snapshot.decay_date = decay_date;

    return rec;
}

} // namespace

std::vector<GpRecord> parse_gp_response(const std::string& json_body) {
    const nlohmann::json parsed = nlohmann::json::parse(json_body);

    std::vector<GpRecord> records;
    records.reserve(parsed.size());
    for (const auto& entry : parsed) {
        records.push_back(parse_one(entry));
    }
    return records;
}

} // namespace vitale::poller
