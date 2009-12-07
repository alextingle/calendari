#include "queue.h"

#include "err.h"

#include <cstdarg>
#include <cstdio>

namespace calendari {


std::string
Queue::quote(const std::string& s) const
{
  std::string result = "";
  std::string::size_type cur = 0;
  while(true)
  {
    std::string::size_type pos = s.find_first_of("'",cur);
    if(pos==s.npos)
    {
      result += s.substr(cur, pos);
      break;
    }
    result += s.substr(cur, pos-cur) + "''";
    cur = pos+1;
  }
  return result;
}


void
Queue::push(const std::string& sql)
{
  changes.push_back(sql);
  printf("%s\n",sql.c_str());
}


void
Queue::pushf(const char* format, ...)
{
  va_list args;
  va_start(args,format);
  char buf[256];
  int ret = vsnprintf(buf,sizeof(buf),format,args);
  if(ret>=sizeof(buf))
      util::error(1,0,"SQL too large for buffer."); //??
  push(buf);
  va_end(args);
}


void flush(Db& db)
{
}


} // end namespace calendari
