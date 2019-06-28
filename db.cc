#include "db.h"

#include "err.h"
#include "ics.h"
#include "queue.h"
#include "sql.h"
#include "util.h"

#include <cassert>
#include <cstdarg>
#include <libical/ical.h>
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
      "  DTSTAMP  integer," // Last time this calendar was modified.
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
      "  RECURS   integer," // Summarises all recurrence rules.
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
      "  RECURS   integer," // The recurrence rule that made this occurrence.
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
  sql::exec(CALI_HERE,_sdb,
      "create table if not exists SETTING ("
      "  KEY      string,"
      "  VALUE    string,"
      "  primary key(KEY)"
      ")"
    );
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
    // ?? Does this need to be restricted by calnum...?
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
  const char* sql =
      "select CALID,CALNUM,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW "
      "from CALENDAR "
      "where VERSION=? "
      "order by POSITION";
  sql::Statement select_stmt(CALI_HERE,_sdb,sql);
  sql::bind_int(CALI_HERE,_sdb,select_stmt,1,version);
  _load_calendars(select_stmt,version);
}


Calendar*
Db::load_calendar(int calnum, int version)
{
  const char* sql =
      "select CALID,CALNUM,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW "
      "from CALENDAR "
      "where VERSION=? and CALNUM=? "
      "order by POSITION";
  sql::Statement select_stmt(CALI_HERE,_sdb,sql);
  sql::bind_int(CALI_HERE,_sdb,select_stmt,1,version);
  sql::bind_int(CALI_HERE,_sdb,select_stmt,2,calnum);
  _load_calendars(select_stmt,version);
  return calendar(calnum);
}


void
Db::_load_calendars(sql::Statement& select_stmt, int version)
{
  Version& ver = _ver[version];
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
                  ::sqlite3_column_int( select_stmt,5),  // position
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
}


std::multimap<time_t,Occurrence*>
Db::find(time_t begin, time_t end, int version)
{
  std::multimap<time_t,Occurrence*> result;

  const char* sql =
      "select O.CALNUM,O.UID,SUMMARY,SEQUENCE,ALLDAY,"
          "E.RECURS,DTSTART,DTEND,O.RECURS "
      "from OCCURRENCE O "
      "left join EVENT E on E.UID=O.UID and E.VERSION=O.VERSION "
      "where DTEND>=? and DTSTART<? and O.VERSION=? "
      "order by DTSTART";
  sql::Statement select_stmt(CALI_HERE,_sdb,sql);
  sql::bind_int64(CALI_HERE,_sdb,select_stmt,1,begin);
  sql::bind_int64(CALI_HERE,_sdb,select_stmt,2,end);
  sql::bind_int(  CALI_HERE,_sdb,select_stmt,3,version);

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
        int2recur(::sqlite3_column_int( select_stmt,5)), // event recurs
                  ::sqlite3_column_int( select_stmt,6),  // dtstart
                  ::sqlite3_column_int( select_stmt,7),  // dtend
        int2recur(::sqlite3_column_int( select_stmt,8)), // occ recurs
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
  // ?? SQL begin..commit here - but it would sometimes be re-entrant :(
  int calnum =1;
  std::string sql_calid = sql::quote(calid);
  bool ok =
    sql::query_val(
        CALI_HERE,_sdb,
        calnum,                                  // <== output
        "select CALNUM from CALENDAR where CALID='%s' order by VERSION",
        sql_calid.c_str()
      );
  if(!ok)
  {
    // ...well find the next free number, then.
    sql::query_val(
        CALI_HERE,_sdb,
        calnum,                                  // <== output
        "select 1 + coalesce(max(CALNUM),0) from CALENDAR"
      );
  }
  return calnum;
}


Calendar*
Db::create_calendar(
    const char*  calid,
    const char*  calname,
    const char*  path,
    bool         readonly,
    const char*  colour,
    bool         show,
    int          version
  )
{
  // ?? Potential race here - we don't get new calnum and write new calendar in
  // a single transaction.
  int new_calnum;
  sql::query_val(CALI_HERE, _sdb, new_calnum,
      "select 1 + coalesce(max(CALNUM),0) from CALENDAR"
    );
  int new_position;
  sql::query_val(CALI_HERE, _sdb, new_position,
      "select 1 + coalesce(max(POSITION),0) from CALENDAR"
    );
  assert(_ver[version]._calendar.count(new_calnum)==0);
  Calendar* cal = new Calendar(
      version,
      calid,
      new_calnum,
      calname,
      (path? path: ""),
      readonly,
      new_position,
      colour,
      show
    );
  cal->create();
  Queue::inst().flush();
  _ver[version]._calendar.insert(std::make_pair(new_calnum,cal));
  return cal;
}


void
Db::erase_calendar(Calendar* cal)
{
  assert(cal);
  sql::exec(CALI_HERE,_sdb,"begin");
  try
  {
    sql::execf(CALI_HERE,_sdb,
        "delete from OCCURRENCE where VERSION=%d and CALNUM=%d",
        cal->version,cal->calnum);
    sql::execf(CALI_HERE,_sdb,
        "delete from EVENT where VERSION=%d and CALNUM=%d",
        cal->version,cal->calnum);
    sql::execf(CALI_HERE,_sdb,
        "delete from CALENDAR where VERSION=%d and CALNUM=%d",
        cal->version,cal->calnum);
    sql::exec(CALI_HERE,_sdb,"commit");
    _ver[cal->version].purge(cal->calnum);
    _ver[cal->version]._calendar.erase(cal->calnum);
    delete cal;
  }
  catch(...)
  {
    try{ sql::exec(CALI_HERE,_sdb,"rollback"); } catch(...) {}
    throw;
  }
}


Occurrence*
Db::create_event(
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
        RECUR_NONE, // does not recur
        dtstart,
        dtend,
        RECUR_NONE, // does not recur
        version
      );
  occ->event.create();
  occ->create();
  return occ;
}


icalcomponent*
Db::vevent(const char* uid, int version)
{
  // Read in VEVENTS from the database...
  const char* sql = "select VEVENT from EVENT where VERSION=? and UID=?";
  sql::Statement select_evt(CALI_HERE,_sdb,sql);
  sql::bind_int( CALI_HERE,_sdb,select_evt,1,version);
  sql::bind_text(CALI_HERE,_sdb,select_evt,2,uid,-1);

  const char* veventz =NULL;
  int return_code = ::sqlite3_step(select_evt);
  if(return_code==SQLITE_ROW)
  {
    veventz = safestr(::sqlite3_column_text(select_evt,0));
  }
  else if(return_code!=SQLITE_DONE)
  {
    calendari::sql::error(CALI_HERE,_sdb);
  }

  icalcomponent* vevent =NULL;
  if(veventz && veventz[0])
  {
    vevent = icalparser_parse_string(veventz);
    // ?? Check for error.
  }
  else
  {
    vevent = ics::make_new_vevent(uid);
  }
  return vevent;
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

Occurrence*
Db::make_occurrence(
    int          calnum,
    const char*  uid,
    const char*  summary,
    int          sequence,
    bool         all_day,
    RecurType    evt_recurs,
    time_t       dtstart,
    time_t       dtend,
    RecurType    occ_recurs,
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
          evt_recurs
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
  return ver._occurrence[key] = new Occurrence(*event,dtstart,dtend,occ_recurs);
}


bool
Db::_setting(const char* key, std::string& val) const
{
  bool result = false;
  const char* sql = "select VALUE from SETTING where KEY=?";
  sql::Statement select_stg(CALI_HERE,_sdb,sql);
  sql::bind_text(CALI_HERE,_sdb,select_stg,1,key,-1);

  int return_code = ::sqlite3_step(select_stg);
  if(return_code==SQLITE_ROW)
  {
    val = safestr(::sqlite3_column_text(select_stg,0));
    result = true;
  }
  else if(return_code!=SQLITE_DONE)
  {
    calendari::sql::error(CALI_HERE,_sdb);
  }
  return result;
}


void
Db::_set_setting(const char* key, const char* val) const
{
  CALI_SQLCHK(_sdb, ::sqlite3_exec(_sdb, "begin", 0, 0, 0) );

  std::string oldval;
  if(!_setting(key,oldval))
  {
    const char* sql = "insert into SETTING (KEY,VALUE) values (?,?)";
    sql::Statement insert_stg(CALI_HERE,_sdb,sql);
    sql::bind_text(CALI_HERE,_sdb,insert_stg,1,key,-1);
    sql::bind_text(CALI_HERE,_sdb,insert_stg,2,val,-1);
    sql::step_reset(CALI_HERE,_sdb,insert_stg);
  }
  else if(oldval!=val)
  {
    const char* sql = "update SETTING set VALUE=? where KEY=?";
    sql::Statement update_stg(CALI_HERE,_sdb,sql);
    sql::bind_text(CALI_HERE,_sdb,update_stg,1,val,-1);
    sql::bind_text(CALI_HERE,_sdb,update_stg,2,key,-1);
    sql::step_reset(CALI_HERE,_sdb,update_stg);
  }
  CALI_SQLCHK(_sdb, ::sqlite3_exec(_sdb, "commit", 0, 0, 0) );
}


} // end namespace calendari
