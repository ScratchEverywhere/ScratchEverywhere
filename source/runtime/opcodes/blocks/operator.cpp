#include "../../core/math.hpp"
#include "../opcode_registers.hpp"
#include "../opcodes.hpp"
#include "compiler_context.hpp"

REGISTER_STANDARD_PURE_PARSER("operator_add", operator_add, "NUM1", "NUM2")
DEFINE_EXECUTION(operator_add) {
    Value numValue2 = thread->stack.back();
    thread->stack.pop_back();
    Value numValue1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(numValue1 + numValue2);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_subtract", operator_subtract, "NUM1", "NUM2")
DEFINE_EXECUTION(operator_subtract) {
    Value numValue2 = thread->stack.back();
    thread->stack.pop_back();
    Value numValue1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(numValue1 - numValue2);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_multiply", operator_multiply, "NUM1", "NUM2")
DEFINE_EXECUTION(operator_multiply) {
    Value numValue2 = thread->stack.back();
    thread->stack.pop_back();
    Value numValue1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(numValue1 * numValue2);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_divide", operator_divide, "NUM1", "NUM2")
DEFINE_EXECUTION(operator_divide) {
    Value numValue2 = thread->stack.back();
    thread->stack.pop_back();
    Value numValue1 = thread->stack.back();
    thread->stack.pop_back();
    numValue1 = Value(numValue1.asDouble() / numValue2.asDouble());
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PARSER("operator_random", operator_random, "FROM", "TO")
DEFINE_EXECUTION(operator_random) {
    Value toValue = thread->stack.back();
    thread->stack.pop_back();
    Value fromValue = thread->stack.back();
    thread->stack.pop_back();
    const double a = fromValue.asDouble();
    const double b = toValue.asDouble();
    if (a == b) {
        thread->stack.emplace_back(fromValue);
        return BlockResult::CONTINUE;
    }
    const double from = std::min(a, b);
    const double to = std::max(a, b);
    if (fromValue.isScratchInt() && toValue.isScratchInt()) {
        thread->stack.emplace_back(from + (rand() % static_cast<int>(to + 1 - from)));
    } else {
        thread->stack.emplace_back(from + rand() * (to - from) / (RAND_MAX + 1.0));
    }
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_join", operator_join, "STRING1", "STRING2")
DEFINE_EXECUTION(operator_join) {
    Value str2Value = thread->stack.back();
    thread->stack.pop_back();
    Value str1Value = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(str1Value.asString() + str2Value.asString());
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_letter_of", operator_letterof, "STRING", "LETTER")
DEFINE_EXECUTION(operator_letterof) {
    Value indexValue = thread->stack.back();
    thread->stack.pop_back();
    std::string strValue = thread->stack.back().asString();
    thread->stack.pop_back();
    if (!indexValue.isNumeric() || strValue.empty()) {
        thread->stack.emplace_back("");
        return BlockResult::CONTINUE;
    }
    const int index = static_cast<int>(std::floor(indexValue.asDouble())) - 1;
    if (index >= 0 && index < static_cast<int>(strValue.size())) {
        thread->stack.emplace_back(std::string(1, strValue[index]));
    } else {
        thread->stack.emplace_back("");
    }
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_length", operator_length, "STRING")
DEFINE_EXECUTION(operator_length) {
    Value strValue = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(static_cast<double>(strValue.asString().length()));
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_mod", operator_mod, "NUM1", "NUM2")
DEFINE_EXECUTION(operator_mod) {
    Value numValue2 = thread->stack.back();
    thread->stack.pop_back();
    Value numValue1 = thread->stack.back();
    thread->stack.pop_back();
    const double a = numValue1.asDouble();
    const double b = numValue2.asDouble();
    if (b == 0.0) {
        thread->stack.emplace_back(std::numeric_limits<double>::quiet_NaN());
        return BlockResult::CONTINUE;
    }
    double res = std::fmod(a, b);
    if ((res < 0 && b > 0) || (res > 0 && b < 0)) {
        res += b;
    }
    thread->stack.emplace_back(res);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_round", operator_round, "NUM")
DEFINE_EXECUTION(operator_round) {
    Value numValue = thread->stack.back();
    thread->stack.pop_back();
    if (!numValue.isNumeric()) {
        thread->stack.emplace_back(0.0);
        return BlockResult::CONTINUE;
    }
    thread->stack.emplace_back(std::round(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_equals", operator_equals, "OPERAND1", "OPERAND2")
DEFINE_EXECUTION(operator_equals) {
    Value operand2 = thread->stack.back();
    thread->stack.pop_back();
    Value operand1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(operand1 == operand2);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_gt", operator_greater_than, "OPERAND1", "OPERAND2")
DEFINE_EXECUTION(operator_greater_than) {
    Value operand2 = thread->stack.back();
    thread->stack.pop_back();
    Value operand1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(operand1 > operand2);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_lt", operator_less_than, "OPERAND1", "OPERAND2")
DEFINE_EXECUTION(operator_less_than) {
    Value operand2 = thread->stack.back();
    thread->stack.pop_back();
    Value operand1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(operand1 < operand2);
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_and", operator_and, "OPERAND1", "OPERAND2")
DEFINE_EXECUTION(operator_and) {
    Value operand2 = thread->stack.back();
    thread->stack.pop_back();
    Value operand1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(operand1.asBoolean() && operand2.asBoolean());
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_or", operator_or, "OPERAND1", "OPERAND2")
DEFINE_EXECUTION(operator_or) {
    Value operand2 = thread->stack.back();
    thread->stack.pop_back();
    Value operand1 = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(operand1.asBoolean() || operand2.asBoolean());
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_not", operator_not, "OPERAND")
DEFINE_EXECUTION(operator_not) {
    Value operand = thread->stack.back();
    thread->stack.pop_back();
    thread->stack.emplace_back(!operand.asBoolean());
    return BlockResult::CONTINUE;
}
REGISTER_STANDARD_PURE_PARSER("operator_contains", operator_contains, "STRING1", "STRING2")
DEFINE_EXECUTION(operator_contains) {
    Value str2Value = thread->stack.back();
    thread->stack.pop_back();
    Value str1Value = thread->stack.back();
    thread->stack.pop_back();
    std::string string1 = str1Value.asString();
    std::string string2 = str2Value.asString();
    if (string2.empty()) {
        thread->stack.emplace_back(true);
        return BlockResult::CONTINUE;
    }
    std::transform(string1.begin(), string1.end(), string1.begin(), ::tolower);
    std::transform(string2.begin(), string2.end(), string2.begin(), ::tolower);
    thread->stack.emplace_back(string1.find(string2) != std::string::npos);
    return BlockResult::CONTINUE;
}

DEFINE_CUSTOM_PARSER("operator_mathop", mathop_parser) {
    const std::string mathFunc = ctx.resolveFieldValue("OPERATOR");
    if (mathFunc == "abs") return ParserRegistry::compileStandard(ctx, operator_mathop_abs, {"NUM"}, Purity::Pure);
    else if (mathFunc == "floor") return ParserRegistry::compileStandard(ctx, operator_mathop_floor, {"NUM"}, Purity::Pure);
    else if (mathFunc == "ceiling") return ParserRegistry::compileStandard(ctx, operator_mathop_ceil, {"NUM"}, Purity::Pure);
    else if (mathFunc == "sqrt") return ParserRegistry::compileStandard(ctx, operator_mathop_sqrt, {"NUM"}, Purity::Pure);
    else if (mathFunc == "sin") return ParserRegistry::compileStandard(ctx, operator_mathop_sin, {"NUM"}, Purity::Pure);
    else if (mathFunc == "cos") return ParserRegistry::compileStandard(ctx, operator_mathop_cos, {"NUM"}, Purity::Pure);
    else if (mathFunc == "tan") return ParserRegistry::compileStandard(ctx, operator_mathop_tan, {"NUM"}, Purity::Pure);
    else if (mathFunc == "asin") return ParserRegistry::compileStandard(ctx, operator_mathop_asin, {"NUM"}, Purity::Pure);
    else if (mathFunc == "acos") return ParserRegistry::compileStandard(ctx, operator_mathop_acos, {"NUM"}, Purity::Pure);
    else if (mathFunc == "atan") return ParserRegistry::compileStandard(ctx, operator_mathop_atan, {"NUM"}, Purity::Pure);
    else if (mathFunc == "ln") return ParserRegistry::compileStandard(ctx, operator_mathop_ln, {"NUM"}, Purity::Pure);
    else if (mathFunc == "log") return ParserRegistry::compileStandard(ctx, operator_mathop_log, {"NUM"}, Purity::Pure);
    else if (mathFunc == "e ^") return ParserRegistry::compileStandard(ctx, operator_mathop_epow, {"NUM"}, Purity::Pure);
    else if (mathFunc == "10 ^") return ParserRegistry::compileStandard(ctx, operator_mathop_10pow, {"NUM"}, Purity::Pure);
    else return ParserRegistry::compileStandard(ctx, operator_mathop_abs, {"NUM"}, Purity::Pure);
}

DEFINE_EXECUTION(operator_mathop_abs) {
    Value &numValue = thread->stack.back();
    numValue = Value(abs(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_floor) {
    Value &numValue = thread->stack.back();
    numValue = Value(floor(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_ceil) {
    Value &numValue = thread->stack.back();
    numValue = Value(ceil(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_sqrt) {
    Value &numValue = thread->stack.back();
    numValue = Value(sqrt(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_sin) {
    Value &numValue = thread->stack.back();
    numValue = Value(std::round(std::sin(Math::degreesToRadians(numValue.asDouble())) * 1e10) / 1e10);
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_cos) {
    Value &numValue = thread->stack.back();
    numValue = Value(std::round(std::cos(Math::degreesToRadians(numValue.asDouble())) * 1e10) / 1e10);
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_tan) {
    Value &numValue = thread->stack.back();
    double value = numValue.asDouble();
    double modAngle = std::fmod(value, 360.0);
    if (modAngle < -180.0) modAngle += 360.0;
    if (modAngle > 180.0) modAngle -= 360.0;
    if (modAngle == 90.0 || modAngle == -270.0) numValue = Value(std::numeric_limits<double>::infinity());
    else if (modAngle == -90.0 || modAngle == 270.0) numValue = Value(-std::numeric_limits<double>::infinity());
    else numValue = Value(std::round(std::tan(Math::degreesToRadians(value)) * 1e10) / 1e10);
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_asin) {
    Value &numValue = thread->stack.back();
    numValue = Value(Math::radiansToDegrees(asin(numValue.asDouble())));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_acos) {
    Value &numValue = thread->stack.back();
    numValue = Value(Math::radiansToDegrees(acos(numValue.asDouble())));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_atan) {
    Value &numValue = thread->stack.back();
    numValue = Value(Math::radiansToDegrees(atan(numValue.asDouble())));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_ln) {
    Value &numValue = thread->stack.back();
    numValue = Value(log(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_log) {
    Value &numValue = thread->stack.back();
    numValue = Value(log10(numValue.asDouble())); // or should we use log(x)/log(10)?
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_epow) {
    Value &numValue = thread->stack.back();
    numValue = Value(exp(numValue.asDouble()));
    return BlockResult::CONTINUE;
}
DEFINE_EXECUTION(operator_mathop_10pow) {
    Value &numValue = thread->stack.back();
    numValue = Value(pow(10, numValue.asDouble()));
    return BlockResult::CONTINUE;
}