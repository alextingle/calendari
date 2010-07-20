#include "event.h"

#include "db.h"
#include "queue.h"
#include "sql.h"

#include <cassert>
#include <cstring>
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
Calendar::create(void)
{
  static Queue& q( Queue::inst() );
  q.pushf(
      "insert into CALENDAR ("
          "VERSION,"
          "CALNUM,"
          "CALID,"
          "CALNAME,"
          "DTSTAMP,"
          "PATH,"
          "READONLY,"
          "POSITION,"
          "COLOUR,"
          "SHOW"
      ") values ("
          "%d,"   // VERSION
          "%d,"   // CALNUM
          "'%s'," // CALID
          "'%s'," // CALNAME
          "%d,"   // DTSTAMP
          "'%s'," // PATH
          "%d,"   // READONLY
          "%d,"   // POSITION
          "'%s'," // COLOUR
          "%d"    // SHOW
      ");",
      version,
      calnum,
      sql::quote(calid).c_str(),
      sql::quote(_name).c_str(),
      ::time(NULL),
      sql::quote(_path).c_str(),
      (_readonly? 1: 0),
      _position,
      sql::quote(_colour).c_str(),
      (_show? 1: 0)
    );
}


void
Calendar::set_name(const std::string& s)
{
  if(s==_name)
      return;
  _name = s;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set CALNAME='%s' where VERSION=%d and CALNUM=%d",
      sql::quote(s).c_str(), version, calnum
    );
  touch();
}


void
Calendar::set_path(const std::string& s)
{
  if(s==_path)
      return;
  _path = s;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set PATH='%s' where VERSION=%d and CALNUM=%d",
      sql::quote(s).c_str(), version, calnum
    );
  touch();
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
  if(s==_colour)
      return;
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

void
Calendar::touch(void)
{
  static Queue& q( Queue::inst() );
  q.pushf(
      "update CALENDAR set DTSTAMP=%lu where VERSION=%d and CALNUM=%d",
      ::time(NULL), version, calnum
    );
}



// -- Event --

Event::Event(
    Calendar&    c,
    const char*  u,
    const char*  s,
    int          q,
    bool         a,
    RecurType    r
  )
  : uid(u),
    _calendar(&c),
    _summary(s),
    _sequence(q),
    _all_day(a),
    _recurs(r),
    _vevent(NULL),
    _ref_count(0)
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
      recur2int(_recurs)
    );
}


bool
Event::readonly(void) const
{
  // For the time being, just make all recurring events readonly.
  return _calendar->readonly() || _recurs != RECUR_NONE;
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
  if(s==_summary)
      return;
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
Event::add_recurs(RecurType r)
{
  RecurType new_recurs = add_recurrence(_recurs,r);
  if(_recurs==new_recurs)
      return;
  _recurs = new_recurs;
  // --
  static Queue& q( Queue::inst() );
  q.pushf(
      "update EVENT set RECURS=%d where VERSION=%d and UID='%s'",
      recur2int(_recurs),
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
  if(s[0])
  {
      // Set / change the description.
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
  }
  else
  {
      // Erase the description.
      if(iprop)
          icalcomponent_remove_property(_vevent,iprop);
      else
          return; // No change.
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
  _calendar->touch();
}


void
Event::load_vevent(void) const
{
  if(!_vevent)
      _vevent = Queue::inst().db()->vevent(uid.c_str(),_calendar->version);
}


// -- Occurrence --

Occurrence::Occurrence(Event& e, time_t t0, time_t t1, RecurType r):
  event(e), _dtstart(t0), _dtend(t1), _recurs(r), _key(e.uid,t0)
{
  ++event._ref_count;
}


Occurrence::~Occurrence(void)
{
  --event._ref_count;
}


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
          "DTEND,"
          "RECURS"
      ") values ("
          "%d,"   // VERSION
          "%d,"   // CALNUM
          "'%s'," // UID
          "%d,"   // DTSTART
          "%d,"   // DTEND
          "%d"    // RECURS
      ");",
      event.calendar().version,
      event.calendar().calnum,
      sql::quote(event.uid).c_str(),
      _dtstart,
      _dtend,
      recur2int(_recurs)
    );
  event.add_recurs(_recurs);
  event.calendar().touch();
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
  std::string uid_sql = sql::quote(event.uid);
  q.pushf(
      "delete from OCCURRENCE "
        "where VERSION=%d and UID='%s' and DTSTART=%d and DTEND=%d",
      event.calendar().version,
      uid_sql.c_str(),
      _dtstart,_dtend
    );
  if(event._ref_count == 1)
  {
    q.pushf(
      "delete from EVENT "
        "where VERSION=%d and UID='%s' and 0=("
          "select count(0) from OCCURRENCE O where VERSION=%d and UID='%s'"
        ")",
      event.calendar().version, uid_sql.c_str(),
      event.calendar().version, uid_sql.c_str()
    );
  }
  event.calendar().touch();
}


} // end namespace calendari
