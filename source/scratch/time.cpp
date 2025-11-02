#include <chrono>
#include <cstddef>
#include <ctime>
#include <time.hpp>

int Time::getHours() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_hour;
}

int Time::getMinutes() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_min;
}

int Time::getSeconds() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_sec;
}

int Time::getDay() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mday;
}

int Time::getDayOfWeek() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_wday + 1;
}

int Time::getMonth() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mon + 1;
}

int Time::getYear() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_year + 1900;
}

double Time::getDaysSince2000() {
    using namespace std::chrono;

    const auto now = system_clock::now();

    struct tm start_tm = {0};
    start_tm.tm_year = 2000 - 1900;
    start_tm.tm_mon = 0;
    start_tm.tm_mday = 1;
    start_tm.tm_hour = 0;
    start_tm.tm_min = 0;
    start_tm.tm_sec = 0;

    const time_t start_time_t = mktime(&start_tm);
    const auto start = system_clock::from_time_t(start_time_t);

    const auto diff = now - start;
    const auto millis = duration_cast<milliseconds>(diff).count();

    return millis / 86400000.0;
}
