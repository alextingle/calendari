#include "db.h"

#include "err.h"
#include "queue.h"
#include "sql.h"
#include "util.h"

#include <cassert>
#include <cstdarg>
#include <set>

namespace calendari {


Db::Db(const char* dbname)
{
  if( SQLITE_OK != ::sqlite3_open(dbname,&_sdb) )
      CALI_ERRO(1,0,"Failed to open database %s",dbname);
  Queue::inst().set_db( this );
  create_db(); // ?? Wasteful to do this if not needed?
}


Db::~Db(void)
{
  if(_sdb)
    ::sqlite3_close(_sdb);
  typedef std::map<std::string,Calendar*>::iterator CIt;
  for(CIt c =_calendar.begin(); c!=_calendar.end(); ++c)
      delete c->second;
  typedef std::map<std::string,Event*>::iterator EIt;
  for(EIt e =_event.begin(); e!=_event.end(); ++e)
      delete e->second;
  typedef std::map<std::pair<time_t,std::string>,Occurrence*>::iterator OIt;
  for(OIt o =_occurrence.begin(); o!=_occurrence.end(); ++o)
      delete o->second;
}


void
Db::create_db(void)
{
  // VERSION slices the database between the current (VERSION=1) contents and
  // content that is being generated (VERSION>1)
  sql::exec(CALI_HERE,_sdb,
      "create table if not exists CALENDAR ("
      "  VERSION  integer,"
      "  CALNUM   integer,"
      "  CALID    string,"
      "  CALNAME  string,"
      "  PATH     string,"
      "  READONLY boolean,"
      "  POSITION integer," // ordering within the UI's calendar list.
      "  COLOUR   string,"
      "  SHOW     boolean,"
      "  primary key(VERSION,CALID)"
      ")"
    );
  sql::exec(CALI_HERE,_sdb,
      "create table if not exists EVENT ("
      "  VERSION  integer,"
      "  CALNUM   integer,"
      "  UID      string,"
      "  SUMMARY  string,"
      "  CALID    string,"
      "  SEQUENCE integer,"
      "  ALLDAY   boolean,"
      "  VEVENT   blob,"
      "  primary key(VERSION,UID)"
      ")"
    );
  sql::exec(CALI_HERE,_sdb,
      "create table if not exists OCCURRENCE ("
      "  VERSION  integer,"
      "  CALNUM   integer,"
      "  UID      string,"
      "  DTSTART  integer," // time_t (need to allow for 'all-day')
      "  DTEND    integer," // time_t
      "  primary key(VERSION,UID,DTSTART)"
      ")"
    );
  sql::exec(CALI_HERE,_sdb,
      "create index if not exists OCC_START_INDEX on OCCURRENCE(DTSTART)");
  sql::exec(CALI_HERE,_sdb,
      "create index if not exists OCC_END_INDEX on OCCURRENCE(DTEND)");
  /*
  -- Find all occurances between two times.
  select O.UID,DTSTART,DTEND,SUMMARY,COLOUR
    from OCCURANCE O
      left join EVENT E on O.UID=E.UID and E.VERSION=0
      left join CALENDAR C on E.CALID=C.CALID and C.VERSION=0
    where
      O.VERSION=0 and
      DTEND > ?period_start and
      DTSTART < ?period_end and
      E.CALID in (?active_calendars);

  -- Update a version.
  begin;
  -- List latest version of all events.
  select UID, max(SEQUENCE), VERSION
    from EVENT
    order by desc SEQUENCE
    group by UID;
  */
}


void
Db::refresh_cal(int calnum, int version)
{
  sql::exec(CALI_HERE,_sdb,"begin");
  try
  {
    sql::execf(CALI_HERE,_sdb,
        "delete from OCCURRENCE where VERSION=1 and CALNUM=%d",calnum);
    sql::execf(CALI_HERE,_sdb,
        "delete from EVENT where VERSION=1 and CALNUM=%d",calnum);
    // Ensure that UID is unique
    // ?? should look at SEQUENCE to decide which event to keep.
    sql::execf(CALI_HERE,_sdb,
        "delete from EVENT where VERSION=1 and "
          "UID in (select UID from EVENT where VERSION=%d)",version);
    sql::execf(CALI_HERE,_sdb,
        "update OCCURRENCE set VERSION=1 where VERSION=%d",version);
    sql::execf(CALI_HERE,_sdb,
        "update EVENT set VERSION=1 where VERSION=%d",version);
    sql::execf(CALI_HERE,_sdb,
        "delete from CALENDAR where VERSION=%d",version);
    sql::exec(CALI_HERE,_sdb,"commit");
  }
  catch(...)
  {
    try{ sql::exec(CALI_HERE,_sdb,"rollback"); } catch(...) {}
    throw;
  }
}


void
Db::load_calendars(void)
{
  sqlite3_stmt* select_stmt;
  const char* sql =
      "select CALID,CALNUM,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW "
      "from CALENDAR "
      "where VERSION=1 "
      "order by POSITION";
  CALI_SQLCHK(_sdb, ::sqlite3_prepare_v2(_sdb,sql,-1,&select_stmt,NULL) );

  int position = 0;
  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Calendar* cal = new Calendar(
          safestr(::sqlite3_column_text(select_stmt,0)), // calid
                  ::sqlite3_column_int( select_stmt,1),  // calnum
          safestr(::sqlite3_column_text(select_stmt,2)), // name
          safestr(::sqlite3_column_text(select_stmt,3)), // path
                  ::sqlite3_column_int( select_stmt,4),  // readonly
          position++,
//??              ::sqlite3_column_int( select_stmt,5),  // position
          safestr(::sqlite3_column_text(select_stmt,6)), // colour
                  ::sqlite3_column_int( select_stmt,7)   // show
        );
      _calendar.insert(std::make_pair(cal->calid,cal));
    }
    else if(return_code==SQLITE_DONE)
    {
      break;
    }
    else
    {
      calendari::sql::error(CALI_HERE,_sdb);
      break;
    }
  }
  CALI_SQLCHK(_sdb, ::sqlite3_finalize(select_stmt) );
}

  
std::multimap<time_t,Occurrence*>
Db::find(time_t begin, time_t end)
{
  std::multimap<time_t,Occurrence*> result;

  sqlite3_stmt* select_stmt;
  const char* sql =
      "select O.UID,DTSTART,DTEND,SUMMARY,ALLDAY,CALID "
      "from OCCURRENCE O "
      "left join EVENT E on E.UID=O.UID and E.VERSION=1 "
      "where DTEND>=? and DTSTART<? and O.VERSION=1 "
      "order by DTSTART";
  CALI_SQLCHK(_sdb, ::sqlite3_prepare_v2(_sdb,sql,-1,&select_stmt,NULL) );
  sql::bind_int(CALI_HERE,_sdb,select_stmt,1,begin);
  sql::bind_int(CALI_HERE,_sdb,select_stmt,2,end);

  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Occurrence* occ = make_occurrence(
          safestr(::sqlite3_column_text(select_stmt,0)), // uid
                  ::sqlite3_column_int( select_stmt,1),  // dtstart
                  ::sqlite3_column_int( select_stmt,2),  // dtend
          safestr(::sqlite3_column_text(select_stmt,3)), // summary
                  ::sqlite3_column_int( select_stmt,4),  // all_day
          safestr(::sqlite3_column_text(select_stmt,5))  // calid
        );
      result.insert(std::make_pair(occ->dtstart(),occ));
    }
    else if(return_code==SQLITE_DONE)
    {
      break;
    }
    else
    {
      calendari::sql::error(CALI_HERE,_sdb);
      break;
    }
  }
  CALI_SQLCHK(_sdb, ::sqlite3_finalize(select_stmt) );
  return result;
}


int
Db::calnum(const char* calid)
{
  assert(calid);
  // Look it up in our _calendars.
  std::map<std::string,Calendar*>::const_iterator pos = _calendar.find(calid);
  if(pos != _calendar.end())
      return pos->second->calnum;

  // Look it up in the database, then.
  int calnum =1;
  std::string sql_calid = sql::quote(calid);
  bool ok =
    sql::query_val(
        CALI_HERE,_sdb,
        calnum,                                  // <== output
        "select CALNUM from CALENDAR where CALID='%s' order by VERSION",
        sql_calid.c_str()
      );
  if(ok)
      return calnum;

  // OK, well find the next free number, then.  
  sql::query_val(
      CALI_HERE,_sdb,
      calnum,                                  // <== output
      "select 1 + coalesce(max(CALNUM),0) from CALENDAR",
      sql_calid.c_str()
    );
  return calnum;
}


Occurrence* Db::make_occurrence(
    const char*  uid,
    time_t       dtstart,
    time_t       dtend,
    const char*  summary,
    bool         all_day,
    const char*  calid
  )
{
  Event* event;
  std::map<std::string,Event*>::iterator e = _event.find(uid);
  if(e==_event.end())
  {
    event = _event[uid] =
      new Event(
          *_calendar[calid],
          uid,
          summary,
          0, // ??? sequence
          all_day
        );
  }
  else
  {
    event = e->second;
  }

  Occurrence::key_type key(dtstart,uid);
  std::map<Occurrence::key_type,Occurrence*>::iterator o =_occurrence.find(key);
  if(o!=_occurrence.end())
      return o->second;
  else
      return _occurrence[key] = new Occurrence(*event,dtstart,dtend);
}


void
Db::moved(Occurrence* occ)
{
  _occurrence.erase( occ->key() );
  _occurrence[ occ->rekey() ] = occ;
}


void
Db::erase(Occurrence* occ)
{
  occ->destroy();
  _occurrence.erase( occ->key() );
  delete occ;
}


} // end namespace calendari
