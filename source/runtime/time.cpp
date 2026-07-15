#include "time.hpp"
#include <chrono>
#include <cstddef>
#include <ctime>

// 'Time' is a typedef in <X11/Xlib.h>: use 'TimeSE' as class name to avoid conflicts!

int TimeSE::getHours() {
    const time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_hour;
}

int TimeSE::getMinutes() {
    const time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_min;
}

int TimeSE::getSeconds() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_sec;
}

int TimeSE::getDay() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mday;
}

int TimeSE::getDayOfWeek() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_wday + 1;
}

int TimeSE::getMonth() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mon + 1;
}

int TimeSE::getYear() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_year + 1900;
}

double TimeSE::getDaysSince2000() {
    const auto now = std::chrono::system_clock::now();

    struct tm start_tm = {.tm_sec = 0, .tm_min = 0, .tm_hour = 0, .tm_mday = 1, .tm_mon = 0, .tm_year = 2000 - 1900};

    const time_t start_time_t = mktime(&start_tm);
    const auto start = std::chrono::system_clock::from_time_t(start_time_t);

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    return millis / 86400000.0;
}
