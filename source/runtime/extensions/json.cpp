#include "json.hpp"
#include <log.hpp>

namespace extensions::json {
sol::table jsonToTable(const nlohmann::json &json, sol::state_view &luaState) {
    sol::table table = luaState.create_table();

    for (const auto &[key, value] : json.items()) {
        switch (value.type()) {
        case nlohmann::json::value_t::string:
            table[key] = value.get<std::string>();
            break;
        case nlohmann::json::value_t::number_integer:
            table[key] = value.get<int>();
            break;
        case nlohmann::json::value_t::number_unsigned:
            table[key] = value.get<unsigned int>();
            break;
        case nlohmann::json::value_t::number_float:
            table[key] = value.get<double>();
            break;
        case nlohmann::json::value_t::boolean:
            table[key] = value.get<bool>();
            break;
        case nlohmann::json::value_t::array:
        case nlohmann::json::value_t::object:
            table[key] = jsonToTable(value, luaState);
            break;
        default:
            break;
        }
    }

    return table;
}

nlohmann::json objectToJson(const sol::object &value) {
    if (value.is<std::string>()) return value.as<std::string>();
    else if (value.is<double>()) return value.as<double>();
    else if (value.is<int>()) return value.as<int>();
    else if (value.is<bool>()) return value.as<bool>();
    else if (value.is<sol::table>()) return tableToJson(value.as<sol::table>());
    else {
        Log::logWarning("Unknown object type: " + std::to_string(static_cast<uint8_t>(value.get_type())));
        return nlohmann::json();
    }
}

nlohmann::json tableToJson(const sol::table table) {
    nlohmann::json jsonObject = nlohmann::json::object();
    nlohmann::json jsonArray = nlohmann::json::array();
    bool isArray = true;
    size_t expectedIndex = 1;

    table.for_each([&](sol::object const &key, sol::object const &value) {
        if (!isArray) {
            if (key.is<std::string>()) jsonObject[key.as<std::string>()] = objectToJson(value);
            else if (key.is<int>()) jsonObject[std::to_string(key.as<int>())] = objectToJson(value);
            return;
        }

        if (key.is<int>() && key.as<int>() == expectedIndex) {
            jsonArray.push_back(objectToJson(value));
            expectedIndex++;
        } else {
            isArray = false;
            for (size_t i = 0; i < jsonArray.size(); ++i) {
                jsonObject[std::to_string(i + 1)] = jsonArray[i];
            }
            if (key.is<std::string>()) jsonObject[key.as<std::string>()] = objectToJson(value);
            else if (key.is<int>()) jsonObject[std::to_string(key.as<int>())] = objectToJson(value);
        }
    });

    return isArray ? jsonArray : jsonObject;
}

sol::table decode(const std::string data, sol::this_state s) {
    if (!nlohmann::json::accept(data)) return sol::lua_nil;

    sol::state_view luaState = s.lua_state();
    nlohmann::json json = nlohmann::json::parse(data);
    return jsonToTable(json, luaState);
}

std::string encode(const sol::table table) {
    return tableToJson(table).dump();
}

std::string encodePretty(const sol::table table) {
    return tableToJson(table).dump(2);
}

void registerAPI(sol::state_view &luaState) {
    luaState["json"] = luaState.create_table();
    luaState["json"]["decode"] = json::decode;
    luaState["json"]["encode"] = json::encode;
    luaState["json"]["encodePretty"] = json::encodePretty;
}
} // namespace extensions::json
