#ifndef CALENDARI__RECUR_H
#define CALENDARI__RECUR_H 1

#include <libical/icalrecur.h>

namespace calendari {


/** Categorisation for simple recurrence rules. Used in the database.
*   Enumerations correspond to combo-box items. */
enum RecurType {
  RECUR_CUSTOM   =-1,
  RECUR_NONE     =0,
  RECUR_DAILY    =1,
  RECUR_WEEKLY   =2,
  RECUR_WEEKDAYS =3,
  RECUR_BIWEEKLY =4,
  RECUR_MONTHLY  =5,
  RECUR_YEARLY   =6
};
RecurType int2recur(int r);
int recur2int(RecurType r);


/** Helper function used by process_rrule(). Calculated the
*   database enumeration to use to describe a recurring event. Anything
*   complicated goes to RECUR_CUSTOM. */
RecurType add_recurrence(RecurType recur, icalrecurrencetype_frequency freq);


} // end namespace calendari

#endif // CALENDARI__RECUR_H
