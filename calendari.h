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
  Db*         db;

  // Widgets
  GtkWidget*  window;            ///< Application window
  GtkWidget*  main_drawingarea;  ///< Canvas for the main view.
  GtkLabel*   main_label;        ///< Label for the main view.

  // Subordinate components.
  View*          main_view;
  CalendarList*  calendar_list;
  DetailView*    detail_view;
  Occurrence*    occurrence;

  /** Load database. */
  void load(const char* dbname);

  /** Populate members. */
  void build(GtkBuilder* builder);

  /** Set the currently selected occurrence. */
  void select(Occurrence* occ);
};


} // end namespace

#endif // CALENDARI__CALENDARI_H
