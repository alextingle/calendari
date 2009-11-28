#ifndef CALENDARI__DB_H
#define CALENDARI__DB_H 1

#include "event.h"

#include <sqlite3.h>
#include <map>

namespace calendari {

/*

-- VERSION slices the database between the current (VERSION=0) contents and
-- content that is being generated (VERSION>0)

create table EVENT (
  VERSION  integer,
  UID      string,
  SUMMARY  string,
  CALID    string,
  SEQUENCE integer,
  VEVENT   blob,
  primary key(VERSION,UID)
);

create table OCCURRENCE (
  VERSION  integer,
  UID      string,
  DTSTART  integer, -- time_t (need to allow for 'all-day')
  DTEND    integer, -- time_t
  primary key(VERSION,UID,DTSTART)
);

create table CALENDAR (
  VERSION  integer,
  CALID    string,
  CALNAME  string,
  POSITION integer, -- ordering within the UI's calendar list.
  COLOUR   string,
  primary key(VERSION,CALID)
);

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

delete from EVENT where


*/

class Db
{
public:
  explicit Db(const char* dbname);
  ~Db(void);

  void load_calendars(void);  
  std::multimap<time_t,Occurrence*> find(time_t begin, time_t end);

  const std::map<std::string,Calendar*>& calendars(void) const
    { return _calendar; }

private:
  Occurrence* make_occurrence(
      const char*  uid,
      time_t       dtstart,
      time_t       dtend,
      const char*  summary,
      const char*  calid
    );

  sqlite3* _db;

  std::map<std::string,Calendar*>                      _calendar;
  std::map<std::string,Event*>                         _event;
  std::map<std::pair<time_t,std::string>,Occurrence*>  _occurrence;
};


} // end namespace

#endif // CALENDARI__DB_H
