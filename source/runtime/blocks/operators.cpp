#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <interpret.hpp>
#include <math.h>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_REPORTER_BLOCK(operator, add) {
    return Scratch::getInputValue(block, "NUM1", sprite) + Scratch::getInputValue(block, "NUM2", sprite);
}

SCRATCH_REPORTER_BLOCK(operator, subtract) {
    return Scratch::getInputValue(block, "NUM1", sprite) - Scratch::getInputValue(block, "NUM2", sprite);
}

SCRATCH_REPORTER_BLOCK(operator, multiply) {
    return Scratch::getInputValue(block, "NUM1", sprite) * Scratch::getInputValue(block, "NUM2", sprite);
}

SCRATCH_REPORTER_BLOCK(operator, divide) {
    return Scratch::getInputValue(block, "NUM1", sprite) / Scratch::getInputValue(block, "NUM2", sprite);
}

SCRATCH_REPORTER_BLOCK(operator, random) {
    Value value1 = Scratch::getInputValue(block, "FROM", sprite);
    Value value2 = Scratch::getInputValue(block, "TO", sprite);

    const double a = value1.asDouble();
    const double b = value2.asDouble();

    if (a == b) return Value(a);

    const double from = std::min(a, b);
    const double to = std::max(a, b);

    if (value1.isScratchInt() && value2.isScratchInt()) return Value(from + (rand() % static_cast<int>(to + 1 - from)));
    return Value(from + rand() * (to - from) / (RAND_MAX + 1.0));
}

SCRATCH_REPORTER_BLOCK(operator, join) {
    return Value(Scratch::getInputValue(block, "STRING1", sprite).asString() + Scratch::getInputValue(block, "STRING2", sprite).asString());
}

SCRATCH_REPORTER_BLOCK(operator, letter_of) {
    const Value value1 = Scratch::getInputValue(block, "LETTER", sprite);
    const Value value2 = Scratch::getInputValue(block, "STRING", sprite);

    if (!value1.isNumeric() || value2.asString() == "") return Value();

    const int index = std::floor(value1.asDouble()) - 1;
    if (index >= 0 && index < static_cast<int>(value2.asString().size())) {
        return Value(std::string(1, value2.asString()[index]));
    }

    return Value();
}

SCRATCH_REPORTER_BLOCK(operator, length) {
    return Value(static_cast<double>(Scratch::getInputValue(block, "STRING", sprite).asString().size()));
}

SCRATCH_REPORTER_BLOCK(operator, mod) {
    const Value value1 = Scratch::getInputValue(block, "NUM1", sprite);
    const Value value2 = Scratch::getInputValue(block, "NUM2", sprite);

    if (!value1.isNumeric() || !value2.isNumeric() || value2.asDouble() == 0.0)
        return Value(0);

    const double a = value1.asDouble();
    const double b = value2.asDouble();

    double res = std::fmod(a, b);
    if ((res < 0 && b > 0) || (res > 0 && b < 0))
        res += b;

    return Value(res);
}

SCRATCH_REPORTER_BLOCK(operator, round) {
    return Value(std::round(Scratch::getInputValue(block, "NUM", sprite).asDouble()));
}

SCRATCH_REPORTER_BLOCK(operator, mathop) {
    const Value inputValue = Scratch::getInputValue(block, "NUM", sprite);
    if (!inputValue.isNumeric()) return Value(0);

    const std::string operation = Scratch::getFieldValue(block, "OPERATOR");
    const double value = inputValue.asDouble();

    if (operation == "abs") return Value(abs(value));
    if (operation == "floor") return Value(floor(value));
    if (operation == "ceiling") return Value(ceil(value));
    if (operation == "sqrt") return Value(sqrt(value));
    if (operation == "sin") return Value(std::round(std::sin(Math::degreesToRadians(value)) * 1e10) / 1e10);
    if (operation == "cos") return Value(std::round(std::cos(Math::degreesToRadians(value)) * 1e10) / 1e10);
    if (operation == "tan") {
        double modAngle = std::fmod(value, 360.0);
        if (modAngle < -180.0) modAngle += 360.0;
        if (modAngle > 180.0) modAngle -= 360.0;

        if (modAngle == 90.0 || modAngle == -270.0) return Value(std::numeric_limits<double>::infinity());
        if (modAngle == -90.0 || modAngle == 270.0) return Value(-std::numeric_limits<double>::infinity());
        return Value(std::round(std::tan(Math::degreesToRadians(value)) * 1e10) / 1e10);
    }
    if (operation == "asin") return Value(Math::radiansToDegrees(asin(value)));
    if (operation == "acos") return Value(Math::radiansToDegrees(acos(value)));
    if (operation == "atan") return Value(Math::radiansToDegrees(atan(value)));
    if (operation == "ln") return Value(log(value));
    if (operation == "log") return Value(log(value) / log(10));
    if (operation == "e ^") return Value(exp(value));
    if (operation == "10 ^") return Value(pow(10, value));

    return Value(0);
}

SCRATCH_REPORTER_BLOCK(operator, equals) {
    return Value(Scratch::getInputValue(block, "OPERAND1", sprite) == Scratch::getInputValue(block, "OPERAND2", sprite));
}

SCRATCH_REPORTER_BLOCK(operator, gt) {
    return Value(Scratch::getInputValue(block, "OPERAND1", sprite) > Scratch::getInputValue(block, "OPERAND2", sprite));
}

SCRATCH_REPORTER_BLOCK(operator, lt) {
    return Value(Scratch::getInputValue(block, "OPERAND1", sprite) < Scratch::getInputValue(block, "OPERAND2", sprite));
}

SCRATCH_REPORTER_BLOCK(operator, and) {
    const auto oper1 = block.parsedInputs->find("OPERAND1");
    const auto oper2 = block.parsedInputs->find("OPERAND2");

    if (oper1 == block.parsedInputs->end() || oper2 == block.parsedInputs->end()) return Value(false);

    return Value(executor.getBlockValue(*findBlock(oper1->second.blockId), sprite).asBoolean() && executor.getBlockValue(*findBlock(oper2->second.blockId), sprite).asBoolean());
}

SCRATCH_REPORTER_BLOCK(operator, or) {
    bool result1 = false;
    bool result2 = false;

    const auto oper1 = block.parsedInputs->find("OPERAND1");
    if (oper1 != block.parsedInputs->end()) {
        const Value value1 = executor.getBlockValue(*findBlock(oper1->second.blockId), sprite);
        result1 = value1.asBoolean();
    }

    const auto oper2 = block.parsedInputs->find("OPERAND2");
    if (oper2 != block.parsedInputs->end()) {
        const Value value2 = executor.getBlockValue(*findBlock(oper2->second.blockId), sprite);
        result2 = value2.asBoolean();
    }

    return Value(result1 || result2);
}

SCRATCH_REPORTER_BLOCK(operator, not) {
    const auto oper = block.parsedInputs->find("OPERAND");
    if (oper == block.parsedInputs->end()) return Value(true);
    return Value(!executor.getBlockValue(*findBlock(oper->second.blockId), sprite).asBoolean());
}

SCRATCH_REPORTER_BLOCK(operator, contains) {
    return Value(Scratch::getInputValue(block, "STRING1", sprite).asString().find(Scratch::getInputValue(block, "STRING2", sprite).asString()) != std::string::npos);
}
