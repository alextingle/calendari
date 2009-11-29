#ifndef CALENDARI__CALLBACK_H
#define CALENDARI__CALLBACK_H 1

#include <gtk/gtk.h>
#include <gmodule.h>

namespace calendari {
  class Calendari;
}

extern "C"
{

  G_MODULE_EXPORT gboolean
  cali_drawingarea_button_press_event_cb(
      GtkWidget*             widget,
      GdkEventButton*        event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT gboolean
  cali_drawingarea_expose_event_cb(
      GtkWidget*             widget,
      GdkEventExpose*        event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT gboolean
  prev_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  G_MODULE_EXPORT gboolean
  next_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  G_MODULE_EXPORT gboolean
  zoom_in_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  G_MODULE_EXPORT gboolean
  zoom_out_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

} // end extern "C"

#endif // CALENDARI__CALLBACK_H
