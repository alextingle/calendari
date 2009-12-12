#include "queue.h"

#include "db.h"
#include "err.h"

#include <cstdarg>
#include <cstdio>
#include <gtk/gtk.h>

namespace calendari {


bool
Queue::idle(void*)
{
  static Queue& q( Queue::inst() );
  if(!q.empty())
      q.flush();
  return false;
}


std::string
Queue::quote(const std::string& s)
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
Queue::set_db(Db* db)
{
  _db = db;
}


void
Queue::push(const std::string& sql)
{
  // Hook-in idle processing.
  (void)g_idle_add((GSourceFunc)idle,(gpointer)this);

  _changes.push_back(sql);
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
      CALI_ERRO(1,0,"SQL too large for buffer."); //??
  push(buf);
  va_end(args);
}


void
Queue::flush(void)
{
  if(!_changes.empty())
  {
    _db->exec("begin");
    while(!_changes.empty())
    {
      _db->exec( _changes.front().c_str() );
      _changes.pop_front();
    }
    _db->exec("commit");
  }
}


} // end namespace calendari
