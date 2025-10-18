#include "json.hpp"

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
    nlohmann::json json;

    if (table.size() > 0) {
        json = nlohmann::json::array();
        for (unsigned int i = 1; i <= table.size(); i++)
            json.push_back(objectToJson(table[i]));

        return json;
    }

    table.for_each([&](sol::object const &key, sol::object const &value) {
        std::string jsonKey;
        if (key.is<std::string>()) jsonKey = key.as<std::string>();
        else if (key.is<double>()) jsonKey = key.as<double>();
        else if (key.is<int>()) jsonKey = std::to_string(key.as<int>());
        else if (key.is<bool>()) jsonKey = std::to_string(key.as<bool>());
        else {
            Log::logWarning("Unknown key type in table: " + std::to_string(static_cast<uint8_t>(key.get_type())));
            return;
        }

        json[jsonKey] = objectToJson(value);
    });

    return json;
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
} // namespace extensions::json
