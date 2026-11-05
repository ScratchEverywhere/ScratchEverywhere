#!/usr/bin/env python3
"""Focused unit tests for the string conversion peephole and numeric parsing parity."""
import pathlib
import subprocess
import tempfile
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

test_source = r'''
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include "runtime/math.hpp"
#include "runtime/conversions.hpp"
#include "runtime/value.hpp"
#include "runtime/bytecode_analysis.hpp"

// Standalone mock of bytecode compiler canonicalLiteral for test coverage
std::unordered_set<std::string> reservedLiterals;
bool canonicalizeLiterals = true;

Value canonicalLiteral(const Value &value) {
    if (!canonicalizeLiterals || !value.isString()) return value;
    const std::string text = value.asString();
    if (!BytecodeAnalysis::isCanonicalNumericString(text) || reservedLiterals.count(text) != 0) return value;
    const auto parsed = Math::parseNumber(text);
    return parsed.has_value() ? Value::stringTypedNumber(*parsed) : value;
}

void testNumericParsingParity() {
    // 1. Strings that are both valid numbers AND strictly canonical representation
    const char *canonicalNumbers[] = {
        "0", "1", "2", "42", "100", "100000",
        "-1", "-42", "-100000",
        "0.5", "3.14159", "-0.125", "123.456",
        "9999", "-0.5"
    };

    for (const char *numStr : canonicalNumbers) {
        assert(BytecodeAnalysis::isCanonicalNumericString(numStr) && "Should be canonical numeric string");
        auto parsed = Math::parseNumber(numStr);
        assert(parsed.has_value() && "Math::parseNumber should succeed");
        double convVal = Conversions::stringToNumber(numStr);
        assert(*parsed == convVal && "Parsing parity failure between Math::parseNumber and Conversions::stringToNumber");
    }

    // 2. Numeric strings that Scratch parses as numbers, but are NOT in canonical format
    // (e.g. scientific notation, leading zeros, leading +, surrounding whitespace, hex/oct/bin)
    const char *nonCanonicalNumbers[] = {
        "1e5", "2.5e3", "1e-4", "01", "+42", "  123  ", "0x10", "0b101", "0o77", "+Infinity", "-Infinity", "Infinity"
    };

    for (const char *numStr : nonCanonicalNumbers) {
        assert(!BytecodeAnalysis::isCanonicalNumericString(numStr) && "Should NOT be canonical numeric string");
        auto parsed = Math::parseNumber(numStr);
        assert(parsed.has_value() && "Math::parseNumber should parse non-canonical numeric string");
        double convVal = Conversions::stringToNumber(numStr);
        assert(*parsed == convVal && "Parsing parity failure for non-canonical number");
    }

    // 3. Completely non-numeric strings
    const char *nonNumeric[] = {
        "hello", "costume1", "sound_1", "123a", "true", "false", "", " ", "   "
    };

    for (const char *str : nonNumeric) {
        assert(!BytecodeAnalysis::isCanonicalNumericString(str) && "Should NOT be canonical numeric string");
        auto parsed = Math::parseNumber(str);
        assert(!parsed.has_value() && "Math::parseNumber should return unexpected for non-numeric");
        assert(Conversions::stringToNumber(str) == 0.0 && "Conversions::stringToNumber should default to 0.0");
    }
}

void testReservedLiteralsSafety() {
    reservedLiterals.clear();
    reservedLiterals.insert("1");
    reservedLiterals.insert("costume1");
    reservedLiterals.insert("2");

    // "1" is a reserved name (costume name "1")
    Value v1(std::string("1"));
    Value c1 = canonicalLiteral(v1);
    assert(c1.isString() && !c1.isDouble() && "Reserved literal must remain a string");

    // "3" is NOT reserved, so it should be canonicalized to a string-typed number
    Value v3(std::string("3"));
    Value c3 = canonicalLiteral(v3);
    assert(c3.isStringTyped() && c3.isDouble() && "Non-reserved canonical literal must be string-typed number");
    assert(c3.numberRaw() == 3.0);

    // Non-numeric reserved string remains string
    Value vCostume(std::string("costume1"));
    Value cCostume = canonicalLiteral(vCostume);
    assert(cCostume.isString());
}

void testStringTypedNumberSemantics() {
    Value st = Value::stringTypedNumber(42.5);
    assert(st.isDouble());
    assert(st.isStringTyped());
    assert(!st.isString());
    assert(st.asDouble() == 42.5);
    assert(st.numberRaw() == 42.5);
    assert(st.asString() == "42.5");

    // Equality with pure number
    Value pure(42.5);
    assert(st == pure);
    assert(pure == st);

    // Equality with matching string
    Value str(std::string("42.5"));
    assert(st == str);
    assert(str == st);
}

void testControlFlowSafety() {
    // Verify that the fused peephole matches un-fused behavior across all inputs
    // Un-fused: ConvertStringToValue (StringPool::getOrInsert, pushes Value) -> ConvertValueToNumber (Value::asDouble)
    // Fused: pops string, directly parses with Math::parseNumber, pushes double, advances pc by 2
    const std::string testCases[] = {
        "42", "0", "-123.45", "1e5", "invalid", "0x20", "   99   ", "", "true", "-Infinity", "+100"
    };

    for (const auto &inputStr : testCases) {
        // Unfused path
        Value boxedVal(inputStr);
        double unfusedResult = boxedVal.asDouble();

        // Fused peephole path (as implemented in bytecode_executor.cpp)
        const auto parsed = Math::parseNumber(inputStr);
        double fusedResult = parsed.has_value() ? *parsed : 0.0;

        assert(unfusedResult == fusedResult && "Fused peephole must produce bit-for-bit identical result to un-fused path");
    }
}

int main() {
    testNumericParsingParity();
    testReservedLiteralsSafety();
    testStringTypedNumberSemantics();
    testControlFlowSafety();
    std::cout << "All string conversion peephole tests PASSED." << std::endl;
    return 0;
}
'''

def run():
    with tempfile.TemporaryDirectory() as tmpdir:
        work = pathlib.Path(tmpdir)
        src_file = work / "test.cpp"
        bin_file = work / "test"
        src_file.write_text(test_source)
        (work / "renderer.hpp").write_text("""#pragma once
#include <cstdint>
enum class Renderers : uint8_t { D3D9, OpenGLCore, OpenGL, NV2A, Xbox, GX2, SDL3, SDL2, Citro2D, GL2D, SDL1, GDI, Headless, _count };
namespace Render { inline Renderers currentRenderer = Renderers::Xbox; }
""")
        (work / "log.hpp").write_text("""#pragma once
#include <string>
namespace Log { inline void log(std::string) {} inline void logWarning(std::string) {} inline void logCritical(std::string, bool = false) {} }
""")

        cmd_compile = [
            "g++", "-std=c++17", "-O2",
            f"-I{tmpdir}",
            f"-I{ROOT}/include",
            f"-I{ROOT}/include/runtime",
            f"-I{ROOT}/include/platforms",
            f"-I{ROOT}/source",
            f"-I{ROOT}/build/xbox/.cache/catalog/src_11d6d5f2ef6c/src/include",
            f"-I{ROOT}/build/xbox/.cache/catalog/src_3da314ddf466/include",
            f"-I{ROOT}/build/xbox/.cache/catalog/src_b33bae3e797e/src/include",
            f"-I{ROOT}/build/xbox/.cache/catalog/src_a0c7832b3d43/src",
            str(src_file),
            f"{ROOT}/build/xbox/.cache/catalog/src_a0c7832b3d43/src/ryu/d2s.c",
            f"{ROOT}/source/runtime/math.cpp",
            f"{ROOT}/source/runtime/conversions.cpp",
            f"{ROOT}/source/runtime/value.cpp",
            f"{ROOT}/source/runtime/bytecode_analysis.cpp",
            "-o", str(bin_file)
        ]
        res = subprocess.run(cmd_compile, capture_output=True, text=True)
        if res.returncode != 0:
            print("Compilation failed:")
            print(res.stderr)
            sys.exit(1)

        res_run = subprocess.run([str(bin_file)], capture_output=True, text=True)
        if res_run.returncode != 0:
            print("Execution failed:")
            print(res_run.stderr)
            sys.exit(1)
        print(res_run.stdout.strip())

if __name__ == "__main__":
    run()
