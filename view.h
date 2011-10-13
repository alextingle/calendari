#ifndef CALENDARI__VIEW_H
#define CALENDARI__VIEW_H 1

#include <gtk/gtk.h>
#include <vector>

namespace calendari {


class Occurrence;


struct Day
{
  time_t start;
  int    year;
  int    mon;
  int    mday;
  int    wday;
  std::vector<Occurrence*> occurrence; ///< Occurrences that start on this day.
  std::vector<Occurrence*> slot;       ///< slot# -> occurrence.
};


class View
{
public:
  virtual void set(time_t self_time) =0;
  virtual void draw(GtkWidget* widget, cairo_t* cr) =0;
  virtual void click(GdkEventType type, double x, double y) =0;
  virtual void motion(GtkWidget* widget, double x, double y) =0; ///< Pointer
  virtual void leave(void) =0; ///< Pointer has left the widget.
  virtual void release(void) =0; ///< Mouse button released.
  virtual bool drag_drop(GtkWidget*,GdkDragContext*,int x,int y,guint time) =0;
  virtual void drag_data_get(GtkSelectionData*,guint info) =0;
  virtual void drag_data_received(GdkDragContext*, int x, int y, GtkSelectionData*,guint info,guint time) =0;
  virtual void select(Occurrence* occ) =0;
  virtual void moved(Occurrence* occ) =0;
  virtual void erase(Occurrence* occ) =0;
  virtual void reload(void) =0;
  virtual void create_event(void) =0;
  virtual void ok(void) {} ///< Called when user hits ENTER
  virtual void cancel(void) {} ///< Called when user hits ESC
  virtual View* go_today(void) { return this; }
  virtual View* go_up(void)    { return this; }
  virtual View* go_right(void) { return this; }
  virtual View* go_down(void)  { return this; }
  virtual View* go_left(void)  { return this; }
  virtual View* prev(void)     { return this; }
  virtual View* next(void)     { return this; }
  virtual View* zoom_in(void)  { return this; }
  virtual View* zoom_out(void) { return this; }
  virtual void move_here(Occurrence*) =0; ///< Moves Occurrence to current posn.
  virtual void copy_here(Occurrence*) =0; ///< Copies Occurrence to current posn
};


} // end namespace calendari

#endif // CALENDARI__VIEW_H
