#pragma once
#include <se_export.hpp>

#include "meta.hpp"
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace extensions::json {
SE_EXPORT sol::table jsonToTable(const nlohmann::json &json, sol::state_view &luaState);
SE_EXPORT nlohmann::json objectToJson(const sol::object &value);
SE_EXPORT nlohmann::json tableToJson(const sol::table table);

SE_EXPORT sol::table decode(const std::string data, sol::this_state s);
SE_EXPORT std::string encode(const sol::table data);
SE_EXPORT std::string encodePretty(const sol::table data);

SE_EXPORT void registerAPI(Extension *extension);
} // namespace extensions::json
