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

  Calendar* current(void) const;
  void toggle(gchar* path, calendari::Calendari* cal);
  void add(void);
  void refresh(calendari::Calendari* cal);
  void select(Occurrence* occ);
};


} // end namespace

#endif // CALENDARI__CALENDAR_LIST_H
