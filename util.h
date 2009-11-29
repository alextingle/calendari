#ifndef CALENDARI__UTIL_H
#define CALENDARI__UTIL_H 1

#include <time.h>

namespace calendari {


inline time_t normalise_local_tm(struct tm& v)
{
    time_t result = mktime(&v);
    localtime_r(&result,&v);
    return result;
}


} // end namespace calendari

#endif
