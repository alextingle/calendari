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
      GtkWidget*       widget,
      GdkEventButton*  event,
      calendari::Calendari*       data
    );

  G_MODULE_EXPORT gboolean
  cali_drawingarea_expose_event_cb(
      GtkWidget*       widget,
      GdkEventExpose*  event,
      calendari::Calendari*        data
    );

} // end extern "C"

#endif // CALENDARI__CALLBACK_H
