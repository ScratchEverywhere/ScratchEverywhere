#include "operator.hpp"
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
    return Value(0);
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
    return Value(0);
}

Value OperatorBlocks::divide(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    Value value2 = Scratch::getInputValue(block, "NUM2", sprite);
    return value1 / value2;
    return Value(0);
}

Value OperatorBlocks::random(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "FROM", sprite);
    Value value2 = Scratch::getInputValue(block, "TO", sprite);

    if (value1.isNumeric() && value2.isNumeric()) {
        if (value1.isInteger() && value2.isInteger()) {
            int a = value1.asInt();
            int b = value2.asInt();
            int from = std::min(a, b);
            int to = std::max(a, b);
            return Value(rand() % (to - from + 1) + from);
        } else {
            double a = value1.asDouble();
            double b = value2.asDouble();
            double from = std::min(a, b);
            double to = std::max(a, b);
            return Value(from + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (to - from))));
        }
    }

    return Value(0);
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
    return Value(static_cast<int>(value1.asString().size()));
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
    if (value1.isNumeric()) {
        return Value(static_cast<int>(std::round(value1.asDouble())));
    }
    return Value(0);
}

Value OperatorBlocks::mathOp(Block &block, Sprite *sprite) {
    Value inputValue = Scratch::getInputValue(block, "NUM", sprite);
    if (inputValue.isNumeric()) {
        std::string operation = Scratch::getFieldValue(block, "OPERATOR");
        ;
        double value = inputValue.asDouble();

        if (operation == "abs") {
            return Value(abs(value));
        }
        if (operation == "floor") {
            return Value(static_cast<int>(floor(value)));
        }
        if (operation == "ceiling") {
            return Value(static_cast<int>(ceil(value)));
        }
        if (operation == "sqrt") {
            return Value(sqrt(value));
        }
        if (operation == "sin") {
            return Value(sin(value * M_PI / 180.0));
        }
        if (operation == "cos") {
            return Value(cos(value * M_PI / 180.0));
        }
        if (operation == "tan") {
            return Value(tan(value * M_PI / 180.0));
        }
        if (operation == "asin") {
            return Value(asin(value) * 180.0 / M_PI);
        }
        if (operation == "acos") {
            return Value(acos(value) * 180.0 / M_PI);
        }
        if (operation == "atan") {
            return Value(atan(value) * 180.0 / M_PI);
        }
        if (operation == "ln") {
            return Value(log(value));
        }
        if (operation == "log") {
            return Value(log10(value));
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
    Value value1;
    Value value2;
    try {
        value1 = Scratch::getInputValue(block, "OPERAND1", sprite);
        value2 = Scratch::getInputValue(block, "OPERAND2", sprite);
    } catch (...) {
        return Value(false);
    }

    if (value1.isNumeric() && value2.isNumeric()) {
        return Value(value1.asDouble() == value2.asDouble());
    } else {
        std::string string1 = value1.asString();
        std::string string2 = value2.asString();
        std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
        std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);
        return Value(string1 == string2);
    }
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
    return Value(value1.asInt() == 1 && value2.asInt() == 1);
}

Value OperatorBlocks::or_(Block &block, Sprite *sprite) {
    int result1 = 0;
    int result2 = 0;

    auto oper1 = block.parsedInputs->find("OPERAND1");
    if (oper1 != block.parsedInputs->end()) {
        Value value1 = executor.getBlockValue(*findBlock(oper1->second.blockId), sprite);
        result1 = value1.asInt();
    }

    auto oper2 = block.parsedInputs->find("OPERAND2");
    if (oper2 != block.parsedInputs->end()) {
        Value value2 = executor.getBlockValue(*findBlock(oper2->second.blockId), sprite);
        result2 = value2.asInt();
    }

    return Value(result1 == 1 || result2 == 1);
}

Value OperatorBlocks::not_(Block &block, Sprite *sprite) {
    auto oper = block.parsedInputs->find("OPERAND");
    if (oper == block.parsedInputs->end()) {
        return Value(true);
    }
    Value value = executor.getBlockValue(*findBlock(oper->second.blockId), sprite);
    return Value(value.asInt() != 1);
}

Value OperatorBlocks::contains(Block &block, Sprite *sprite) {
    Value value1 = Scratch::getInputValue(block, "STRING1", sprite);
    Value value2 = Scratch::getInputValue(block, "STRING2", sprite);
    return Value(value1.asString().find(value2.asString()) != std::string::npos);
}
