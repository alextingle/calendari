#include "recur.h"

#include "err.h"

namespace calendari {


RecurType
int2recur(int r)
{
  if(-1 <= r && r <= 6)
      return static_cast<RecurType>(r);
  CALI_WARN(0,"Unknown recur type: %d",r);
  return RECUR_NONE;
}


int
recur2int(RecurType r)
{
  return static_cast<int>(r);
}


RecurType
add_recurrence(RecurType recur, icalrecurrencetype_frequency freq)
{
  RecurType r;
  switch(freq)
  {
    case ICAL_SECONDLY_RECURRENCE:
    case ICAL_MINUTELY_RECURRENCE:
    case ICAL_HOURLY_RECURRENCE:
    default:
        return RECUR_CUSTOM;
    case ICAL_DAILY_RECURRENCE:   r = RECUR_DAILY;   break;
    case ICAL_WEEKLY_RECURRENCE:  r = RECUR_WEEKLY;  break;
    case ICAL_MONTHLY_RECURRENCE: r = RECUR_MONTHLY; break;
    case ICAL_YEARLY_RECURRENCE:  r = RECUR_YEARLY;  break;
    case ICAL_NO_RECURRENCE:      r = RECUR_NONE;    break;
  }
  if(recur == RECUR_NONE || recur == r)
      return r;
  else
      return RECUR_CUSTOM;
}


} // end namespace calendari
