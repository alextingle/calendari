#ifndef CALENDARI__UTIL__SQL_H
#define CALENDARI__UTIL__SQL_H

#include "err.h"

#include <cstdio>
#include <cstdarg>
#include <sqlite3.h>
#include <string>

namespace calendari {
namespace sql {


typedef void(*MemoryStatus)(void*);


inline void
error(const util::Here& here, sqlite3* sdb)
{
  int         errcode = ::sqlite3_errcode(sdb);
  const char* errmsg  = ::sqlite3_errmsg(sdb);
  util::error(here,0,0,"sqlite error %i: %s",errcode,errmsg);
}


inline void
check_error(const util::Here& here, sqlite3* sdb, int return_code)
{
  if( SQLITE_OK != return_code )
      sql::error(here,sdb);
}
#define CALI_SQLCHK(DB,RET) \
  do{calendari::sql::check_error( \
    calendari::util::Here(__FILE__,__LINE__),DB,RET); \
  }while(0)


/** Escape single-quote characters for Sqlite strings. */
inline std::string
quote(const std::string& s)
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


/** Scoped class that wraps sqlite3_prepare_v2()..sqlite3_finalize().
 *  The constructor takes the same parameters as sqlite3_prepare_v2().
 *  The object behaves as a sqlite3_stmt*. sqlite3_finalize() is called
 *  upon destruction. */
class Statement
{
public:
  Statement(const util::Here& here,
      sqlite3* db, const char* zSql, int nByte=-1, const char** pzTail=NULL);
  ~Statement(void);
  operator sqlite3_stmt* (void) const;
  sqlite3* db(void) const;

private:
  sqlite3*      _db;
  sqlite3_stmt* _stmt;
};


inline void
bind_int(
    const util::Here&  here,
    sqlite3*           sdb,
    sqlite3_stmt*      stmt,
    int                idx,
    int                val)
{
  int ret;
  ret= ::sqlite3_bind_int(stmt,idx,val);
  sql::check_error(here,sdb,ret);
}


inline void
bind_text(
    const util::Here&  here,
    sqlite3*           sdb,
    sqlite3_stmt*      stmt,
    int                idx,
    const char*        s,
    int                n = -1,
    MemoryStatus       mem = SQLITE_TRANSIENT
  )
{
  int ret;
  ret= ::sqlite3_bind_text(stmt,idx,s,n,mem);
  sql::check_error(here,sdb,ret);
}


inline void
step_reset(const util::Here& here, sqlite3* sdb, sqlite3_stmt* stmt)
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
      sql::error(here,sdb);
  } // end switch
  ::sqlite3_reset(stmt);
}


/** Execute arbitrary SQL. Discards any resulting columns. */
inline void
exec(const util::Here& here, sqlite3* sdb, const char* sql)
{
  char* errmsg;
  int ret = ::sqlite3_exec(sdb,sql,NULL,NULL,&errmsg);
  if(SQLITE_OK != ret)
  {
    util::error(here,1,0,"sqlite error %i: %s in SQL \"%s\"",ret,errmsg,sql);
    ::sqlite3_free(errmsg);
  }
}


/** Execute arbitrary SQL. Discards any resulting columns. */
inline void
execf(const util::Here& here, sqlite3* sdb, const char* format, ...)
{
  va_list args;
  va_start(args,format);
  char buf[256];
  int ret = vsnprintf(buf,sizeof(buf),format,args);
  if(ret >= static_cast<int>(sizeof(buf)))
      CALI_ERRO(1,0,"SQL too large for buffer."); //??
  sql::exec(here,sdb,buf);
  va_end(args);
}


/** Query a single value, returns TRUE if the value was found. */
inline bool
query_val(
    const util::Here&  here,
    sqlite3*           sdb,
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
  if(ret >= static_cast<int>(sizeof(sql)))
      util::error(here,1,0,"SQL too large for buffer."); //??
  va_end(args);
  // Query
  Statement select_stmt(here,sdb,sql);
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
        sql::error(here,sdb);
  }
  return result;
}


} } // end namespace calendari::sql

#endif // CALENDARI__UTIL__SQL_H
