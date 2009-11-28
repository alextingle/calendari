#ifndef CALENDARI__UTIL__SQL_H
#define CALENDARI__UTIL__SQL_H

#include "err.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdarg.h>

namespace calendari {
namespace sql {


typedef void(*MemoryStatus)(void*);


inline void
error(sqlite3* db, const char* filename, int line)
{
  int         errcode = ::sqlite3_errcode(db);
  const char* errmsg  = ::sqlite3_errmsg(db);
  util::error(0,0,"sqlite error %i: %s at %s:%i",errcode,errmsg,filename,line);
}


inline void
check_error(sqlite3* db, int return_code, const char* filename, int line)
{
  if( SQLITE_OK != return_code )
      sql::error(db,__FILE__,__LINE__);
}


inline void
bind_int(sqlite3* db, sqlite3_stmt* stmt, int idx, int val)
{
  int ret;
  ret= ::sqlite3_bind_int(stmt,idx,val);
  sql::check_error(db,ret,__FILE__,__LINE__);
}


inline void
bind_text(
    sqlite3*       db,
    sqlite3_stmt*  stmt,
    int            idx,
    const char*    s,
    int            n = -1,
    MemoryStatus   mem = SQLITE_TRANSIENT
  )
{
  int ret;
  ret= ::sqlite3_bind_text(stmt,idx,s,n,mem);
  sql::check_error(db,ret,__FILE__,__LINE__);
}


inline void
step(sqlite3* db, sqlite3_stmt* stmt)
{
  int return_code = ::sqlite3_step(stmt);
  switch(return_code)
  {
    case SQLITE_ROW:
      util::error(0,0,"Executed INSERT, but got rows!");
      break;
    case SQLITE_DONE: // OK
    case SQLITE_CONSTRAINT: // Probably primary key violation.
      break;
    default:
      sql::error(db,__FILE__,__LINE__);
  } // end switch
  ::sqlite3_reset(stmt);
}


} } // end namespace calendari::sql

#endif // CALENDARI__UTIL__SQL_H
