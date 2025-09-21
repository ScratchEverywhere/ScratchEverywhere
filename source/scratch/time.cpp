#include <cstddef>
#include <ctime>
#include <time.hpp>


int Time::getHours() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    return timeStruct->tm_hour;
}

int Time::getMinutes() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    return timeStruct->tm_min;
}

int Time::getSeconds() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    return timeStruct->tm_sec;
}

int Time::getDay() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    return timeStruct->tm_mday;
}

int Time::getDayOfWeek() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    // tm_wday: days since Sunday [0,6], so add 1 to make Sunday=1, Monday=2, etc.
    return timeStruct->tm_wday + 1;
}

int Time::getMonth() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    return timeStruct->tm_mon;
}

int Time::getYear() {
    time_t unixTime = time(NULL);
    struct tm *timeStruct = gmtime((const time_t *)&unixTime);
    return timeStruct->tm_year + 1900;
}

// Returns days (including fractional) since Jan 1, 2000 UTC
double Time::getDaysSince2000() {
    time_t now = time(NULL);

    // Set up struct tm for Jan 1, 2000, 00:00:00 UTC
    struct tm start_tm = {0};
    start_tm.tm_year = 2000 - 1900;
    start_tm.tm_mon = 0;
    start_tm.tm_mday = 1;
    start_tm.tm_hour = 0;
    start_tm.tm_min = 0;
    start_tm.tm_sec = 0;

    time_t start = mktime(&start_tm);

    double seconds = difftime(now, start);
    return seconds / 86400.0;
}
