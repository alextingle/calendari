#include "event.h"

#include <cassert>

namespace calendari {


bool
Occurrence::set_start(time_t start_)
{
  if(dtstart==start_)
      return false;
  assert(dtend >= dtstart);
  time_t duration = dtend - dtstart;
  dtstart = start_;
  dtend = start_ + duration;
  return true;
}


bool
Occurrence::set_end(time_t end_)
{
  if(dtend==end_)
      return false;
  if(event.all_day && end_<=dtstart)
      return false;
  if(!event.all_day && end_<dtstart)
      return false;
  // ?? Enforce restrictions from all_day events.
  dtend = end_;
  return true;
}


} // end namespace calendari
