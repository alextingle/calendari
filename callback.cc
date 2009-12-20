#include "callback.h"

#include "calendari.h"
#include "calendarlist.h"
#include "detailview.h"
#include "monthview.h"

#include <cstdio>
#include <gdk/gdkkeysyms.h>


// -- menus --

G_MODULE_EXPORT void
cali_menu_refresh_cb(
    GtkMenuItem*           menuitem,
    calendari::Calendari*  cal
  )
{
  cal->calendar_list->refresh(cal);
}


G_MODULE_EXPORT void
cali_menu_delete_cb(
    GtkMenuItem*           menuitem,
    calendari::Calendari*  cal
  )
{
  if(gtk_widget_has_focus(cal->main_drawingarea))
      cal->erase_selected();
}


// -- cali_main_drawingarea --

G_MODULE_EXPORT gboolean
cali_drawingarea_button_press_event_cb(
    GtkWidget*             widget,
    GdkEventButton*        event,
    calendari::Calendari*  cal
  )
{
  gtk_widget_grab_focus(widget);
  cal->main_view->click(event->type, event->x, event->y);
  return true;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_expose_event_cb(
    GtkWidget*             widget,
    GdkEventExpose*        event,
    calendari::Calendari*  cal
  )
{
  cairo_t* cr = gdk_cairo_create(widget->window);
  // set a clip region for the expose event
  cairo_rectangle(cr,
      event->area.x,     event->area.y,
      event->area.width, event->area.height
    );
  cairo_clip(cr);
  cal->main_view->draw(widget,cr);
  cairo_destroy(cr);
  return false;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_key_press_event_cb(
    GtkWidget*             widget,
    GdkEventKey*           event,
    calendari::Calendari*  cal
  )
{
  if(event->type==GDK_KEY_PRESS)
  {
    switch(event->keyval)
    {
      case GDK_Left:
          printf("Left\n");
          break;
      case GDK_Up:
          printf("Up\n");
          break;
      case GDK_Right:
          printf("Right\n");
          break;
      case GDK_Down:
          printf("Down\n");
          break;
      case GDK_BackSpace:
      case GDK_Delete:
      case GDK_KP_Delete:
          cal->erase_selected();
          break;
      case GDK_Page_Up:
          cal->main_view = cal->main_view->prev();
          break;
      case GDK_Page_Down:
          cal->main_view = cal->main_view->next();
          break;
      default:
          printf("Other\n");
          break;
    }
  }
  return true;
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


// -- calendar list --

G_MODULE_EXPORT void
calendar_toggle_cb(
    GtkCellRendererToggle*  cell_renderer,
    gchar*                  path,
    calendari::Calendari*   cal
  )
{
  cal->calendar_list->toggle(path,cal);
}


// -- detail view --

G_MODULE_EXPORT gboolean
detail_entry_focus_event_cb(
    GtkWidget*             e,
    GdkEventFocus*         event,
    calendari::Calendari*  cal
  )
{
  cal->detail_view->entry_cb(GTK_ENTRY(e),cal);
  return false;
}


G_MODULE_EXPORT void
detail_entry_done_event_cb(GtkCellEditable* e, calendari::Calendari* cal)
{
  // ?? NOT CURRENTLY USED
  cal->detail_view->entry_cb(GTK_ENTRY(e),cal);
}


G_MODULE_EXPORT void
detail_combobox_changed_cb(GtkComboBox* cb, calendari::Calendari* cal)
{
  cal->detail_view->combobox_cb(cb,cal);
}
