#include "blockExecutor.hpp"
#include "math.hpp"
#include <memory>
#include <runtime.hpp>
#include <string>
#include <type_traits>
#include <variant>

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>

extern std::unique_ptr<MistConnection> cloudConnection;
#endif

void BlockExecutor::setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite) {
    const auto assignToVar = [&newValue](Variable &var) {
        std::visit([&newValue, &var](auto &&current) {
            using T = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<T, double>) {
                var.value = newValue.asDouble();
            } else if constexpr (std::is_same_v<T, std::shared_ptr<const std::string>>) {
                if (newValue.isString()) var.value = newValue.getStringPtr();
                else var.value = std::make_shared<const std::string>(newValue.asString());
            } else if constexpr (std::is_same_v<T, bool>) {
                var.value = newValue.asBoolean();
            } else if constexpr (std::is_same_v<T, Value>) {
                var.value = newValue;
            }
        },
                   var.value);
    };

    // Set sprite variable
    if (sprite != nullptr) {
        const auto it = sprite->variables.find(variableId);
        if (it != sprite->variables.end()) {
            assignToVar(it->second);
            return;
        }
    }

    // Set global variable
    auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        assignToVar(globalIt->second);
#ifdef ENABLE_CLOUDVARS
        if (globalIt->second.cloud) {
            if (cloudConnection) {
                cloudConnection->set(globalIt->second.name, getVariableValueAs<std::string>(&globalIt->second));
            }
        }
#endif
        return;
    }

    if (sprite != nullptr) {
        sprite->variables[variableId].value = newValue;
    }
}

Variable *BlockExecutor::getVariable(const std::string &variableId, Sprite *sprite) {
    // Check sprite variables
    if (sprite != nullptr) {
        const auto it = sprite->variables.find(variableId);
        if (it != sprite->variables.end()) return &it->second;
    }

    // Check global variables
    const auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        return &globalIt->second;
    }

    return nullptr;
}

template <typename T>
T BlockExecutor::getVariableValueAs(Variable *var) {
    if (!var) {
        if constexpr (std::is_same_v<T, Value>) return Value(0);
        else if constexpr (std::is_same_v<T, double>) return 0.0;
        else if constexpr (std::is_same_v<T, bool>) return false;
        else if constexpr (std::is_same_v<T, std::string>) return "0";
        else return Value(0).template get<T>();
    }

    return std::visit([](auto &&arg) -> T {
        using VarT = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value>) {
            if constexpr (std::is_same_v<VarT, Value>) return arg;
            else return Value(arg);
        } else if constexpr (std::is_same_v<T, std::string> && std::is_same_v<VarT, std::shared_ptr<const std::string>>) {
            return *arg;
        } else if constexpr (std::is_same_v<VarT, T> && std::is_same_v<T, double>) {
            return std::isnan(arg) ? 0.0 : arg;
        } else if constexpr (std::is_same_v<VarT, T>) {
            return arg;
        } else if constexpr (std::is_same_v<VarT, Value>) {
            return arg.template get<T>();
        } else {
            return Value(arg).template get<T>();
        }
    },
                      var->value);
}

template <typename T>
T BlockExecutor::getVariableValueAs(const std::string &variableId, Sprite *sprite) {
    // Check sprite variables
    if (sprite != nullptr) {
        const auto it = sprite->variables.find(variableId);
        if (it != sprite->variables.end()) {
            return getVariableValueAs<T>(&it->second);
        }
    }

    // Check global variables
    const auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        return getVariableValueAs<T>(&globalIt->second);
    }

    if (sprite != nullptr) {
        sprite->variables[variableId].value = Value(0);
        return getVariableValueAs<T>(&sprite->variables[variableId]);
    }

    return getVariableValueAs<T>(nullptr);
}

Value BlockExecutor::getVariableValue(const std::string &variableId, Sprite *sprite) {
    return getVariableValueAs<Value>(variableId, sprite);
}

#define GET_VARIABLE_VALUE_AS_TEMPLATE(T)                        \
    template T BlockExecutor::getVariableValueAs<T>(Variable *); \
    template T BlockExecutor::getVariableValueAs<T>(const std::string &, Sprite *)

GET_VARIABLE_VALUE_AS_TEMPLATE(Value);
GET_VARIABLE_VALUE_AS_TEMPLATE(double);
GET_VARIABLE_VALUE_AS_TEMPLATE(std::string);
GET_VARIABLE_VALUE_AS_TEMPLATE(bool);
GET_VARIABLE_VALUE_AS_TEMPLATE(Color);

Value BlockExecutor::getListValue(const std::string &listId, Sprite *sprite) {
    // Check sprite lists
    if (sprite != nullptr) {
        const auto listIt = sprite->lists.find(listId);
        if (listIt != sprite->lists.end()) {
            std::string result;
            std::string seperator = "";
            for (const auto &item : listIt->second.items) {
                if (!item.isString() || Math::utf16Length(item.get<std::string>()) > 1) {
                    seperator = " ";
                    break;
                }
            }
            for (const auto &item : listIt->second.items) {
                result += item.asString() + seperator;
            }
            if (!result.empty() && !seperator.empty()) result.pop_back();
            return Value(result);
        }
    }

    // Check global lists
    auto globalListIt = Scratch::stageSprite->lists.find(listId);
    if (globalListIt != Scratch::stageSprite->lists.end()) {
        std::string result;
        std::string seperator = "";
        for (const auto &item : globalListIt->second.items) {
            if (!item.isString() || Math::utf16Length(item.get<std::string>()) > 1) {
                seperator = " ";
                break;
            }
        }
        for (const auto &item : globalListIt->second.items) {
            result += item.asString() + seperator;
        }
        if (!result.empty() && !seperator.empty()) result.pop_back();
        return Value(result);
    }

    if (sprite != nullptr) {
        List newList;
        newList.id = listId;
        newList.items = {};
        sprite->lists[listId] = newList;
    }

    return Value("");
}

#ifdef ENABLE_CLOUDVARS
void BlockExecutor::handleCloudVariableChange(const std::string &name, const std::string &value) {
    if (!Scratch::stageSprite) return;
    for (auto it = Scratch::stageSprite->variables.begin(); it != Scratch::stageSprite->variables.end(); ++it) {
        if (it->second.name != name) continue;
        it->second.value = Value(value);
        return;
    }
}
#endif
