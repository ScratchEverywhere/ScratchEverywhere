#include <chrono>
#include <cstddef>
#include <ctime>
#include <time.hpp>

static inline std::tm getLocalTime() {
    time_t unixTime = time(NULL);
    return *localtime(&unixTime);
}

int Time::getHours() {
    return getLocalTime().tm_hour;
}

int Time::getMinutes() {
    return getLocalTime().tm_min;
}

int Time::getSeconds() {
    return getLocalTime().tm_sec;
}

int Time::getDay() {
    return getLocalTime().tm_mday;
}

int Time::getDayOfWeek() {
    return getLocalTime().tm_wday + 1; // +1 to match Scratch (1-7)
}

int Time::getMonth() {
    return getLocalTime().tm_mon + 1; // +1 to match Scratch (1-12)
}

int Time::getYear() {
    return getLocalTime().tm_year + 1900;
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
