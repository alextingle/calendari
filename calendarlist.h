#ifndef CALENDARI__CALENDAR_LIST_H
#define CALENDARI__CALENDAR_LIST_H 1

#include <gtk/gtk.h>

namespace calendari {

class Calendar;
class Calendari;
class Occurrence;


struct CalendarList
{
  GtkListStore* liststore_cal; ///< List of calendars
  GtkTreeView*  treeview;

  /** Populate members. */
  void build(Calendari* cal, GtkBuilder* builder);

  /** The order of calendars in the list view has been changed. Change the
  *   model accordingly. */
  void reorder(void);

  Calendar* current(void) const;
  void toggle(gchar* path, calendari::Calendari* cal);
  void add(void);

  /** Synchronise the currently selected calendar with its .ics file.
  *   Readonly calendars are re-read from the .ics file. Other calendars are
  *   written-out to the .ics file. */
  void refresh(calendari::Calendari* cal);
  void refresh_all(calendari::Calendari* cal);
  void select(Occurrence* occ);
};


} // end namespace

#endif // CALENDARI__CALENDAR_LIST_H
