#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <math.h>
#include <types.hpp>
#include <value.hpp>

SCRATCH_BLOCK_DOUBLE(operator, add) {
    double num1, num2;
    if (!Scratch::getInputValueAs(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValueAs(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 + num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, subtract) {
    double num1, num2;
    if (!Scratch::getInputValueAs(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValueAs(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 - num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, multiply) {
    double num1, num2;
    if (!Scratch::getInputValueAs(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValueAs(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 * num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, divide) {
    double num1, num2;
    if (!Scratch::getInputValueAs(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInputValueAs(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 / num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, random) {
    Value fromValue, toValue;
    if (!Scratch::getInputValue(block, "FROM", thread, sprite, fromValue) ||
        !Scratch::getInputValue(block, "TO", thread, sprite, toValue)) return BlockResult::REPEAT;
    const double a = fromValue.get<double>();
    const double b = toValue.get<double>();
    if (a == b) {
        *outValue = a;

        return BlockResult::CONTINUE;
    }
    const double from = std::min(a, b);
    const double to = std::max(a, b);

    if (fromValue.isScratchInt() && toValue.isScratchInt())
        *outValue = from + (rand() % static_cast<int>(to + 1 - from));
    else
        *outValue = from + rand() * (to - from) / (RAND_MAX + 1.0);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_STRING(operator, join) {
    std::string string1, string2;
    if (!Scratch::getInputValueAs(block, "STRING1", thread, sprite, string1) ||
        !Scratch::getInputValueAs(block, "STRING2", thread, sprite, string2)) return BlockResult::REPEAT;
    *outValue = string1 + string2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_STRING(operator, letter_of) {
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
        *outValue = std::string(1, str[index]);
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, length) {
    Value strValue;
    if (!Scratch::getInputValue(block, "STRING", thread, sprite, strValue)) return BlockResult::REPEAT;

    const std::string *strPtr = strValue.tryGetStringRef();
    const size_t size = strPtr ? strPtr->size() : strValue.asString().size();

    *outValue = static_cast<double>(size);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, mod) {
    double a, b;
    if (!Scratch::getInputValueAs(block, "NUM1", thread, sprite, a) ||
        !Scratch::getInputValueAs(block, "NUM2", thread, sprite, b)) return BlockResult::REPEAT;

    if (b == 0.0) {
        *outValue = std::numeric_limits<double>::quiet_NaN();
        return BlockResult::CONTINUE;
    }

    double res = std::fmod(a, b);
    if ((res < 0 && b > 0) || (res > 0 && b < 0))
        res += b;
    *outValue = res;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, round) {
    double num;
    if (!Scratch::getInputValueAs(block, "NUM", thread, sprite, num)) return BlockResult::REPEAT;
    *outValue = std::round(num);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_DOUBLE(operator, mathop) {
    double value;
    if (!Scratch::getInputValueAs(block, "NUM", thread, sprite, value)) return BlockResult::REPEAT;

    const std::string operation = Scratch::getFieldValue(*block, "OPERATOR");

    if (operation == "abs") *outValue = abs(value);
    else if (operation == "floor") *outValue = floor(value);
    else if (operation == "ceiling") *outValue = ceil(value);
    else if (operation == "sqrt") *outValue = (sqrt(value));
    else if (operation == "sin") *outValue = std::round(std::sin(Math::degreesToRadians(value)) * 1e10) / 1e10;
    else if (operation == "cos") *outValue = std::round(std::cos(Math::degreesToRadians(value)) * 1e10) / 1e10;
    else if (operation == "tan") {
        double modAngle = std::fmod(value, 360.0);

        if (modAngle < -180.0) modAngle += 360.0;
        if (modAngle > 180.0) modAngle -= 360.0;

        if (modAngle == 90.0 || modAngle == -270.0) *outValue = std::numeric_limits<double>::infinity();
        else if (modAngle == -90.0 || modAngle == 270.0) *outValue = -std::numeric_limits<double>::infinity();
        else *outValue = std::round(std::tan(Math::degreesToRadians(value)) * 1e10) / 1e10;
    } else if (operation == "asin") *outValue = Math::radiansToDegrees(asin(value));
    else if (operation == "acos") *outValue = Math::radiansToDegrees(acos(value));
    else if (operation == "atan") *outValue = Math::radiansToDegrees(atan(value));
    else if (operation == "ln") *outValue = log(value);
    else if (operation == "log") *outValue = log(value) / log(10);
    else if (operation == "e ^") *outValue = exp(value);
    else if (operation == "10 ^") *outValue = pow(10, value);
    else *outValue = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, equals) {
    Value op1, op2;
    if (!Scratch::getInputValue(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValue(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;
    *outValue = op1 == op2;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, gt) {
    Value op1, op2;
    if (!Scratch::getInputValue(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValue(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;
    *outValue = op1 > op2;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, lt) {
    Value op1, op2;
    if (!Scratch::getInputValue(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValue(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;
    *outValue = op1 < op2;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, and) {
    bool op1, op2;
    if (!Scratch::getInputValueAs(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValueAs(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;
    *outValue = op1 && op2;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, or) {
    bool op1, op2;
    if (!Scratch::getInputValueAs(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInputValueAs(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;
    *outValue = op1 || op2;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, not) {
    bool op1;
    if (!Scratch::getInputValueAs(block, "OPERAND", thread, sprite, op1)) return BlockResult::REPEAT;
    *outValue = !op1;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_BOOLEAN(operator, contains) {
    std::string string1, string2;
    if (!Scratch::getInputValueAs(block, "STRING1", thread, sprite, string1) ||
        !Scratch::getInputValueAs(block, "STRING2", thread, sprite, string2)) return BlockResult::REPEAT;

    if (string2.empty()) {
        thread->eraseState(block);
        *outValue = true;
        return BlockResult::CONTINUE;
    }

    std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
    std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);

    thread->eraseState(block);
    *outValue = string1.find(string2) != std::string::npos;
    return BlockResult::CONTINUE;
}
