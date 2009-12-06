#include "monthview.h"

#include "calendari.h"
#include "setting.h"
#include "util.h"

#include <cassert>
#include <iostream>
#include <algorithm>

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

  slot_height = Setting::body_font_size * 1.4;
}


MonthView::~MonthView(void)
{
  pango_font_description_free(body_pfont);
}


void
MonthView::set(time_t self_time)
{
  localtime_r(&self_time,&self_local);
  
  self_local.tm_hour = 0;
  self_local.tm_min  = 0;
  self_local.tm_sec  = 0;

  // Find the start of the month.
  struct tm i = self_local;
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

  typedef std::multimap<time_t,Occurrence*>::const_reverse_iterator OIt;
  OIt o = all.rbegin();
  for(size_t c=0; c<MAX_CELLS; ++c)
  {
    size_t cell = MAX_CELLS-1-c;
    while(o!=all.rend() && (cell==0 || o->first >= day[cell].start) )
    {
      day[cell].occurrence.push_back(o->second);
      ++o;
    }
    std::reverse(day[cell].occurrence.begin(),day[cell].occurrence.end());
  }
  assert(o==all.rend());
}


void
MonthView::draw(GtkWidget* widget, cairo_t* cr)
{
  now = ::time(NULL);
  cairo_save(cr);

  // Initialise the widget's dimensions.
  init_dimensions(widget,cr);

  // Clear the surface
  cairo_set_source_rgb(cr, 1,1,1);
  cairo_paint(cr);

  arrange_slots();

  draw_cells(cr);
  draw_grid(cr);

  cairo_restore(cr);
}


void
MonthView::click(GdkEventType type, double x, double y)
{
  if(x<0.0 || x>=width)
      return;
  if(y<header_height || y>=height)
      return;
  
  int row = int((y-header_height)/cell_height);
  int cell = row * 7 + int(x/cell_width);
  if(cell<0 || cell>=MAX_CELLS)
      return;
  size_t slot = int(y - header_height - cell_height*row) / slot_height;

  Occurrence* occ = NULL;
  if(slot>=day[cell].slot.size() || !day[cell].slot[slot])
  {
    std::cout<<cell<<std::endl;
  }
  else
  {
    occ = day[cell].slot[slot];
    std::cout<<cell<<" "<<occ->event.summary<<std::endl;
  }
  switch(type)
  {
    case GDK_2BUTTON_PRESS:
        if(!occ)
        {
          tm slot_tm;
          localtime_r(&day[cell].start,&slot_tm);
          slot_tm.tm_sec  = 0;
          slot_tm.tm_min  = 0;
          now = ::time(NULL);
          tm now_tm;
          localtime_r(&now,&now_tm);
          slot_tm.tm_hour = now_tm.tm_hour;
          time_t dtstart = ::mktime(&slot_tm);
          cal.create_event( occ, dtstart, dtstart+3600 );
        }
        break;
    case GDK_BUTTON_PRESS:
        if(occ != cal.occurrence)
        {
          cal.select( occ );
          gtk_widget_queue_draw(GTK_WIDGET(cal.main_drawingarea));
        }
        break;
    default:
        break;
  }
}


void
MonthView::select(Occurrence* occ)
{}


void
MonthView::moved(Occurrence* occ)
{
  Occurrence* add = occ;
  Occurrence* del = occ;
  typedef std::vector<Occurrence*> OV;
  for(size_t c=0; c<MAX_CELLS && (add || del); ++c)
  {
    size_t cell = MAX_CELLS-1-c;
    OV& ov( day[cell].occurrence );
    for(OV::iterator o=ov.begin(); del && o!=ov.end(); ++o)
    {
      if(*o == del)
      {
        ov.erase( o );
        del = NULL;
      }
    }
    if(add && add->dtstart >= day[cell].start)
    {
      // insert
      OV::iterator o=ov.end();
      while(add && o!=ov.begin())
      {
        --o;
        if(add->dtstart >= (**o).dtstart)
        {
          ++o;
          ov.insert(o,add);
          add = NULL;
        }
      }
      if(add)
      {
        ov.insert(ov.begin(),add);
        add = NULL;
      }
    }
  }
  if(!add || !del) // Something was changed
      gtk_widget_queue_draw(GTK_WIDGET(cal.main_drawingarea));
}


void
MonthView::erase(Occurrence* occ)
{
  Occurrence* del = occ;
  typedef std::vector<Occurrence*> OV;
  for(size_t c=0; c<MAX_CELLS; ++c)
  {
    size_t cell = MAX_CELLS-1-c;
    OV& ov( day[cell].occurrence );
    for(OV::iterator o=ov.begin(); o!=ov.end(); ++o)
    {
      if(*o == del)
      {
        ov.erase( o );
        gtk_widget_queue_draw(GTK_WIDGET(cal.main_drawingarea));
        return;
      }
    }
  }
}


View*
MonthView::prev(void)
{
  --self_local.tm_mon;
  set( normalise_local_tm(self_local) );
  gtk_widget_queue_draw(GTK_WIDGET(cal.main_drawingarea));
  return this;
}


View*
MonthView::next(void)
{
  ++self_local.tm_mon;
  set( normalise_local_tm(self_local) );
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
  slots_per_cell = cell_height / slot_height; // rounds down.
}


void
MonthView::arrange_slots(void)
{
  typedef std::vector<Occurrence*> OV;
  for(size_t cell=0; cell<MAX_CELLS; ++cell)
  {
    day[cell].slot.clear();
    day[cell].slot.resize(slots_per_cell,NULL);
  }
  for(size_t cell=0; cell<MAX_CELLS; ++cell)
  {
    size_t next_slot = 1;
    Day& d( day[cell] );
    for(OV::const_iterator i=d.occurrence.begin(); i!=d.occurrence.end(); ++i)
    {
      Occurrence& occ( **i );
      if(!occ.event.calendar().show)
        continue;
      while(next_slot<slots_per_cell && d.slot[next_slot])
          next_slot++;
      if(next_slot>=slots_per_cell)
      {
        // ?? Mark cell as full.
        // ?? Mark any all-day events on future cells.
        break;
      }
      d.slot[next_slot] = &occ;
      if(occ.event.all_day)
      {
        size_t future_cell = cell+1;
        while(future_cell<MAX_CELLS && occ.dtend>day[future_cell].start)
        {
          day[future_cell].slot[next_slot] = &occ;
          future_cell++;
        }
      }
    }
  }
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

  for(int i=0; i<7; ++i)
  {
    cairo_move_to(cr, i * cell_width, header_height * 0.25);
    pango_layout_set_text(pl,dayname[i].c_str(),dayname[i].size());
    pango_cairo_show_layout(cr,pl);
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
  double pl_height = std::max(1.0,/* ??cell_height-*/font_extents.height);
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
  if(self_local.tm_mon != day[cell].mon)
  {
    // Colour in this day, because it's not in this month.
    cairo_rectangle (cr, cellx,celly, cell_width,cell_height);
    cairo_fill(cr);
  }

  if(now>=day[cell].start && now<day[cell].start+86400) //??bs
  {
    // Mark the current day.
    cairo_set_source_rgb(cr, 1,0,0);
    cairo_move_to(cr,cellx,celly);
    cairo_line_to(cr,cellx+slot_height,celly);
    cairo_line_to(cr,cellx,celly+slot_height);
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

  for(size_t s=1; s<slots_per_cell; ++s)
  {
    if(day[cell].slot[s])
    {
      Occurrence& occ = *day[cell].slot[s];
      GdkColor col;
      gdk_color_parse(occ.event.calendar().colour.c_str(),&col);
      gdk_cairo_set_source_color(cr,&col);

      if(&occ == cal.occurrence)
      {
        // Selected - fill slot.
        cairo_rectangle(cr,
            cellx, celly + s * slot_height,
            cell_width, slot_height
          );
        cairo_fill(cr);
        cairo_set_source_rgb(cr,1,1,1);
      }
      pango_layout_set_height(pl,slot_height*PANGO_SCALE);
      std::string pango_text = (occ.event.all_day? "*": "") + occ.event.summary;
      cairo_move_to(cr, cellx, celly + s * slot_height);
      pango_layout_set_text(pl,pango_text.c_str(),pango_text.size());
      pango_cairo_show_layout(cr,pl);
    }
  }
}


} // end namespace calendari
