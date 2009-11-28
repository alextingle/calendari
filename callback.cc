#include "callback.h"

#include "calendari.h"
#include "calendarlist.h"

#include <cstdio>

G_MODULE_EXPORT void
cali_cb_clicked(
    GtkButton *button,
    gpointer   data )
{
    printf("Hello world.\n");
}


G_MODULE_EXPORT gboolean
cali_drawingarea_button_press_event_cb(
    GtkWidget*             widget,
    GdkEventButton*        event,
    calendari::Calendari*  cal
  )
{
  printf("Click.\n");
  cal->calendar_list->add();
  return true;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_expose_event_cb(
    GtkWidget*       widget,
    GdkEventExpose*  event,
    calendari::Calendari*        data
  )
{
  printf("Hello world.\n");
  return true;
}
