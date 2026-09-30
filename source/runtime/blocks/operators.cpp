#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <math.h>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(operator, add) {
    Value num1, num2;
    if (!Scratch::getInputValue(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValue(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 + num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, subtract) {
    Value num1, num2;
    if (!Scratch::getInputValue(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValue(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 - num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, multiply) {
    Value num1, num2;
    if (!Scratch::getInputValue(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValue(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 * num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, divide) {
    Value num1, num2;
    if (!Scratch::getInputValue(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValue(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 / num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, random) {
    Value fromValue, toValue;
    if (!Scratch::getInputValue(block, "FROM", thread, sprite, fromValue) ||
        !Scratch::getInputValue(block, "TO", thread, sprite, toValue)) return BlockResult::REPEAT;
    const double a = fromValue.get<double>();
    const double b = toValue.get<double>();
    if (a == b) {
        *outValue = fromValue;

        return BlockResult::CONTINUE;
    }
    const double from = std::min(a, b);
    const double to = std::max(a, b);

    if (fromValue.isScratchInt() && toValue.isScratchInt())
        *outValue = Value(from + (rand() % static_cast<int>(to + 1 - from)));
    else
        *outValue = Value(from + rand() * (to - from) / (RAND_MAX + 1.0));

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, join) {
    Value string1, string2;
    if (!Scratch::getInputValue(block, "STRING1", thread, sprite, string1) ||
        !Scratch::getInputValue(block, "STRING2", thread, sprite, string2)) return BlockResult::REPEAT;
    *outValue = Value(string1.get<std::string>() + string2.get<std::string>());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, letter_of) {
    double letter;
    Value strValue;
    if (!Scratch::getInputValueAs(block, "LETTER", thread, sprite, letter) ||
        !Scratch::getInputValue(block, "STRING", thread, sprite, strValue)) return BlockResult::REPEAT;

    std::string ownedStr;
    const std::string *strPtr = strValue.tryGetStringRef();
    if (!strPtr) {
        ownedStr = strValue.asString();
        strPtr = &ownedStr;
    }
    const std::string &str = *strPtr;

    if (str.empty()) {
        return BlockResult::CONTINUE;
    }
    const int index = std::floor(letter) - 1;
    if (index >= 0 && index < static_cast<int>(str.size())) {
        *outValue = Value(std::string(1, str[index]));
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, length) {
    Value strValue;
    if (!Scratch::getInputValue(block, "STRING", thread, sprite, strValue)) return BlockResult::REPEAT;

    const std::string *strPtr = strValue.tryGetStringRef();
    const size_t size = strPtr ? strPtr->size() : strValue.asString().size();

    *outValue = Value(static_cast<double>(size));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, mod) {
    double a, b;
    if (!Scratch::getInputValueAs(block, "NUM1", thread, sprite, a) ||
        !Scratch::getInputValueAs(block, "NUM2", thread, sprite, b)) return BlockResult::REPEAT;

    if (b == 0.0) {
        *outValue = Value(std::numeric_limits<double>::quiet_NaN());
        return BlockResult::CONTINUE;
    }

    double res = std::fmod(a, b);
    if ((res < 0 && b > 0) || (res > 0 && b < 0))
        res += b;
    *outValue = Value(res);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, round) {
    double num;
    if (!Scratch::getInputValueAs(block, "NUM", thread, sprite, num)) return BlockResult::REPEAT;

    *outValue = Value(std::round(num));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, mathop) {
    double value;
    if (!Scratch::getInputValueAs(block, "NUM", thread, sprite, value)) return BlockResult::REPEAT;

    const std::string operation = Scratch::getFieldValue(*block, "OPERATOR");

    if (operation == "abs") *outValue = Value(abs(value));
    else if (operation == "floor") *outValue = Value(floor(value));
    else if (operation == "ceiling") *outValue = Value(ceil(value));
    else if (operation == "sqrt") *outValue = Value(sqrt(value));
    else if (operation == "sin") *outValue = Value(std::round(std::sin(Math::degreesToRadians(value)) * 1e10) / 1e10);
    else if (operation == "cos") *outValue = Value(std::round(std::cos(Math::degreesToRadians(value)) * 1e10) / 1e10);
    else if (operation == "tan") {
        double modAngle = std::fmod(value, 360.0);

        if (modAngle < -180.0) modAngle += 360.0;
        if (modAngle > 180.0) modAngle -= 360.0;

        if (modAngle == 90.0 || modAngle == -270.0) *outValue = Value(std::numeric_limits<double>::infinity());
        else if (modAngle == -90.0 || modAngle == 270.0) *outValue = Value(-std::numeric_limits<double>::infinity());
        else *outValue = Value(std::round(std::tan(Math::degreesToRadians(value)) * 1e10) / 1e10);
    } else if (operation == "asin") *outValue = Value(Math::radiansToDegrees(asin(value)));
    else if (operation == "acos") *outValue = Value(Math::radiansToDegrees(acos(value)));
    else if (operation == "atan") *outValue = Value(Math::radiansToDegrees(atan(value)));
    else if (operation == "ln") *outValue = Value(log(value));
    else if (operation == "log") *outValue = Value(log(value) / log(10));
    else if (operation == "e ^") *outValue = Value(exp(value));
    else if (operation == "10 ^") *outValue = Value(pow(10, value));
    else *outValue = Value(0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, equals) {
    Value op1, op2;
    if (!Scratch::getInputValue(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValue(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 == op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, gt) {
    Value op1, op2;
    if (!Scratch::getInputValue(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValue(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 > op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, lt) {
    Value op1, op2;
    if (!Scratch::getInputValue(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValue(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 < op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, and) {
    bool op1, op2;
    if (!Scratch::getInputValueAs(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValueAs(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 && op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, or) {
    bool op1, op2;
    if (!Scratch::getInputValueAs(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValueAs(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 || op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, not) {
    bool op1;
    if (!Scratch::getInputValueAs(block, "OPERAND", thread, sprite, op1)) return BlockResult::REPEAT;

    *outValue = Value(!op1);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, contains) {
    std::string string1, string2;
    if (!Scratch::getInputValueAs(block, "STRING1", thread, sprite, string1) ||
        !Scratch::getInputValueAs(block, "STRING2", thread, sprite, string2)) return BlockResult::REPEAT;

    if (string2.empty()) {
        thread->eraseState(block);
        *outValue = Value(true);
        return BlockResult::CONTINUE;
    }

    std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
    std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);

    thread->eraseState(block);
    *outValue = Value(string1.find(string2) != std::string::npos);
    return BlockResult::CONTINUE;
}
