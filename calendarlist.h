#ifndef CALENDARI__CALENDAR_LIST_H
#define CALENDARI__CALENDAR_LIST_H 1

#include <gtk/gtk.h>
#include <set>

namespace calendari {

class Calendar;
class Calendari;
class Occurrence;


struct CalendarList
{
  /** Periodically trigger a call to refresh_all(). */
  static bool timeout_refresh_all(void*);

  /** Refreshes the next calendar in the refresh_queue. Returns TRUE if there
  *   are more refreshes queued. */
  static bool idle_refresh_next(void*);

  GtkListStore* liststore_cal; ///< List of calendars
  GtkTreeView*  treeview;

  /** Populate members. */
  void build(Calendari* app, GtkBuilder* builder);

  /** The order of calendars in the list view has been changed. Change the
  *   model accordingly. */
  void reorder(void);

  /** The user has double-clicked on a row - show the cal_dialog. */
  void row_activated(GtkTreePath* path);

  Calendar* iter2cal(GtkTreeIter& iter) const;
  bool get_selected_iter(GtkTreeIter& iter) const;
  Calendar* current(void) const;
  void toggle(gchar* path, Calendari* app);
  void add(void);

  /** Synchronise the calendar with its .ics file.
  *   Readonly calendars are re-read from the .ics file. Other calendars are
  *   written-out to the .ics file. */
  void refresh(Calendari* app, Calendar* calendar);

  /** Synchronise the currently selected calendar with its .ics file.
  *   Readonly calendars are re-read from the .ics file. Other calendars are
  *   written-out to the .ics file. */
  void refresh_selected(Calendari* app);
  void refresh_all(Calendari* app);
  bool refresh_next(Calendari* app);
  void select(Occurrence* occ);

  // -- cal_dialog --

  GtkDialog*    cal_dialog;
  GtkEntry*     name_entry;

  void entry_cb(GtkEntry* entry, calendari::Calendari* app);

private:
  /** Queue of calendars (CALNUMs) to refresh. */
  std::set<int> refresh_queue;
};


} // end namespace

#endif // CALENDARI__CALENDAR_LIST_H
