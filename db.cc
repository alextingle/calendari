#include "db.h"

#include "err.h"
#include "queue.h"
#include "sql.h"
#include "util.h"

#include <cassert>
#include <cstdarg>
#include <set>

namespace calendari {


void
Version::purge(int calnum)
{
  typedef std::map<Occurrence::key_type,Occurrence*>::iterator OIt;
  for(OIt oi =_occurrence.begin(); oi!=_occurrence.end(); )
  {
    OIt o = oi++;
    if(calnum == o->second->event.calendar().calnum)
    {
      delete o->second;
      _occurrence.erase(o);
    }
  }
  typedef std::map<std::string,Event*>::iterator EIt;
  for(EIt ei =_event.begin(); ei!=_event.end(); )
  {
    EIt e = ei++;
    if(calnum == e->second->calendar().calnum)
    {
      delete e->second;
      _event.erase(e);
    }
  }
}


void
Version::destroy(void)
{
  typedef std::map<int,Calendar*>::iterator CIt;
  for(CIt c =_calendar.begin(); c!=_calendar.end(); ++c)
      delete c->second;
  typedef std::map<std::string,Event*>::iterator EIt;
  for(EIt e =_event.begin(); e!=_event.end(); ++e)
      delete e->second;
  typedef std::map<Occurrence::key_type,Occurrence*>::iterator OIt;
  for(OIt o =_occurrence.begin(); o!=_occurrence.end(); ++o)
      delete o->second;
  _calendar.clear();
  _event.clear();
  _occurrence.clear();
}


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
  for(std::map<int,Version>::iterator v=_ver.begin(); v!=_ver.end(); ++v)
      v->second.destroy();
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
      "  SEQUENCE integer,"
      "  ALLDAY   boolean,"
      "  RECURS   boolean,"
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
Db::refresh_cal(int calnum, int from_version, int to_version)
{
  sql::exec(CALI_HERE,_sdb,"begin");
  try
  {
    // Ensure that UID is unique
    // ?? should look at SEQUENCE to decide which event to keep.
    sql::execf(CALI_HERE,_sdb,
        "delete from OCCURRENCE where VERSION=%d and( CALNUM=%d or "
          "UID in (select UID from EVENT where VERSION=%d) )",
        to_version,calnum,from_version);
    sql::execf(CALI_HERE,_sdb,
        "delete from EVENT where VERSION=%d and( CALNUM=%d or "
          "UID in (select UID from EVENT where VERSION=%d) )",
        to_version,calnum,from_version);
    sql::execf(CALI_HERE,_sdb,
        "update OCCURRENCE set VERSION=%d where VERSION=%d",
        to_version,from_version);
    sql::execf(CALI_HERE,_sdb,
        "update EVENT set VERSION=%d where VERSION=%d",to_version,from_version);
    sql::execf(CALI_HERE,_sdb,
        "delete from CALENDAR where VERSION=%d",from_version);
    sql::exec(CALI_HERE,_sdb,"commit");
    _ver[to_version].purge(calnum);
  }
  catch(...)
  {
    try{ sql::exec(CALI_HERE,_sdb,"rollback"); } catch(...) {}
    throw;
  }
}


void
Db::load_calendars(int version)
{
  Version& ver = _ver[version];
  sqlite3_stmt* select_stmt;
  const char* sql =
      "select CALID,CALNUM,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW "
      "from CALENDAR "
      "where VERSION=? "
      "order by POSITION";
  CALI_SQLCHK(_sdb, ::sqlite3_prepare_v2(_sdb,sql,-1,&select_stmt,NULL) );
  sql::bind_int(CALI_HERE,_sdb,select_stmt,1,version);

  int position = 0;
  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Calendar* cal = new Calendar(version,
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
      ver._calendar.insert(std::make_pair(cal->calnum,cal));
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
Db::find(time_t begin, time_t end, int version)
{
  std::multimap<time_t,Occurrence*> result;

  sqlite3_stmt* select_stmt;
  const char* sql =
      "select O.CALNUM,O.UID,SUMMARY,SEQUENCE,ALLDAY,RECURS,DTSTART,DTEND "
      "from OCCURRENCE O "
      "left join EVENT E on E.UID=O.UID and E.VERSION=O.VERSION "
      "where DTEND>=? and DTSTART<? and O.VERSION=? "
      "order by DTSTART";
  CALI_SQLCHK(_sdb, ::sqlite3_prepare_v2(_sdb,sql,-1,&select_stmt,NULL) );
  sql::bind_int(CALI_HERE,_sdb,select_stmt,1,begin);
  sql::bind_int(CALI_HERE,_sdb,select_stmt,2,end);
  sql::bind_int(CALI_HERE,_sdb,select_stmt,3,version);

  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Occurrence* occ = make_occurrence(
                  ::sqlite3_column_int( select_stmt,0),  // calnum
          safestr(::sqlite3_column_text(select_stmt,1)), // uid
          safestr(::sqlite3_column_text(select_stmt,2)), // summary
                  ::sqlite3_column_int( select_stmt,3),  // sequence
                  ::sqlite3_column_int( select_stmt,4),  // all_day
                  ::sqlite3_column_int( select_stmt,5),  // recurs
                  ::sqlite3_column_int( select_stmt,6),  // dtstart
                  ::sqlite3_column_int( select_stmt,7),  // dtend
          version
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
  Version& v1 = _ver[1];
  // Look it up in version 1 _calendars.
  typedef std::map<int,Calendar*>::iterator CIt;
  for(CIt c =v1._calendar.begin(); c!=v1._calendar.end(); ++c)
  {
    if(c->second->calid == calid)
      return c->second->calnum;
  }

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
      "select 1 + coalesce(max(CALNUM),0) from CALENDAR"
    );
  return calnum;
}


Occurrence* Db::create_event(
    const char*  uid,
    time_t       dtstart,
    time_t       dtend,
    const char*  summary,
    bool         all_day,
    int          calnum,
    int          version
  )
{
  Occurrence* occ =
    make_occurrence(
        calnum,
        uid,
        summary,
        1, // sequence
        all_day,
        false, // recurs
        dtstart,
        dtend,
        version
      );
  occ->event.create();
  occ->create();
  return occ;
}


void
Db::moved(Occurrence* occ, int version)
{
  Version& ver = _ver[version];
  ver._occurrence.erase( occ->key() );
  ver._occurrence[ occ->rekey() ] = occ;
}


void
Db::erase(Occurrence* occ, int version)
{
  occ->destroy();
  _ver[version]._occurrence.erase( occ->key() );
  delete occ;
}


// -- private: --

Occurrence* Db::make_occurrence(
    int          calnum,
    const char*  uid,
    const char*  summary,
    int          sequence,
    bool         all_day,
    bool         recurs,
    time_t       dtstart,
    time_t       dtend,
    int          version
  )
{
  Version& ver = _ver[version];
  Event* event;
  std::map<std::string,Event*>::iterator e = ver._event.find(uid);
  if(e==ver._event.end())
  {
    event = ver._event[uid] =
      new Event(
          *ver._calendar[calnum],
          uid,
          summary,
          sequence,
          all_day,
          recurs
        );
  }
  else
  {
    event = e->second;
  }

  Occurrence::key_type key(uid,dtstart);
  std::map<Occurrence::key_type,Occurrence*>::iterator o =
      ver._occurrence.find(key);
  if(o!=ver._occurrence.end())
      return o->second;
  else
      return ver._occurrence[key] = new Occurrence(*event,dtstart,dtend);
}


} // end namespace calendari
