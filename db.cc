#include "err.h"
#include "sql.h"
#include "db.h"

namespace calendari {


Db::Db(const char* dbname)
{
  if( SQLITE_OK != ::sqlite3_open(dbname,&_db) )
      calendari::sql::error(_db,__FILE__,__LINE__);
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
Db::load_calendars(void)
{
  sqlite3_stmt* select_stmt;
  const char* sql =
      "select CALID, CALNAME, POSITION, COLOUR "
      "from CALENDAR "
      "order by POSITION";
  if( SQLITE_OK != ::sqlite3_prepare_v2(_db,sql,-1,&select_stmt,NULL) )
      sql::error(_db,__FILE__,__LINE__);

  int position = 0;
  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Calendar* cal = new Calendar(
          (const char*)::sqlite3_column_text(select_stmt,0), // calid
          (const char*)::sqlite3_column_text(select_stmt,1), // name
          position++,
//                       ::sqlite3_column_int( select_stmt,2), // position
          (const char*)::sqlite3_column_text(select_stmt,3)  // colour
        );
      _calendar.insert(std::make_pair(cal->calid,cal));
    }
    else if(return_code==SQLITE_DONE)
    {
      break;
    }
    else
    {
      calendari::sql::error(_db,__FILE__,__LINE__);
      break;
    }
  }
  if( SQLITE_OK != ::sqlite3_finalize(select_stmt) )
      calendari::sql::error(_db,__FILE__,__LINE__);
}

  
std::multimap<time_t,Occurrence*>
Db::find(time_t begin, time_t end)
{
  std::multimap<time_t,Occurrence*> result;

  sqlite3_stmt* select_stmt;
  const char* sql =
      "select O.UID,DTSTART,DTEND,SUMMARY,ALLDAY,CALID "
      "from OCCURRENCE O "
      "left join EVENT E on E.UID=O.UID "
      "where DTSTART>=? and DTEND<? "
      "order by DTSTART";
  if( SQLITE_OK != ::sqlite3_prepare_v2(_db,sql,-1,&select_stmt,NULL) )
      sql::error(_db,__FILE__,__LINE__);

  sql::bind_int(_db,select_stmt,1,begin);
  sql::bind_int(_db,select_stmt,2,end);

  while(true)
  {
    int return_code = ::sqlite3_step(select_stmt);
    if(return_code==SQLITE_ROW)
    {
      Occurrence* occ = make_occurrence(
          (const char*)::sqlite3_column_text(select_stmt,0), // uid
                       ::sqlite3_column_int( select_stmt,1), // dtstart
                       ::sqlite3_column_int( select_stmt,2), // dtend
          (const char*)::sqlite3_column_text(select_stmt,3), // summary
                       ::sqlite3_column_int( select_stmt,4), // all_day
          (const char*)::sqlite3_column_text(select_stmt,5)  // calid
        );
      result.insert(std::make_pair(occ->dtstart,occ));
    }
    else if(return_code==SQLITE_DONE)
    {
      break;
    }
    else
    {
      calendari::sql::error(_db,__FILE__,__LINE__);
      break;
    }
  }
  if( SQLITE_OK != ::sqlite3_finalize(select_stmt) )
      calendari::sql::error(_db,__FILE__,__LINE__);
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
    event = _event[uid] = new Event( *_calendar[calid] ); //??
    event->uid = uid;
    event->summary = summary;
    event->sequence = 0; //??
    event->all_day = all_day;
  }
  else
  {
    event = e->second;
  }

  Occurrence* result;
  std::pair<time_t,std::string> idx(dtstart,uid);
  std::map<std::pair<time_t,std::string>,Occurrence*>::iterator o =
      _occurrence.find(idx);
  if(o==_occurrence.end())
  {
     result = _occurrence[idx] = new Occurrence(*event);
     result->dtstart = dtstart;
     result->dtend = dtend;
  }
  else
  {
    result = o->second;
  }
  return result;
}


} // end namespace calendari
