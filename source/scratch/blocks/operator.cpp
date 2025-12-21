#include "operator.hpp"
#include "../math.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <math.h>

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

    double from = std::min(a, b);
    double to = std::max(a, b);

    if (value1.isScratchInt() && value2.isScratchInt()) {
        return Value(from + (rand() % static_cast<int>(to + 1 - from)));
    } else {
        return Value(from + rand() * (to - from) / (RAND_MAX + 1.0));
    }
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
        int index = std::floor(value1.asDouble() - 1);
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

    if (!value1.isNumeric() || !value2.isNumeric() || value2.asDouble() == 0.0)
        return Value(0);

    double a = value1.asDouble();
    double b = value2.asDouble();

    if (value1.isScratchInt() && value2.isScratchInt()) {
        int res = a - b * static_cast<int>(std::floor(static_cast<double>(a) / b));
        return Value(res);
    }

    double res = std::fmod(a, b);
    if ((res < 0 && b > 0) || (res > 0 && b < 0))
        res += b;

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
        double value = inputValue.asDouble();

        if (operation == "abs") {
            return Value(abs(value));
        }
        if (operation == "floor") {
            return Value(floor(value));
        }
        if (operation == "ceiling") {
            return Value(ceil(value));
        }
        if (operation == "sqrt") {
            return Value(sqrt(value));
        }
        if (operation == "sin") {
            return Value(std::round(std::sin(Math::degreesToRadians(value)) * 1e10) / 1e10);
        }
        if (operation == "cos") {
            return Value(std::round(std::cos(Math::degreesToRadians(value)) * 1e10) / 1e10);
        }
        if (operation == "tan") {
            double modAngle = std::fmod(value, 360.0);
            if (modAngle < -180.0) modAngle += 360.0;
            if (modAngle > 180.0) modAngle -= 360.0;

            if (modAngle == 90.0 || modAngle == -270.0) {
                return Value(std::numeric_limits<double>::infinity());
            }
            if (modAngle == -90.0 || modAngle == 270.0) {
                return Value(-std::numeric_limits<double>::infinity());
            }
            return Value(std::round(std::tan(Math::degreesToRadians(value)) * 1e10) / 1e10);
        }
        if (operation == "asin") {
            return Value(Math::radiansToDegrees(asin(value)));
        }
        if (operation == "acos") {
            return Value(Math::radiansToDegrees(acos(value)));
        }
        if (operation == "atan") {
            return Value(Math::radiansToDegrees(atan(value)));
        }
        if (operation == "ln") {
            return Value(log(value));
        }
        if (operation == "log") {
            return Value(log(value) / log(10));
        }
        if (operation == "e ^") {
            return Value(exp(value));
        }
        if (operation == "10 ^") {
            return Value(pow(10, value));
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
