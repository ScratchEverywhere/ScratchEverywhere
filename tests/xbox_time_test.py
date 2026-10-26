#!/usr/bin/env python3
"""Exercise the real Xbox date reporters with a controlled SDK clock."""
import datetime as dt
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
UTC = dt.timezone.utc
epoch = dt.datetime(2000, 1, 1, tzinfo=UTC)
file_epoch = dt.datetime(1601, 1, 1, tzinfo=UTC)
cases = [
    # UTC instants and console offsets; local and UTC dates deliberately differ.
    (dt.datetime(1999, 12, 31, 23, 59, 59, 999000, UTC), 0),
    (epoch, 0),
    (epoch, -300),
    (epoch + dt.timedelta(hours=12), 330),
    (dt.datetime(2000, 2, 29, 23, 59, 59, 123000, UTC), 120),
    (dt.datetime(2026, 10, 26, 0, 0, 0, 1000, UTC), -240),
    (dt.datetime(2026, 12, 31, 23, 45, 12, 750000, UTC), 840),
    (dt.datetime(2100, 3, 1, 1, 2, 3, 456000, UTC), -720),
]

checks = []
for instant, offset in cases:
    local = instant.astimezone(dt.timezone(dt.timedelta(minutes=offset)))
    delta = instant - file_epoch
    ticks = ((delta.days * 86400 + delta.seconds) * 1000000 + delta.microseconds) * 10
    expected = (instant - epoch).total_seconds() / 86400
    checks.append(f"""
    clockLocal = {{{local.year}, {local.month}, {(local.weekday() + 1) % 7},
                  {local.day}, {local.hour}, {local.minute}, {local.second},
                  {local.microsecond // 1000}}};
    clockUtc = {{{ticks & 0xffffffff}u, {ticks >> 32}u}};
    assert(TimeSE::getYear() == {local.year});
    assert(TimeSE::getMonth() == {local.month});
    assert(TimeSE::getDay() == {local.day});
    assert(TimeSE::getDayOfWeek() == {(local.weekday() + 1) % 7 + 1});
    assert(TimeSE::getHours() == {local.hour});
    assert(TimeSE::getMinutes() == {local.minute});
    assert(TimeSE::getSeconds() == {local.second});
    assert(std::abs(TimeSE::getDaysSince2000() - ({expected!r})) < 1e-10);
""")

with tempfile.TemporaryDirectory(prefix="xbox-time-") as directory:
    work = pathlib.Path(directory)
    (work / "windows.h").write_text("""#pragma once
#include <cstdint>
struct SYSTEMTIME {
    std::uint16_t wYear, wMonth, wDayOfWeek, wDay;
    std::uint16_t wHour, wMinute, wSecond, wMilliseconds;
};
struct FILETIME { std::uint32_t dwLowDateTime, dwHighDateTime; };
void GetLocalTime(SYSTEMTIME*);
void GetSystemTimeAsFileTime(FILETIME*);
""")
    (work / "clock.cpp").write_text("""#include "time.hpp"
#include <windows.h>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <ctime>
static SYSTEMTIME clockLocal;
static FILETIME clockUtc;
static int localReads = 0, utcReads = 0;
void GetLocalTime(SYSTEMTIME* out) { *out = clockLocal; ++localReads; }
void GetSystemTimeAsFileTime(FILETIME* out) { *out = clockUtc; ++utcReads; }
// Any accidental return to the broken CRT path must fail this test.
extern "C" struct tm* localtime(const time_t*) { std::abort(); }
extern "C" time_t mktime(struct tm*) { std::abort(); }
int main() {
""" + "".join(checks) + f"""
    assert(localReads == {len(cases) * 7});
    assert(utcReads == {len(cases)});
}}
""")
    exe = work / "clock"
    subprocess.run([
        "clang++", "-std=c++17", "-D__XBOX__", "-fsanitize=address,undefined",
        "-I", str(work), "-I", str(ROOT / "include/runtime"),
        "-I", str(ROOT / "include"), str(ROOT / "source/runtime/time.cpp"),
        str(work / "clock.cpp"), "-o", str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True)

print(f"Xbox date reporters passed {len(cases)} clock cases (ASan/UBSan).")
