#include "queue.h"

#include "db.h"
#include "err.h"
#include "sql.h"

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
  char buf[0x1000];
  int ret = vsnprintf(buf,sizeof(buf),format,args);
  if(ret >= static_cast<int>(sizeof(buf)))
      CALI_ERRO(1,0,"SQL too large for buffer."); //??
  push(buf);
  va_end(args);
}


void
Queue::flush(void)
{
  if(!_changes.empty())
  {
    sql::exec(CALI_HERE,*_db,"begin");
    while(!_changes.empty())
    {
      sql::exec(CALI_HERE,*_db, _changes.front().c_str() );
      _changes.pop_front();
    }
    sql::exec(CALI_HERE,*_db,"commit");
  }
}


} // end namespace calendari
