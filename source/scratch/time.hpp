#include <stdint.h>
typedef uint16_t u16;

class Time {
  public:
    static int getHours();

    static int getMinutes();

    static int getSeconds();

    static int getDay();

    static int getDayOfWeek();

    static int getMonth();

    static int getYear();

    static double getDaysSince2000();
};
