#ifndef CALENDARI__MONTH_VIEW_H
#define CALENDARI__MONTH_VIEW_H 1

#include "db.h"

#include <gtk/gtk.h>
#include <string>
#include <vector>

namespace calendari {

class Calendari;


struct Day
{
  time_t start;
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
  virtual void click(double x, double y) =0;
  virtual void select(Occurrence* occ) =0;
  virtual View* prev(void)     { return this; }
  virtual View* next(void)     { return this; }
  virtual View* zoom_in(void)  { return this; }
  virtual View* zoom_out(void) { return this; }
};


class MonthView: public View
{
public:
  MonthView(Calendari& cal);
  ~MonthView(void);
  virtual void set(time_t self_time);
  virtual void draw(GtkWidget* widget, cairo_t* cr);
  virtual void click(double x, double y);
  virtual void select(Occurrence* occ);
  virtual View* prev(void);
  virtual View* next(void);
private:
  Calendari& cal;
  // Time
  time_t now;
  struct tm self_local;
  static const size_t MAX_CELLS = 7 * 5;
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

  void init_dimensions(GtkWidget* widget, cairo_t* cr);
  void arrange_slots(void);
  void draw_grid(cairo_t* cr);
  void draw_cells(cairo_t* cr);
  void draw_cell(cairo_t* cr, PangoLayout* pl, int cell);
};


} // end namespace calendari

#endif // CALENDARI__MONTH_VIEW_H
