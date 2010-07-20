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
add_recurrence(RecurType r0, RecurType r1)
{
  if(r0 == RECUR_NONE || r0 == r1)
      return r1;
  else
      return RECUR_CUSTOM;
}


} // end namespace calendari
