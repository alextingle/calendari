#ifndef CALENDARI__UTIL__ERROR_H
#define CALENDARI__UTIL__ERROR_H

#include <error.h>
#include <stdio.h>
#include <stdarg.h>

namespace calendari {
namespace util {


inline void error(int,int,const char* format,...)
{
  va_list va_args;
  va_start(va_args,format);
  vfprintf(stderr,format,va_args);
  va_end(va_args);
  fprintf(stderr,"\n");
}


inline void warning(int,const char* format,...)
{
  va_list va_args;
  va_start(va_args,format);
  vfprintf(stderr,format,va_args);
  va_end(va_args);
  fprintf(stderr,"\n");
}


} } // end namespace calendari::util

#endif // CALENDARI__UTIL__ERROR_H
