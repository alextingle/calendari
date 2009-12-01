#include "callback.h"

#include "calendari.h"
#include "calendarlist.h"
#include "monthview.h"

#include <cstdio>

/*??
G_MODULE_EXPORT void
cali_cb_clicked(
    GtkButton *button,
    gpointer   data )
{
    printf("Hello world.\n");
}
??*/


G_MODULE_EXPORT gboolean
cali_drawingarea_button_press_event_cb(
    GtkWidget*             widget,
    GdkEventButton*        event,
    calendari::Calendari*  cal
  )
{
  cal->main_view->click(event->x,event->y);
  return true;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_expose_event_cb(
    GtkWidget*             widget,
    GdkEventExpose*        event,
    calendari::Calendari*  cal
  )
{
  cairo_t* cr;

  // get a cairo_t
  cr = gdk_cairo_create(widget->window);

  // set a clip region for the expose event
  cairo_rectangle(cr,
      event->area.x,     event->area.y,
      event->area.width, event->area.height
    );
  cairo_clip(cr);

  cal->main_view->draw(widget,cr);

  cairo_destroy (cr);
  return false;
}

G_MODULE_EXPORT gboolean
prev_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->prev();
}


G_MODULE_EXPORT gboolean
next_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->next();
}


G_MODULE_EXPORT gboolean
zoom_in_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->zoom_in();
}

G_MODULE_EXPORT gboolean
zoom_out_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->zoom_out();
}

G_MODULE_EXPORT void
calendar_toggle_cb(
    GtkCellRendererToggle*  cell_renderer,
    gchar*                  path,
    calendari::Calendari*   cal
  )
{
  cal->calendar_list->toggle(path,cal);
}
