#ifndef CALENDARI__MONTH_VIEW_H
#define CALENDARI__MONTH_VIEW_H 1

#include "db.h"
#include "view.h"

#include <gtk/gtk.h>
#include <string>

namespace calendari {

class Calendari;


class MonthView: public View
{
public:
  MonthView(Calendari& cal);
  ~MonthView(void);
  virtual void set(time_t self_time);
  virtual void draw(GtkWidget* widget, cairo_t* cr);
  virtual void click(GdkEventType type, double x, double y);
  virtual void motion(GtkWidget* widget, double x, double y);
  virtual void leave(void);
  virtual void release(void);
  virtual bool drag_drop(GtkWidget*,GdkDragContext*,int x,int y,guint time);
  virtual void drag_data_get(GtkSelectionData*,guint info);
  virtual void drag_data_received(GdkDragContext*, int x, int y, GtkSelectionData*,guint info,guint time);
  virtual void select(Occurrence* occ);
  virtual void moved(Occurrence* occ);
  virtual void erase(Occurrence* occ);
  virtual void reload(void);
  virtual void create_event(void);
  virtual void ok(void);
  virtual void cancel(void);
  virtual View* go_today(void);
  virtual View* go_up(void);
  virtual View* go_right(void);
  virtual View* go_down(void);
  virtual View* go_left(void);
  virtual View* prev(void);
  virtual View* next(void);
  virtual void move_here(Occurrence*);
  virtual void copy_here(Occurrence*);
private:
  Calendari& cal;
  // Time
  time_t    now; ///< Current, wall-clock time.
  struct tm self_local; ///< A time somewhere in the current view.
  int       current_cell; ///< The selected cell, or NULL_CELL.
  int       month_cells; ///< Num. cells in the current display (28, 35 or 42).
  static const int MAX_CELLS = 7 * 6 + 1;
  static const int NULL_CELL = -1000;
  Day day[MAX_CELLS];
  std::string dayname[7]; 
  // Dimensions
  double width;
  double height;
  double header_height;
  double cell_width;
  double cell_height;
  double slot_height;
  // Fonts
  PangoFontDescription* head_pfont;
  PangoFontDescription* body_pfont;
  cairo_font_extents_t font_extents;
  // Slots
  size_t slots_per_cell;
  size_t current_slot; ///< The selected slot, or zero.
  // Statusbar
  Occurrence*   statusbar_occ;
  unsigned int  statusbar_ctx_id;
  // Drag & Drop
  double drag_x;
  double drag_y;

  void init_dimensions(GtkWidget* widget, cairo_t* cr);
  void arrange_slots(void);
  void draw_grid(cairo_t* cr);
  void draw_cells(cairo_t* cr);
  void draw_cell(cairo_t* cr, PangoLayout* pl, int cell);
  void draw_occurrence(cairo_t* cr, PangoLayout* pl, int cell, int slot);

  /** Find the cell, slot and (maybe) Occurrence at coordinates x,y.
   *  Returns TRUE if the cell and slot are valid, and FALSE otherwise. */
  bool xy(
      double x, double y,
      int& out_cell, size_t& out_slot, Occurrence*& out_occ
    ) const;
};


} // end namespace calendari

#endif // CALENDARI__MONTH_VIEW_H
