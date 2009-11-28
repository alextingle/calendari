#ifndef CALENDARI__CALENDAR_LIST_H
#define CALENDARI__CALENDAR_LIST_H 1

#include <gtk/gtk.h>

namespace calendari {

class Calendari;


/** Main application data store. */
struct CalendarList
{
  GtkListStore* liststore_cal; ///< List of calendars

  /** Populate members. */
  void build(Calendari* cal, GtkBuilder* builder);
  
  void add(void);
};


} // end namespace

#endif // CALENDARI__CALENDAR_LIST_H
