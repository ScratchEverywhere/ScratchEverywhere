#include "operator.hpp"
#include "math.hpp"
#include "random.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

Value OperatorBlocks::add(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    Value value2 = Scratch::getInputValue(block, "NUM2", sprite);
    return value1 + value2;
}

Value OperatorBlocks::subtract(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    Value value2 = Scratch::getInputValue(block, "NUM2", sprite);
    return value1 - value2;
}

Value OperatorBlocks::multiply(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    Value value2 = Scratch::getInputValue(block, "NUM2", sprite);
    return value1 * value2;
}

Value OperatorBlocks::divide(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    Value value2 = Scratch::getInputValue(block, "NUM2", sprite);
    return value1 / value2;
}

Value OperatorBlocks::random(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "FROM", sprite);
    Value value2 = Scratch::getInputValue(block, "TO", sprite);

    double a = value1.asDouble();
    double b = value2.asDouble();

    if (a == b) return Value(a);

    auto [from, to] = std::minmax(a, b);

    if (value1.isScratchInt() && value2.isScratchInt()) {
        return Value(Random::get().randomInt(static_cast<uint32_t>(from), static_cast<uint32_t>(to)));
    }

    return Value(Random::get().randomDouble(from, to));
}

Value OperatorBlocks::join(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "STRING1", sprite);
    Value value2 = Scratch::getInputValue(block, "STRING2", sprite);
    return Value(value1.asString() + value2.asString());
}

Value OperatorBlocks::letterOf(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "LETTER", sprite);
    Value value2 = Scratch::getInputValue(block, "STRING", sprite);
    if (value1.isNumeric() && value2.asString() != "") {
        int index = value1.asInt() - 1;
        if (index >= 0 && index < static_cast<int>(value2.asString().size())) {
            return Value(std::string(1, value2.asString()[index]));
        }
    }
    return Value();
}

Value OperatorBlocks::length(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "STRING", sprite);
    return Value(static_cast<double>(value1.asString().size()));
}

Value OperatorBlocks::mod(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    Value value2 = Scratch::getInputValue(block, "NUM2", sprite);

    if (!value1.isNumeric() || !value2.isNumeric() || value2.asDouble() == 0.0) return Value(0);

    double res = value1.asDouble() - value2.asDouble() * floor(value1.asDouble() / value2.asDouble());

    if (floor(value1.asDouble()) == value1.asDouble() && floor(value2.asDouble()) == value2.asDouble()) return Value(static_cast<int>(res));
    return Value(res);
}

Value OperatorBlocks::round(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM", sprite);
    return Value(std::round(value1.asDouble()));
}

Value OperatorBlocks::mathOp(Block &block, Sprite *sprite) {
    Value inputValue = Scratch::getInputValue(block, "NUM", sprite);
    if (inputValue.isNumeric()) {
        std::string operation = Scratch::getFieldValue(block, "OPERATOR");
        ;
        double value = inputValue.asDouble();

        if (operation == "abs") {
            return Value(std::abs(value));
        }
        if (operation == "floor") {
            return Value(static_cast<int>(std::floor(value)));
        }
        if (operation == "ceiling") {
            return Value(static_cast<int>(std::ceil(value)));
        }
        if (operation == "sqrt") {
            return Value(std::sqrt(value));
        }
        if (operation == "sin") {
            return Value(std::sin(value * M_PI / 180.0));
        }
        if (operation == "cos") {
            return Value(std::cos(value * M_PI / 180.0));
        }
        if (operation == "tan") {
            return Value(std::tan(value * M_PI / 180.0));
        }
        if (operation == "asin") {
            return Value(std::asin(value) * 180.0 / M_PI);
        }
        if (operation == "acos") {
            return Value(std::acos(value) * 180.0 / M_PI);
        }
        if (operation == "atan") {
            return Value(std::atan(value) * 180.0 / M_PI);
        }
        if (operation == "ln") {
            return Value(std::log(value));
        }
        if (operation == "log") {
            return Value(std::log10(value));
        }
        if (operation == "e ^") {
            return Value(std::exp(value));
        }
        if (operation == "10 ^") {
            return Value(std::pow(10, value));
        }
    }
    return Value(0);
}

Value OperatorBlocks::equals(Block &block, Sprite *sprite) {
    return Value(Scratch::getInputValue(block, "OPERAND1", sprite) == Scratch::getInputValue(block, "OPERAND2", sprite));
}

Value OperatorBlocks::greaterThan(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "OPERAND1", sprite);
    Value value2 = Scratch::getInputValue(block, "OPERAND2", sprite);
    return Value(value1 > value2);
}

Value OperatorBlocks::lessThan(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "OPERAND1", sprite);
    Value value2 = Scratch::getInputValue(block, "OPERAND2", sprite);
    return Value(value1 < value2);
}

Value OperatorBlocks::and_(Block &block, Sprite *sprite) {
    auto oper1 = block.parsedInputs->find("OPERAND1");
    auto oper2 = block.parsedInputs->find("OPERAND2");

    if (oper1 == block.parsedInputs->end() || oper2 == block.parsedInputs->end()) {
        return Value(false);
    }

    Value value1 = executor.getBlockValue(*findBlock(oper1->second.blockId), sprite);
    Value value2 = executor.getBlockValue(*findBlock(oper2->second.blockId), sprite);
    return Value(value1.asBoolean() && value2.asBoolean());
}

Value OperatorBlocks::or_(Block &block, Sprite *sprite) {
    bool result1 = false;
    bool result2 = false;

    auto oper1 = block.parsedInputs->find("OPERAND1");
    if (oper1 != block.parsedInputs->end()) {
        Value value1 = executor.getBlockValue(*findBlock(oper1->second.blockId), sprite);
        result1 = value1.asBoolean();
    }

    auto oper2 = block.parsedInputs->find("OPERAND2");
    if (oper2 != block.parsedInputs->end()) {
        Value value2 = executor.getBlockValue(*findBlock(oper2->second.blockId), sprite);
        result2 = value2.asBoolean();
    }

    return Value(result1 || result2);
}

Value OperatorBlocks::not_(Block &block, Sprite *sprite) {
    auto oper = block.parsedInputs->find("OPERAND");
    if (oper == block.parsedInputs->end()) {
        return Value(true);
    }
    Value value = executor.getBlockValue(*findBlock(oper->second.blockId), sprite);
    return Value(!value.asBoolean());
}

Value OperatorBlocks::contains(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "STRING1", sprite);
    Value value2 = Scratch::getInputValue(block, "STRING2", sprite);
    return Value(value1.asString().find(value2.asString()) != std::string::npos);
}
