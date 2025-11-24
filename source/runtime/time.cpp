#include "time.hpp"
#include <chrono>
#include <cstddef>
#include <ctime>

int Time::getHours() {
    const time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_hour;
}

int Time::getMinutes() {
    const time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_min;
}

int Time::getSeconds() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_sec;
}

int Time::getDay() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mday;
}

int Time::getDayOfWeek() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_wday + 1;
}

int Time::getMonth() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mon + 1;
}

int Time::getYear() {
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_year + 1900;
}

double Time::getDaysSince2000() {
    const auto now = std::chrono::system_clock::now();

    struct tm start_tm = {.tm_sec = 0, .tm_min = 0, .tm_hour = 0, .tm_mday = 1, .tm_mon = 0, .tm_year = 2000 - 1900};

    const time_t start_time_t = mktime(&start_tm);
    const auto start = std::chrono::system_clock::from_time_t(start_time_t);

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    return millis / 86400000.0;
}
