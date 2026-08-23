#include "satcat_record.hpp"

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

int int_field(const nlohmann::json& j, const char* key) {
    const std::string s = str_field(j, key);
    return s.empty() ? 0 : std::stoi(s);
}

ObjectRecord parse_one(const nlohmann::json& j) {
    ObjectRecord obj;
    obj.norad_cat_id = int_field(j, "NORAD_CAT_ID");
    obj.object_name = str_field(j, "OBJECT_NAME");
    obj.object_id = str_field(j, "OBJECT_ID");
    obj.object_type = str_field(j, "OBJECT_TYPE");
    obj.country_code = str_field(j, "COUNTRY");
    obj.launch_date = opt_str_field(j, "LAUNCH");
    obj.site = str_field(j, "SITE");
    obj.rcs_size = str_field(j, "RCS_SIZE");
    obj.decay_date = opt_str_field(j, "DECAY");
    return obj;
}

} // namespace

std::vector<ObjectRecord> parse_satcat_response(const std::string& json_body) {
    const nlohmann::json parsed = nlohmann::json::parse(json_body);

    std::vector<ObjectRecord> records;
    records.reserve(parsed.size());
    for (const auto& entry : parsed) {
        records.push_back(parse_one(entry));
    }
    return records;
}

} // namespace vitale::poller
