#include "monthview.h"

#include "calendari.h"
#include "setting.h"
#include "util.h"

#include <iostream>

namespace calendari {


MonthView::MonthView(Calendari& c): cal(c)
{
  head_pfont = pango_font_description_new();
  pango_font_description_set_absolute_size(
      head_pfont,
      Setting::head_font_size*PANGO_SCALE
    );
  pango_font_description_set_family_static(head_pfont,"sans");

  body_pfont = pango_font_description_new();
  pango_font_description_set_absolute_size(
      body_pfont,
      Setting::body_font_size*PANGO_SCALE
    );
  pango_font_description_set_family_static(body_pfont,"sans");
}


MonthView::~MonthView(void)
{
  pango_font_description_free(body_pfont);
}


void
MonthView::set(time_t now_time)
{
  localtime_r(&now_time,&now_local);
  
  now_local.tm_hour = 0;
  now_local.tm_min  = 0;
  now_local.tm_sec  = 0;

  // Find the start of the month.
  struct tm i = now_local;
  i.tm_mday = 1;
  normalise_local_tm(i);

  char buf[256];
  ::strftime(buf, sizeof(buf), "%B %Y", &i);
  gtk_label_set_text(cal.main_label,buf);

  // Wind back to the first day of the week.
  i.tm_mday -= i.tm_wday;

  // Loop forward through days.
  for(int cell=0; cell<MAX_CELLS; ++cell)
  {
    time_t itime = normalise_local_tm(i);
    day[cell].start = itime;
    day[cell].mon   = i.tm_mon;
    day[cell].mday  = i.tm_mday;
    day[cell].wday  = i.tm_wday;
    day[cell].occurrence.clear();
    // Set-up weekday names.
    if(cell<7)
    {
      ::strftime(buf, sizeof(buf), "%A", &i);
      dayname[cell] = buf;
    }
    ++i.tm_mday;
  }

  // load events for this time period.
  std::multimap<time_t,Occurrence*> all =
      cal.db->find( day[0].start, normalise_local_tm(i) );

  for(int cell=0; cell<MAX_CELLS; ++cell)
  {
    typedef std::multimap<time_t,Occurrence*>::const_iterator OIt;
    OIt lb = all.lower_bound(day[cell].start);
    OIt ub = all.upper_bound(day[cell].start + 86400 );//??bs
    for(; lb!=ub; ++lb)
        day[cell].occurrence.push_back(lb->second);
  }
}


void
MonthView::draw(GtkWidget* widget, cairo_t* cr)
{
  cairo_save(cr);

  // Initialise the widget's dimensions.
  init_dimensions(widget,cr);

  // Clear the surface
  cairo_set_source_rgb(cr, 1,1,1);
  cairo_paint(cr);

  draw_cells(cr);
  draw_grid(cr);

  cairo_restore(cr);
}


void
MonthView::click(double x, double y)
{
  if(x<0.0 || x>=width)
      return;
  if(y<header_height || y>=height)
      return;
  int cell = int((y-header_height)/cell_height) * 7 + int(x/cell_width);
  if(cell<0 || cell>=MAX_CELLS)
      return;
      
  if(day[cell].occurrence.empty())
  {
    std::cout<<cell<<std::endl;
  }
  else
  {
    std::cout<<cell<<" "<<day[cell].occurrence[0]->event.summary<<std::endl;
    cal.select( day[cell].occurrence[0] );
  }
}


View*
MonthView::prev(void)
{
  --now_local.tm_mon;
  set( normalise_local_tm(now_local) );
  gtk_widget_queue_draw(GTK_WIDGET(cal.main_drawingarea));
  return this;
}


View*
MonthView::next(void)
{
  ++now_local.tm_mon;
  set( normalise_local_tm(now_local) );
  gtk_widget_queue_draw(GTK_WIDGET(cal.main_drawingarea));
  return this;
}


void
MonthView::init_dimensions(GtkWidget* widget, cairo_t* cr)
{
  static const double hmargin = 0.0;
  static const double vmargin = 0.0;

  // Find dimensions
  GtkAllocation& alc(widget->allocation);
  width = alc.width - hmargin * 2.0;
  height = alc.height - vmargin * 2.0;
  header_height = Setting::head_font_size * 2.0;
  cairo_translate(cr,
      (alc.width - width) / 2.0,
      (alc.height - height) / 2.0
    );
  cell_width = width / 7.0;
  cell_height = (height - header_height) / 5.0;
}


void
MonthView::draw_grid(cairo_t* cr)
{
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
  cairo_set_line_width(cr, 0.2 * cairo_get_line_width (cr));

  // Horizontal lines.
  cairo_move_to(cr, 0,0);
  cairo_line_to(cr, width,0);
  for(int i=0; i<6; ++i)
  {
    cairo_move_to(cr, 0, header_height + i * cell_height);
    cairo_line_to(cr, width, header_height + i * cell_height);
  }

  // Vertical lines.
  for(int i=0; i<8; ++i)
  {
    cairo_move_to(cr, 0 + i * cell_width, header_height);
    cairo_line_to(cr, 0 + i * cell_width, height);
  }
  cairo_stroke(cr);
  cairo_restore(cr);

  // Day-names.
  cairo_set_source_rgb(cr, 0,0,0);

  PangoLayout* pl = pango_cairo_create_layout(cr);
  pango_layout_set_font_description(pl,head_pfont);
  pango_layout_set_alignment(pl,PANGO_ALIGN_CENTER);
  pango_layout_set_ellipsize(pl,PANGO_ELLIPSIZE_END);
  pango_layout_set_wrap(pl,PANGO_WRAP_WORD_CHAR);
  pango_layout_set_width(pl,cell_width * PANGO_SCALE);
  pango_layout_set_height(pl,Setting::head_font_size*PANGO_SCALE);


//  weekdays.set_total_width(width*0.8);
//  weekdays.set_font_size(cr);
  for(int i=0; i<7; ++i)
  {
    cairo_move_to(cr, i * cell_width, header_height * 0.25);
    pango_layout_set_text(pl,dayname[i].c_str(),dayname[i].size());
    pango_cairo_show_layout(cr,pl);
//    weekdays.show(cr, i);
  }
}


void
MonthView::draw_cells(cairo_t* cr)
{
  cairo_save(cr);

  cairo_translate(cr,0,header_height);
  cairo_set_font_size(cr,10.0);
  cairo_font_extents(cr,&font_extents);

  PangoLayout* pl = pango_cairo_create_layout(cr);
  pango_layout_set_font_description(pl,body_pfont);
  pango_layout_set_ellipsize(pl,PANGO_ELLIPSIZE_END);
  pango_layout_set_wrap(pl,PANGO_WRAP_WORD_CHAR);
  pango_layout_set_width(pl,cell_width * PANGO_SCALE);
  double pl_height = std::max(1.0,cell_height-font_extents.height);
  pango_layout_set_height(pl,pl_height * PANGO_SCALE);

  for(int cell=0; cell<MAX_CELLS; ++cell)
      draw_cell(cr,pl,cell);

  g_object_unref(pl);
  cairo_restore(cr);
}


void
MonthView::draw_cell(cairo_t* cr, PangoLayout* pl, int cell)
{
  double cellx = (cell%7) * cell_width;
  double celly = (cell/7) * cell_height;

  cairo_set_source_rgb(cr, 0.95,0.95,0.95);
  if(now_local.tm_mon != day[cell].mon)
  {
    // Colour in this day, because it's not in this month.
    cairo_rectangle (cr, cellx,celly, cell_width,cell_height);
    cairo_fill(cr);
  }
  
  // Write in the day number.
  cairo_set_source_rgb(cr, 0.2,0.2,0.2);
  cairo_text_extents_t extents;
  char buf[32];
  snprintf(buf,sizeof(buf),"%i ",day[cell].mday);
  cairo_text_extents(cr, buf, &extents);
 
  cairo_move_to(cr,
      cellx + cell_width - extents.x_advance,
      celly + font_extents.height
    );
  cairo_show_text(cr,buf);

  std::string pango_text = "";
  for(int line=0; line<day[cell].occurrence.size(); ++line)
      pango_text += day[cell].occurrence[line]->event.summary + "\n";

  // pW / PANGO_SCALE == cW

  cairo_move_to(cr, cellx, celly + font_extents.height);
  pango_layout_set_text(pl,pango_text.c_str(),pango_text.size());
  pango_cairo_show_layout(cr,pl);
}


} // end namespace calendari
