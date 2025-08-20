// code mostly taken from devkitpro's time example, with edits needed for Scratch.
#include <cstddef>
#include <ctime>
#include <time.hpp>

/* const std::string months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

const std::string weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

const u16 daysAtStartOfMonthLUT[12] =
    {
        0 % 7,   // january    31
        31 % 7,  // february   28+1(leap year)
        59 % 7,  // march      31
        90 % 7,  // april      30
        120 % 7, // may        31
        151 % 7, // june       30
        181 % 7, // july       31
        212 % 7, // august     31
        243 % 7, // september  30
        273 % 7, // october    31
        304 % 7, // november   30
        334 % 7  // december   31
};

static inline bool isLeapYear(int year) {
    return (year % 4) == 0 && !((year % 100) == 0 && (year % 400) != 0);
}

static inline int getDayOfWeek(int day, int month, int year) {
    // http://en.wikipedia.org/wiki/Calculating_the_day_of_the_week
    day += 2 * (3 - ((year / 100) % 4));
    year %= 100;
    day += year + (year / 4);
    day += daysAtStartOfMonthLUT[month] - (isLeapYear(year) && (month <= 1));
    return day % 7;
} */

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
