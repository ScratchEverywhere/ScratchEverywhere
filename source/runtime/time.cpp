#include "time.hpp"
#include <chrono>
#include <cstddef>
#include <ctime>

#if defined(__XBOX__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
std::tm localCalendar() {
#if defined(__XBOX__)
    // NXDK's localtime() is a stub returning nullptr. Use its implemented
    // calendar API, which also applies the console's timezone settings.
    SYSTEMTIME now;
    GetLocalTime(&now);
    std::tm calendar{};
    calendar.tm_hour = now.wHour;
    calendar.tm_min = now.wMinute;
    calendar.tm_sec = now.wSecond;
    calendar.tm_mday = now.wDay;
    calendar.tm_wday = now.wDayOfWeek;
    calendar.tm_mon = now.wMonth - 1;
    calendar.tm_year = now.wYear - 1900;
    return calendar;
#else
    const time_t unixTime = time(nullptr);
    return *localtime(&unixTime);
#endif
}
} // namespace

int TimeSE::getHours() {
    return localCalendar().tm_hour;
}

int TimeSE::getMinutes() {
    return localCalendar().tm_min;
}

int TimeSE::getSeconds() {
    return localCalendar().tm_sec;
}

int TimeSE::getDay() {
    return localCalendar().tm_mday;
}

int TimeSE::getDayOfWeek() {
    return localCalendar().tm_wday + 1;
}

int TimeSE::getMonth() {
    return localCalendar().tm_mon + 1;
}

int TimeSE::getYear() {
    return localCalendar().tm_year + 1900;
}

double TimeSE::getDaysSince2000() {
#if defined(__XBOX__)
    // Scratch VM's timezone adjustment makes this elapsed UTC time since
    // 2000-01-01T00:00:00Z. NXDK's mktime() is also an unimplemented stub.
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const int64_t ticks = (static_cast<int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime;
    constexpr int64_t epoch2000 = 125911584000000000LL; // 100 ns since 1601-01-01.
    return (ticks - epoch2000) / 864000000000.0;
#else
    const auto now = std::chrono::system_clock::now();

    struct tm start_tm = {.tm_sec = 0, .tm_min = 0, .tm_hour = 0, .tm_mday = 1, .tm_mon = 0, .tm_year = 2000 - 1900};

    const time_t start_time_t = mktime(&start_tm);
    const auto start = std::chrono::system_clock::from_time_t(start_time_t);

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    return millis / 86400000.0;
#endif
}
