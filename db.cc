#include "db.h"

#include "err.h"
#include "queue.h"
#include "sql.h"
#include "util.h"

#include <cstdarg>
#include <set>

namespace calendari {


Db::Db(const char* dbname)
{
  if( SQLITE_OK != ::sqlite3_open(dbname,&_db) )
      CALI_ERRO(1,0,"Failed to open database %s",dbname);
  Queue::inst().set_db( this );
}


Db::~Db(void)
{
  if(_db)
    ::sqlite3_close(_db);
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
  exec(
      "create table CALENDAR ("
      "  VERSION  integer,"
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
  exec(
      "create table EVENT ("
      "  VERSION  integer,"
      "  UID      string,"
      "  SUMMARY  string,"
      "  CALID    string,"
      "  SEQUENCE integer,"
      "  ALLDAY   boolean,"
      "  VEVENT   blob,"
      "  primary key(VERSION,UID)"
      ")"
    );
  exec(
      "create table OCCURRENCE ("
      "  VERSION  integer,"
      "  UID      string,"
      "  DTSTART  integer," // time_t (need to allow for 'all-day')
      "  DTEND    integer," // time_t
      "  primary key(VERSION,UID,DTSTART)"
      ")"
    );
  exec("create index OCC_START_INDEX on OCCURRENCE(DTSTART)");
  exec("create index OCC_END_INDEX on OCCURRENCE(DTEND)");
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
Db::refresh_cal(const char* calid, int version)
{
  exec("begin");
  try
  {
    std::string sql_calid = Queue::quote(calid);
    std::set<std::string> uids;
    sqlite3_stmt* select_stmt;
    const char* sql ="select UID from EVENT where VERSION=1 and CALID=?";
    CALI_SQLCHK(_db, ::sqlite3_prepare_v2(_db,sql,-1,&select_stmt,NULL) );
    sql::bind_text(CALI_HERE,_db,select_stmt,1,sql_calid.c_str());
    while(true)
    {
      int return_code = ::sqlite3_step(select_stmt);
      if(return_code==SQLITE_ROW)
      {
        uids.insert(safestr(::sqlite3_column_text(select_stmt,0)));
      }
      else if(return_code==SQLITE_DONE)
      {
        break;
      }
      else
      {
        calendari::sql::error(CALI_HERE,_db);
        break;
      }
    }
    CALI_SQLCHK(_db,  ::sqlite3_finalize(select_stmt) );
    if(!uids.empty())
    {
      sqlite3_stmt* delete_stmt;
      sql = "delete from OCCURRENCE where VERSION=1 and UID=?";
      CALI_SQLCHK(_db,  ::sqlite3_prepare_v2(_db,sql,-1,&delete_stmt,NULL) );
      for(std::set<std::string>::iterator u=uids.begin(); u!=uids.end(); ++u)
      {
        sql::bind_text(CALI_HERE,_db,delete_stmt,1,u->c_str());
        int return_code = ::sqlite3_step(delete_stmt);
        if(return_code!=SQLITE_DONE)
        {
          calendari::sql::error(CALI_HERE,_db);
          break;
        }
        CALI_SQLCHK(_db, ::sqlite3_reset(delete_stmt) );
      }
      CALI_SQLCHK(_db, ::sqlite3_finalize(delete_stmt) );
    }
    execf("delete from EVENT where VERSION=1 and CALID='%s'",sql_calid.c_str());
    execf("update OCCURRENCE set VERSION=1 where VERSION=%d",version);
    execf("update EVENT set VERSION=1 where VERSION=%d",version);
    execf("delete from  CALENDAR where VERSION=%d",version);
    exec("commit");
  }
  catch(...)
  {
    try{ exec("rollback"); } catch(...) {}
    throw;
  }
}


void
Db::exec(const char* sql)
{
  char* errmsg;
  int ret = ::sqlite3_exec(_db,sql,NULL,NULL,&errmsg);
  if(SQLITE_OK != ret)
  {
    CALI_ERRO(1,0,"sqlite error %i: %s in SQL \"%s\"",ret,errmsg,sql);
    ::sqlite3_free(errmsg);
  }
}


void
Db::execf(const char* format, ...)
{
  va_list args;
  va_start(args,format);
  char buf[256];
  int ret = vsnprintf(buf,sizeof(buf),format,args);
  if(ret>=sizeof(buf))
      CALI_ERRO(1,0,"SQL too large for buffer."); //??
  exec(buf);
  va_end(args);
}


void
Db::load_calendars(void)
{
  sqlite3_stmt* select_stmt;
  const char* sql =
      "select CALID,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW "
      "from CALENDAR "
      "where VERSION=1 "
      "order by POSITION";
  CALI_SQLCHK(_db, ::sqlite3_prepare_v2(_db,sql,-1,&select_stmt,NULL) );

  int position = 0;
  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Calendar* cal = new Calendar(
          safestr(::sqlite3_column_text(select_stmt,0)), // calid
          safestr(::sqlite3_column_text(select_stmt,1)), // name
          safestr(::sqlite3_column_text(select_stmt,2)), // path
                  ::sqlite3_column_int( select_stmt,3),  // readonly
          position++,
//??              ::sqlite3_column_int( select_stmt,4),  // position
          safestr(::sqlite3_column_text(select_stmt,5)), // colour
                  ::sqlite3_column_int( select_stmt,6)   // show
        );
      _calendar.insert(std::make_pair(cal->calid,cal));
    }
    else if(return_code==SQLITE_DONE)
    {
      break;
    }
    else
    {
      calendari::sql::error(CALI_HERE,_db);
      break;
    }
  }
  CALI_SQLCHK(_db, ::sqlite3_finalize(select_stmt) );
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
  CALI_SQLCHK(_db, ::sqlite3_prepare_v2(_db,sql,-1,&select_stmt,NULL) );
  sql::bind_int(CALI_HERE,_db,select_stmt,1,begin);
  sql::bind_int(CALI_HERE,_db,select_stmt,2,end);

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
      calendari::sql::error(CALI_HERE,_db);
      break;
    }
  }
  CALI_SQLCHK(_db, ::sqlite3_finalize(select_stmt) );
  return result;
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
