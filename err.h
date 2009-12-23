#ifndef CALENDARI__UTIL__ERROR_H
#define CALENDARI__UTIL__ERROR_H

#include <error.h>
#include <stdio.h>
#include <stdarg.h>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace calendari {
namespace util {


typedef std::pair<const char*,int> Here;
#define CALI_HERE (calendari::util::Here(__FILE__,__LINE__))

inline void error(const Here& here,int r,int e,const char* format,...)
{
  va_list va_args;
  va_start(va_args,format);
  vfprintf(stderr,format,va_args);
  va_end(va_args);
  if(e)
      fprintf(stderr,": %s",::strerror(e));
  fprintf(stderr," at %s:%d\n",here.first,here.second);
#ifndef NDEBUG
  abort();
#endif
  if(r)
      ::exit(r);
}
#define CALI_ERRO(...) \
 do{calendari::util::error( \
   calendari::util::Here(__FILE__,__LINE__),__VA_ARGS__); \
 }while(0)


inline void warning(const Here& here,int e,const char* format,...)
{
  va_list va_args;
  va_start(va_args,format);
  vfprintf(stderr,format,va_args);
  va_end(va_args);
  if(e)
      fprintf(stderr,": %s",::strerror(e));
  fprintf(stderr," at %s:%d\n",here.first,here.second);
}
#define CALI_WARN(...) \
 do{calendari::util::warning( \
   calendari::util::Here(__FILE__,__LINE__),__VA_ARGS__); \
 }while(0)


} } // end namespace calendari::util

#endif // CALENDARI__UTIL__ERROR_H
