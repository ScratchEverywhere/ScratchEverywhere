#include "blockUtils.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <math.h>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(operator, add) {
    Value num1, num2;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInput(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 + num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, subtract) {
    Value num1, num2;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInput(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 - num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, multiply) {
    Value num1, num2;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInput(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 * num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, divide) {
    Value num1, num2;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInput(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 / num2;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, random) {
    Value fromValue, toValue;
    if (!Scratch::getInput(block, "FROM", thread, sprite, fromValue) ||
        !Scratch::getInput(block, "TO", thread, sprite, toValue)) return BlockResult::REPEAT;
    const double a = fromValue.asDouble();
    const double b = toValue.asDouble();
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
    if (!Scratch::getInput(block, "STRING1", thread, sprite, string1) ||
        !Scratch::getInput(block, "STRING2", thread, sprite, string2)) return BlockResult::REPEAT;
    *outValue = Value(string1.asString() + string2.asString());

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, letter_of) {
    Value letter, string;
    if (!Scratch::getInput(block, "LETTER", thread, sprite, letter) ||
        !Scratch::getInput(block, "STRING", thread, sprite, string)) return BlockResult::REPEAT;
    const std::string str = string.asString();
    const double letterValue = letter.asDouble();

    if (!letter.isNumeric() || str == "") {
        return BlockResult::CONTINUE;
    }
    const int targetIndex = std::floor(letterValue) - 1;
    if (targetIndex < 0) return BlockResult::CONTINUE;

    int currentCharIndex = 0;
    size_t byteIndex = 0;

    while (byteIndex < str.size()) {
        size_t charLength = 1;
        unsigned char c = str[byteIndex];

        if ((c & 0x80) == 0x00) charLength = 1;
        else if ((c & 0xE0) == 0xC0) charLength = 2;
        else if ((c & 0xF0) == 0xE0) charLength = 3;
        else if ((c & 0xF8) == 0xF0) charLength = 4;

        if (currentCharIndex == targetIndex) {
            if (byteIndex + charLength > str.size()) {
                charLength = str.size() - byteIndex;
            }

            *outValue = Value(str.substr(byteIndex, charLength));
            return BlockResult::CONTINUE;
        }

        currentCharIndex++;
        byteIndex += charLength;
    }

    *outValue = Value("");
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, length) {
    Value string;
    if (!Scratch::getInput(block, "STRING", thread, sprite, string)) return BlockResult::REPEAT;

    *outValue = Value(static_cast<double>(string.asString().size()));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, mod) {
    Value num1, num2;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInput(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;

    const double a = num1.asDouble();
    const double b = num2.asDouble();

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
    Value num;
    if (!Scratch::getInput(block, "NUM", thread, sprite, num)) return BlockResult::REPEAT;
    if (!num.isNumeric()) {
        *outValue = Value(0);
        return BlockResult::CONTINUE;
    }

    *outValue = Value(std::round(num.asDouble()));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, mathop) {
    Value num;
    if (!Scratch::getInput(block, "NUM", thread, sprite, num)) return BlockResult::REPEAT;

    if (!num.isNumeric()) {
        *outValue = Value(0);
        return BlockResult::CONTINUE;
    }

    const std::string operation = Scratch::getFieldValue(*block, "OPERATOR");

    const double value = num.asDouble();
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
    if (!Scratch::getInput(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInput(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 == op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, gt) {
    Value op1, op2;
    if (!Scratch::getInput(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInput(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 > op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, lt) {
    Value op1, op2;
    if (!Scratch::getInput(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInput(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1 < op2);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, and) {
    Value op1, op2;
    if (!Scratch::getInput(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInput(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1.asBoolean() and op2.asBoolean());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, or) {
    Value op1, op2;
    if (!Scratch::getInput(block, "OPERAND1", thread, sprite, op1) ||
        !Scratch::getInput(block, "OPERAND2", thread, sprite, op2)) return BlockResult::REPEAT;

    *outValue = Value(op1.asBoolean() || op2.asBoolean());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, not) {
    Value op1;
    if (!Scratch::getInput(block, "OPERAND", thread, sprite, op1)) return BlockResult::REPEAT;

    *outValue = Value(!op1.asBoolean());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(operator, contains) {
    Value string1Value, string2Value;
    if (!Scratch::getInput(block, "STRING1", thread, sprite, string1Value) ||
        !Scratch::getInput(block, "STRING2", thread, sprite, string2Value)) return BlockResult::REPEAT;

    std::string string1 = string1Value.asString();
    std::string string2 = string2Value.asString();

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