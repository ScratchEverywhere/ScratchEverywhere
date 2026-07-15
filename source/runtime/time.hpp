#include <stdint.h>
typedef uint16_t u16;

// 'Time' is a typedef in <X11/Xlib.h>: use 'TimeSE' as class name to avoid conflicts!

class TimeSE {
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
