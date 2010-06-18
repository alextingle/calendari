#include "event.h"

#include "db.h"
#include "queue.h"
#include "sql.h"

#include <cassert>
#include <libical/ical.h>

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
Calendar::set_name(const std::string& s)
{
  _name = s;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set CALNAME='%s' where VERSION=%d and CALNUM=%d",
      sql::quote(s).c_str(), version, calnum
    );
}


void
Calendar::set_path(const std::string& s)
{
  _path = s;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set PATH='%s' where VERSION=%d and CALNUM=%d",
      sql::quote(s).c_str(), version, calnum
    );
}


void
Calendar::set_position(int p)
{
  if(p==_position)
      return;
  _position = p;
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set POSITION=%d where VERSION=%d and CALNUM=%d",
      _position, version, calnum
    );
}

void
Calendar::set_colour(const std::string& s)
{
  _colour = s;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set COLOUR='%s' where VERSION=%d and CALNUM=%d",
      sql::quote(s).c_str(), version, calnum
    );
}



void
Calendar::toggle_show(void)
{
  _show = !_show;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set SHOW=%d where VERSION=%d and CALNUM=%d",
      _show, version, calnum
    );
}


// -- Event --

Event::Event(
    Calendar&    c,
    const char*  u,
    const char*  s,
    int          q,
    bool         a,
    bool         r
  )
  : uid(u),
    _calendar(&c),
    _summary(s),
    _sequence(q),
    _all_day(a),
    _recurs(r),
    _vevent(NULL)
{}


Event::~Event(void)
{
  if(_vevent)
  {
    icalcomponent_free(_vevent);
    _vevent = NULL;
  }
}


void
Event::create(void)
{
  static Queue& q( Queue::inst() );
  q.pushf(
      "insert into EVENT ("
          "VERSION,"
          "CALNUM,"
          "UID,"
          "SUMMARY,"
          "SEQUENCE,"
          "ALLDAY,"
          "RECURS,"
          "VEVENT"
      ") values ("
          "%d,"   // VERSION
          "%d,"   // CALNUM
          "'%s'," // UID
          "'%s'," // SUMMARY
          "%d,"   // SEQUENCE
          "%d,"   // ALLDAY
          "%d,"   // RECURS
          "''"    // VEVENT
      ");",
      _calendar->version,
      _calendar->calnum,
      sql::quote(uid).c_str(),
      sql::quote(_summary).c_str(),
      _sequence,
      (_all_day? 1: 0),
      (_recurs? 1: 0)
    );
}


bool
Event::readonly(void) const
{
  // For the time being, just make all recurring events readonly.
  return _calendar->readonly() || _recurs;
}


const char*
Event::description(void) const
{
  load_vevent();
  icalproperty* iprop =
      icalcomponent_get_first_property(_vevent,ICAL_DESCRIPTION_PROPERTY);
  if(!iprop)
      return "";
  const char* desc = icalproperty_get_description(iprop);
  return( desc? desc: "" );
}


void
Event::set_calendar(Calendar& c)
{
  _calendar = &c;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set CALNUM=%d where VERSION=%d and UID='%s'",
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
  increment_sequence();
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
  increment_sequence();
}


void
Event::set_all_day(bool v)
{
  if(_all_day==v)
      return;
  _all_day = v;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set ALLDAY=%d where VERSION=%d and UID='%s'",
      (_all_day? 1: 0),
      _calendar->version,
      sql::quote(uid).c_str()
    );
  increment_sequence();
}


void
Event::set_description(const char* s)
{
  assert(s);
  load_vevent();
  icalproperty* iprop =
      icalcomponent_get_first_property(_vevent,ICAL_DESCRIPTION_PROPERTY);
  if(iprop)
  {
    const char* old_desc = icalproperty_get_description(iprop);
    if(old_desc && 0==::strcmp(s,old_desc))
        return; // No change.
    icalproperty_set_description(iprop,s);
  }
  else
  {
    iprop = icalproperty_new_description(s);
    icalcomponent_add_property(_vevent,iprop);
  }
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set VEVENT='%s' where VERSION=%d and UID='%s'",
      sql::quote(icalcomponent_as_ical_string(_vevent)).c_str(),
      _calendar->version,
      sql::quote(uid).c_str()
    );
  increment_sequence();
}


void
Event::increment_sequence(void)
{
  assert(!_calendar->readonly());
  ++_sequence;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set SEQUENCE=%d where VERSION=%d and UID='%s'",
      _sequence,
      _calendar->version,
      sql::quote(uid).c_str()
    );
}


void
Event::load_vevent(void) const
{
  if(!_vevent)
      _vevent = Queue::inst().db()->vevent(uid.c_str(),_calendar->version);
}


// -- Occurrence --

void
Occurrence::create(void)
{
  assert(!event.calendar().readonly());
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
      event.calendar().version,
      event.calendar().calnum,
      sql::quote(event.uid).c_str(),
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
  event.increment_sequence();
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
  event.increment_sequence();
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
