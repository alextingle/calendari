#ifndef CALENDARI__UTIL__ERROR_H
#define CALENDARI__UTIL__ERROR_H

#include <stdarg.h>
#include <utility>

namespace calendari {
namespace util {


typedef std::pair<const char*,int> Here;
#define CALI_HERE (calendari::util::Here(__FILE__,__LINE__))

void error(const Here& here,int r,int e,const char* format,...);

#define CALI_ERRO(...) \
 do{calendari::util::error( \
   calendari::util::Here(__FILE__,__LINE__),__VA_ARGS__); \
 }while(0)


void warning(const Here& here,int e,const char* format,...);

#define CALI_WARN(...) \
 do{calendari::util::warning( \
   calendari::util::Here(__FILE__,__LINE__),__VA_ARGS__); \
 }while(0)


} } // end namespace calendari::util

#endif // CALENDARI__UTIL__ERROR_H
