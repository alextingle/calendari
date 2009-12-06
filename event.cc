#include "event.h"

#include <cassert>

namespace calendari {


Calendar::Calendar(
    const char* calid_,
    const char* name_,
    int         pos_,
    const char* col_,
    int         show_
  )
  : calid(calid_), _name(name_), _position(pos_), _colour(col_), _show(show_)
{}


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


bool
Occurrence::set_start(time_t start_)
{
  if(_dtstart==start_)
      return false;
  assert(_dtend >= _dtstart);
  time_t duration = _dtend - _dtstart;
  _dtstart = start_;
  _dtend = start_ + duration;
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
  // ?? Enforce restrictions from all_day events.
  _dtend = end_;
  return true;
}


} // end namespace calendari
