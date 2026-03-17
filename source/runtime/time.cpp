#include "time.hpp"

#ifdef PLAYDATE
#include <pdcpp/pdnewlib.h>

extern PlaydateAPI *pd;

static PDDateTime getPDTime() {
    PDDateTime dt;
    uint32_t epoch = pd->system->getSecondsSinceEpoch(nullptr);
    pd->system->convertEpochToDateTime(epoch, &dt);
    return dt;
}
#else
#include <chrono>
#include <cstddef>
#include <ctime>
#endif

int Time::getHours() {
#ifdef PLAYDATE
    return getPDTime().hour;
#else
    const time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_hour;
#endif
}

int Time::getMinutes() {
#ifdef PLAYDATE
    return getPDTime().minute;
#else
    const time_t unixTime = time(NULL);
    struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_min;
#endif
}

int Time::getSeconds() {
#ifdef PLAYDATE
    return getPDTime().second;
#else
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_sec;
#endif
}

int Time::getDay() {
#ifdef PLAYDATE
    return getPDTime().day;
#else
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mday;
#endif
}

int Time::getDayOfWeek() {
#ifdef PLAYDATE
    return getPDTime().weekday;
#else
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_wday + 1;
#endif
}

int Time::getMonth() {
#ifdef PLAYDATE
    return getPDTime().month;
#else
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_mon + 1;
#endif
}

int Time::getYear() {
#ifdef PLAYDATE
    return getPDTime().year;
#else
    const time_t unixTime = time(NULL);
    const struct tm *timeStruct = localtime((const time_t *)&unixTime);
    return timeStruct->tm_year + 1900;
#endif
}

double Time::getDaysSince2000() {
#ifdef PLAYDATE
    unsigned int ms;
    unsigned int seconds = pd->system->getSecondsSinceEpoch(&ms);

    double totalSeconds = (double)seconds + (ms / 1000.0);
    return totalSeconds / 86400.0;
#else
    const auto now = std::chrono::system_clock::now();

    struct tm start_tm = {.tm_sec = 0, .tm_min = 0, .tm_hour = 0, .tm_mday = 1, .tm_mon = 0, .tm_year = 2000 - 1900};

    const time_t start_time_t = mktime(&start_tm);
    const auto start = std::chrono::system_clock::from_time_t(start_time_t);

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();

    return millis / 86400000.0;
#endif
}
