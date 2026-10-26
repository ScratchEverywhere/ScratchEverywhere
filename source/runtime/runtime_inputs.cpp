#include "runtime.hpp"
#include "blockExecutor.hpp"
#include "types.hpp"
#include <cmath>
#include <string>
#include <type_traits>
#include <utility>

static inline void materializeInputCache(ParsedInput &input) {
    switch (input.cacheKind) {
    case ParsedInput::CacheKind::Double:
        input.value = Value(input.cachedDouble);
        break;
    case ParsedInput::CacheKind::Bool:
        input.value = Value(input.cachedBool);
        break;
    case ParsedInput::CacheKind::Value:
        break;
    }
    input.cacheKind = ParsedInput::CacheKind::Value;
}

static inline double sanitizeNumber(double d) {
    return std::isnan(d) ? 0.0 : d;
}

bool Scratch::getInputValue(Block *block, const std::string &inputName, ScriptThread *thread, Sprite *sprite, Value &outValue) {
    const auto &input = block->inputMap.find(inputName);

    if (input == block->inputMap.end()) {
        const auto &field = block->fieldMap.find(inputName);

        if (field != block->fieldMap.end()) {
            outValue = Value(field->second->value);
        }
        return true;
    }

    if (block->recalculateInputs) {
        input->second->calculated = false;
    }

    switch (input->second->inputType) {
    case ParsedInput::InputType::VALUE:
        outValue = input->second->value;
        return true;
    case ParsedInput::InputType::VARIABLE:
        if (input->second->calculated) {
            outValue = input->second->value;
            return true;
        }

        input->second->calculated = true;
#ifdef ENABLE_CACHING
        if (input->second->variable != nullptr) {
            input->second->value = BlockExecutor::getVariableValueAs<Value>(input->second->variable);
        } else if (input->second->list) {
            input->second->value = BlockExecutor::getListValue(input->second->variableId, sprite);
        } else {
            input->second->value = BlockExecutor::getVariableValueAs<Value>(input->second->variableId, sprite);
        }
#else
        if (input->second->list) {
            input->second->value = BlockExecutor::getListValue(input->second->variableId, sprite);
        } else {
            input->second->value = BlockExecutor::getVariableValueAs<Value>(input->second->variableId, sprite);
        }
#endif
        outValue = input->second->value;
        return true;
    case ParsedInput::InputType::BLOCK: {
        if (input->second->calculated) {
            materializeInputCache(*input->second);
            outValue = input->second->value;
            return true;
        }
        if (input->second->block == nullptr) {
            return true;
        }

        Block *targetBlock = input->second->block;

        if (block->recalculateInputs) targetBlock->recalculateInputs = true;

        BlockResult res = targetBlock->blockFunction(targetBlock, thread, sprite, &(input->second->value));
        input->second->cacheKind = ParsedInput::CacheKind::Value;

        targetBlock->recalculateInputs = false;
        if (res != BlockResult::REPEAT) {
            input->second->calculated = true;
            outValue = input->second->value;
            return true;
        }
        return false;
    }
    }

    return true;
}

template <typename T>
bool Scratch::getInputValueAs(Block *block, const std::string &inputName, ScriptThread *thread, Sprite *sprite, T &outValue) {
    const auto &input = block->inputMap.find(inputName);

    if (input == block->inputMap.end()) {
        const auto &field = block->fieldMap.find(inputName);

        if (field != block->fieldMap.end()) {
            if constexpr (std::is_same_v<T, std::string>) {
                outValue = field->second->value;
            } else if constexpr (std::is_same_v<T, double>) {
                outValue = Value(field->second->value).asDouble();
            } else if constexpr (std::is_same_v<T, bool>) {
                outValue = Value(field->second->value).asBoolean();
            } else if constexpr (std::is_same_v<T, Color>) {
                outValue = Value(field->second->value).asColor();
            } else {
                outValue = Value(field->second->value);
            }
        }
        return true;
    }

    ParsedInput &in = *input->second;

    if (block->recalculateInputs) {
        in.calculated = false;
    }

    const auto readValue = [&outValue, &in]() {
        if constexpr (std::is_same_v<T, Value>) {
            outValue = in.value;
        } else {
            outValue = in.value.get<T>();
        }
    };

    switch (in.inputType) {
    case ParsedInput::InputType::VALUE:
        readValue();
        return true;
    case ParsedInput::InputType::VARIABLE:
#ifdef ENABLE_CACHING
        if (in.variable != nullptr) {
            outValue = BlockExecutor::getVariableValueAs<T>(in.variable);
            return true;
        }
#endif
        if (!in.calculated) {
            in.calculated = true;
            if (in.list) {
                in.value = BlockExecutor::getListValue(in.variableId, sprite);
            } else {
                outValue = BlockExecutor::getVariableValueAs<T>(in.variableId, sprite);
                return true;
            }
        }
        readValue();
        return true;
    case ParsedInput::InputType::BLOCK: {
        if (in.calculated) {
            if (in.cacheKind == ParsedInput::CacheKind::Double) {
                if constexpr (std::is_same_v<T, double>) {
                    outValue = sanitizeNumber(in.cachedDouble);
                    return true;
                }
            } else if (in.cacheKind == ParsedInput::CacheKind::Bool) {
                if constexpr (std::is_same_v<T, bool>) {
                    outValue = in.cachedBool;
                    return true;
                }
            }
            materializeInputCache(in);
            readValue();
            return true;
        }
        if (in.block == nullptr) {
            return true;
        }

        Block *targetBlock = in.block;

        if (block->recalculateInputs) targetBlock->recalculateInputs = true;

        BlockResult res;
        bool done = false;
        switch (targetBlock->blockFunction.type) {
        case Type::Value: {
            res = targetBlock->blockFunction.func.value(targetBlock, thread, sprite, &in.value);
            in.cacheKind = ParsedInput::CacheKind::Value;
            break;
        }
        case Type::Number: {
            double d = 0.0;
            res = targetBlock->blockFunction.func.number(targetBlock, thread, sprite, &d);
            if (res != BlockResult::REPEAT) {
                in.cachedDouble = d;
                in.cacheKind = ParsedInput::CacheKind::Double;
                if constexpr (std::is_same_v<T, double>) {
                    outValue = sanitizeNumber(d);
                    done = true;
                } else {
                    materializeInputCache(in);
                }
            }
            break;
        }
        case Type::Boolean: {
            bool b = false;
            res = targetBlock->blockFunction.func.boolean(targetBlock, thread, sprite, &b);
            if (res != BlockResult::REPEAT) {
                in.cachedBool = b;
                in.cacheKind = ParsedInput::CacheKind::Bool;
                if constexpr (std::is_same_v<T, bool>) {
                    outValue = b;
                    done = true;
                } else {
                    materializeInputCache(in);
                }
            }
            break;
        }
        case Type::String: {
            std::string str;
            res = targetBlock->blockFunction.func.string(targetBlock, thread, sprite, &str);
            if (res != BlockResult::REPEAT) {
                if constexpr (std::is_same_v<T, std::string>) {
                    outValue = str;
                    done = true;
                }
                in.value = Value(std::move(str));
                in.cacheKind = ParsedInput::CacheKind::Value;
            }
            break;
        }
        case Type::Color: {
            Color color{};
            res = targetBlock->blockFunction.func.color(targetBlock, thread, sprite, &color);
            if (res != BlockResult::REPEAT) {
                if constexpr (std::is_same_v<T, Color>) {
                    outValue = color;
                    done = true;
                }
                in.value = Value(color);
                in.cacheKind = ParsedInput::CacheKind::Value;
            }
            break;
        }
        }

        targetBlock->recalculateInputs = false;
        if (res == BlockResult::REPEAT) return false;

        in.calculated = true;
        if (!done) readValue();
        return true;
    }
    }

    return true;
}

#define GET_INPUT_VALUE_AS_TEMPLATE(T) \
    template bool Scratch::getInputValueAs<T>(Block *, const std::string &, ScriptThread *, Sprite *, T &outValue)

GET_INPUT_VALUE_AS_TEMPLATE(Value);
GET_INPUT_VALUE_AS_TEMPLATE(double);
GET_INPUT_VALUE_AS_TEMPLATE(std::string);
GET_INPUT_VALUE_AS_TEMPLATE(bool);
GET_INPUT_VALUE_AS_TEMPLATE(Color);

ParsedInput *Scratch::getInput(Block *block, const std::string &inputName) {
    const auto &input = block->inputMap.find(inputName);
    if (input != block->inputMap.end()) return input->second;

    return nullptr;
}

void Scratch::resetInput(Block *block, const std::string &inputName) {
    if (inputName.empty()) {
        block->recalculateInputs = true;
        return;
    }

    const auto &input = block->inputMap.find(inputName);
    if (input != block->inputMap.end()) input->second->calculated = false;
}

ParsedField *Scratch::getField(Block &block, const std::string &fieldName) {
    const auto &field = block.fieldMap.find(fieldName);
    if (field != block.fieldMap.end()) return field->second;

    return nullptr;
}

std::string Scratch::getFieldValue(Block &block, const std::string &fieldName) {
    const auto &field = block.fieldMap.find(fieldName);
    if (field != block.fieldMap.end()) return field->second->value;

    return "";
}

const std::string &Scratch::getFieldId(Block &block, const std::string &fieldName) {
    static const std::string empty;
    const auto &field = block.fieldMap.find(fieldName);
    if (field != block.fieldMap.end()) return field->second->id;

    return empty;
}

std::string Scratch::getListName(Block &block) {
    const auto &field = block.fieldMap.find("LIST");
    if (field != block.fieldMap.end()) return field->second->value;

    return "";
}

std::vector<Value> *Scratch::getListItems(Block &block, Sprite *sprite) {
    const std::string &listId = Scratch::getFieldId(block, "LIST");

    List *targetList = nullptr;
    if (sprite != nullptr) {
        auto it = sprite->lists.find(listId);
        if (it != sprite->lists.end()) targetList = &it->second;
    }
    {
        auto it = stageSprite->lists.find(listId);
        if (it != stageSprite->lists.end()) targetList = &it->second;
    }
    if (!targetList) {
        for (auto &[id, list] : stageSprite->lists) {
            if (list.name == getListName(block)) {
                targetList = &list;
                break;
            }
        }
        if (!targetList && sprite != nullptr) {
            for (auto &[id, list] : sprite->lists) {
                if (list.name == getListName(block)) {
                    targetList = &list;
                    break;
                }
            }
        }
    }
    if (!targetList && sprite) {
        List &newList = sprite->lists[listId];
        newList.id = listId;
        newList.name = getListName(block);
        targetList = &newList;
    }
    return targetList ? &targetList->items : nullptr;
}
