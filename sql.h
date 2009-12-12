#ifndef CALENDARI__UTIL__SQL_H
#define CALENDARI__UTIL__SQL_H

#include "err.h"

#include <cstdio>
#include <cstdarg>
#include <sqlite3.h>

namespace calendari {
namespace sql {


typedef void(*MemoryStatus)(void*);


inline void
error(const util::Here& here, sqlite3* db)
{
  int         errcode = ::sqlite3_errcode(db);
  const char* errmsg  = ::sqlite3_errmsg(db);
  util::error(here,0,0,"sqlite error %i: %s",errcode,errmsg);
}


inline void
check_error(const util::Here& here, sqlite3* db, int return_code)
{
  if( SQLITE_OK != return_code )
      sql::error(here,db);
}
#define CALI_SQLCHK(DB,RET) \
  do{calendari::sql::check_error( \
    calendari::util::Here(__FILE__,__LINE__),DB,RET); \
  }while(0)


inline void
bind_int(
    const util::Here&  here,
    sqlite3*           db,
    sqlite3_stmt*      stmt,
    int                idx,
    int                val)
{
  int ret;
  ret= ::sqlite3_bind_int(stmt,idx,val);
  sql::check_error(here,db,ret);
}


inline void
bind_text(
    const util::Here&  here,
    sqlite3*           db,
    sqlite3_stmt*      stmt,
    int                idx,
    const char*        s,
    int                n = -1,
    MemoryStatus       mem = SQLITE_TRANSIENT
  )
{
  int ret;
  ret= ::sqlite3_bind_text(stmt,idx,s,n,mem);
  sql::check_error(here,db,ret);
}


inline void
step_reset(const util::Here& here, sqlite3* db, sqlite3_stmt* stmt)
{
  int return_code = ::sqlite3_step(stmt);
  switch(return_code)
  {
    case SQLITE_ROW:
      util::error(here,0,0,"Executed INSERT, but got rows!");
      break;
    case SQLITE_DONE: // OK
    case SQLITE_CONSTRAINT: // Probably primary key violation. ??
      break;
    default:
      sql::error(here,db);
  } // end switch
  ::sqlite3_reset(stmt);
}


/** Query a single value, returns TRUE if the value was found. */
inline bool
query_val(
    const util::Here&  here,
    sqlite3*           db,
    int&               val, // output
    const char*        format, ...
  )
{
  bool result = false;
  // Format the SQL.
  va_list args;
  va_start(args,format);
  char sql[256];
  int ret = vsnprintf(sql,sizeof(sql),format,args);
  if(ret>=sizeof(sql))
      util::error(here,1,0,"SQL too large for buffer."); //??
  va_end(args);
  // Query
  sqlite3_stmt* select_stmt;
  check_error(here,db, ::sqlite3_prepare_v2(db,sql,-1,&select_stmt,NULL) );
  int return_code = ::sqlite3_step(select_stmt);
  switch(return_code)
  {
    case SQLITE_ROW:
        result = true;
        val = ::sqlite3_column_int(select_stmt,0);
        break;
    case SQLITE_DONE:
        // No value.
        break;
    default:
        // Error
        sql::error(here,db);
  }
  // Clean up.
  check_error(here,db, ::sqlite3_finalize(select_stmt) );
  return result;
}


} } // end namespace calendari::sql

#endif // CALENDARI__UTIL__SQL_H
