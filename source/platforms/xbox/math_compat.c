#if defined(__XBOX__)
#include <math.h>

long int lround(double x)
{
    return (long int)round(x);
}

long int lroundf(float x)
{
    return (long int)roundf(x);
}

long int lroundl(long double x)
{
    return (long int)roundl(x);
}

long long int llround(double x)
{
    return (long long int)round(x);
}

long long int llroundf(float x)
{
    return (long long int)roundf(x);
}

long long int llroundl(long double x)
{
    return (long long int)roundl(x);
}

long int lrint(double x)
{
    return (long int)rint(x);
}

long int lrintf(float x)
{
    return (long int)rintf(x);
}

long int lrintl(long double x)
{
    return (long int)rintl(x);
}

long long int llrint(double x)
{
    return (long long int)rint(x);
}

long long int llrintf(float x)
{
    return (long long int)rintf(x);
}

long long int llrintl(long double x)
{
    return (long long int)rintl(x);
}
#endif
