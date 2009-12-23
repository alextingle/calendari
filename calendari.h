#ifndef CALENDARI__CALENDARI_H
#define CALENDARI__CALENDARI_H 1

#include <gtk/gtk.h>

namespace calendari {

class Db;
class View;
class CalendarList;
class DetailView;
class Occurrence;


/** Main application data store. */
struct Calendari
{
  // Settings
  bool        debug;
  Db*         db;

  // Widgets
  GtkWidget*  window;            ///< Application window
  GtkWidget*  main_drawingarea;  ///< Canvas for the main view.
  GtkLabel*   main_label;        ///< Label for the main view.
  GtkDialog*  about;             ///< About box.

  // Subordinate components.
  View*          main_view;
  CalendarList*  calendar_list;
  DetailView*    detail_view;

  /** Load database. */
  void load(const char* dbname);

  /** Populate members. */
  void build(GtkBuilder* builder);

  /** Set the currently selected occurrence. */
  void select(Occurrence* occ);

  /** Get the selected occurrence, if any. */
  Occurrence* selected(void) const
    { return _occurrence; }

  /** An occurrence moved. */
  void moved(Occurrence* occ);

  /** Create a new event - triggered by UI. */
  void create_event(time_t dtstart, time_t dtend);

  /** Erase the selected occurrence, if any. */
  void erase_selected(void);

private:
  Occurrence*    _occurrence;
};


} // end namespace

#endif // CALENDARI__CALENDARI_H
