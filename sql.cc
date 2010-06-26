#include "sql.h"

#include <cassert>

namespace calendari {
namespace sql {


Statement::Statement(
    const util::Here&  here,
    sqlite3*           db,
    const char*        zSql,
    int                nByte,
    const char**       pzTail
  )
  : _db(db),
    _stmt(NULL)
{
  assert(db);
  check_error(here,db, ::sqlite3_prepare_v2(db,zSql,nByte,&_stmt,pzTail) );
}


Statement::~Statement(void)
{
  CALI_SQLCHK(_db, ::sqlite3_finalize(_stmt) );
}


Statement::operator sqlite3_stmt* (void) const
{
  return _stmt;
}


sqlite3*
Statement::db(void) const
{
  return _db;
}


} } // end namespace calendari::sql
