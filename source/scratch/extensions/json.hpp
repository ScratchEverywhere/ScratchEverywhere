#include "extensions.hpp"
#include <nlohmann/json.hpp>

namespace extensions::json {
sol::table jsonToTable(const nlohmann::json &json, sol::state_view &luaState);
nlohmann::json objectToJson(const sol::object &value);
nlohmann::json tableToJson(const sol::table table);

sol::table decode(const std::string data, sol::this_state s);
std::string encode(const sol::table data);
std::string encodePretty(const sol::table data);
} // namespace extensions::json
