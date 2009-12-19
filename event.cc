#include "event.h"

#include "queue.h"
#include "sql.h"

#include <cassert>

namespace calendari {


// -- Calendar --

Calendar::Calendar(
    int         version_,
    const char* calid_,
    int         calnum_,
    const char* name_,
    const char* path_,
    int         readonly_,
    int         pos_,
    const char* col_,
    int         show_
  )
  : version(version_),
    calid(calid_),
    calnum(calnum_),
    _name(name_),
    _path(path_),
    _readonly(readonly_),
    _position(pos_),
    _colour(col_),
    _show(show_)
{}


void
Calendar::toggle_show(void)
{
  _show = !_show;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set SHOW=%d where VERSION=%d and CALID='%s'",
      _show, version, sql::quote(calid).c_str()
    );
}


// -- Event --

Event::Event(
    Calendar&    c,
    const char*  u,
    const char*  s,
    int          q,
    bool         a
  )
  : uid(u),
    _calendar(&c),
    _summary(s),
    _sequence(q),
    _all_day(a)
{}


void
Event::create(int version)
{
  static Queue& q( Queue::inst() );
  q.pushf(
      "insert into EVENT ("
          "VERSION,"
          "CALNUM,"
          "UID,"
          "SUMMARY,"
          "CALID,"
          "SEQUENCE,"
          "ALLDAY,"
          "VEVENT"
      ") values ("
          "%d,"   // VERSION
          "%d,"   // CALNUM
          "'%s'," // UID
          "'%s'," // SUMMARY
          "'%s'," // CALID
          "%d,"   // SEQUENCE
          "%d,"   // ALLDAY
          "''"    // VEVENT
      ");",
      version,
      _calendar->calnum,
      uid.c_str(),
      _summary.c_str(),
      _calendar->calid.c_str(),
      _sequence,
      (_all_day? 1: 0)
    );
}


void
Event::set_calendar(Calendar& c)
{
  _calendar = &c;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set CALID='%s',CALNUM=%d where VERSION=%d and UID='%s'",
      sql::quote(_calendar->calid).c_str(),
      _calendar->calnum,
      _calendar->version,
      sql::quote(uid).c_str()
    );
  q.pushf(
      "update OCCURRENCE set CALNUM=%d where VERSION=%d and UID='%s'",
      _calendar->calnum,
      _calendar->version,
      sql::quote(uid).c_str()
    );
}


void
Event::set_summary(const std::string& s)
{
  _summary = s;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set SUMMARY='%s' where VERSION=%d and UID='%s'",
      sql::quote(s).c_str(),
      _calendar->version,
      sql::quote(uid).c_str()
    );
  q.pushf(
      "update EVENT set SEQUENCE=SEQUENCE+1 where VERSION=%d and UID='%s'",
      _calendar->version,
      sql::quote(uid).c_str()
    );
}


// -- Occurrence --

void
Occurrence::create(int version)
{
  static Queue& q( Queue::inst() );
  q.pushf(
      "insert into OCCURRENCE ("
          "VERSION,"
          "CALNUM,"
          "UID,"
          "DTSTART,"
          "DTEND"
      ") values ("
          "%d,"   // VERSION
          "%d,"   // CALNUM
          "'%s'," // UID
          "%d,"   // DTSTART
          "%d"    // DTEND
      ");",
      version,
      event.calendar().calnum,
      event.uid.c_str(),
      _dtstart,
      _dtend
    );
}


bool
Occurrence::set_start(time_t start_)
{
  if(_dtstart==start_)
      return false;
  assert(_dtend >= _dtstart);
  time_t old_dtstart = _dtstart;
  time_t old_dtend   = _dtend;
  // --
  time_t duration = _dtend - _dtstart;
  _dtstart = start_;
  _dtend = start_ + duration;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update OCCURRENCE set DTSTART=%d,DTEND=%d "
        "where VERSION=%d and UID='%s' and DTSTART=%d and DTEND=%d",
      _dtstart,_dtend,
      event.calendar().version,
      sql::quote(event.uid).c_str(),
      old_dtstart,old_dtend
    );
  return true;
}


bool
Occurrence::set_end(time_t end_)
{
  if(_dtend==end_)
      return false;
  if(event.all_day() && end_<=_dtstart)
      return false;
  if(!event.all_day() && end_<_dtstart)
      return false;
  time_t old_dtend = _dtend;
  // ?? Enforce restrictions from all_day events.
  _dtend = end_;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update OCCURRENCE set DTEND=%d "
        "where VERSION=%d and UID='%s' and DTSTART=%d and DTEND=%d",
      _dtend,
      event.calendar().version,
      sql::quote(event.uid).c_str(),
      _dtstart,old_dtend
    );
  return true;
}


void
Occurrence::destroy(void)
{
  static Queue& q( Queue::inst() );
  q.pushf(
      "delete from OCCURRENCE "
        "where VERSION=%d and UID='%s' and DTSTART=%d and DTEND=%d",
      event.calendar().version,
      sql::quote(event.uid).c_str(),
      _dtstart,_dtend
    );
}


} // end namespace calendari
