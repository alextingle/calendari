#ifndef CALENDARI__CALLBACK_H
#define CALENDARI__CALLBACK_H 1

#include <gtk/gtk.h>
#include <gmodule.h>

namespace calendari {
  class Calendari;
}

extern "C"
{

  // -- menus --

  G_MODULE_EXPORT void
  cali_menu_refresh_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_menu_refreshall_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_menu_delete_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_menu_about_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  cal
    );

  // -- cali_main_drawingarea --

  G_MODULE_EXPORT gboolean
  cali_drawingarea_button_press_event_cb(
      GtkWidget*             widget,
      GdkEventButton*        event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT gboolean
  cali_drawingarea_motion_notify_event_cb(
      GtkWidget*             widget,
      GdkEventMotion*        event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT gboolean
  cali_drawingarea_leave_notify_event_cb(
      GtkWidget*             widget,
      GdkEventCrossing*      event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT gboolean
  cali_drawingarea_expose_event_cb(
      GtkWidget*             widget,
      GdkEventExpose*        event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT gboolean
  cali_drawingarea_key_press_event_cb(
      GtkWidget*             widget,
      GdkEventKey*           event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  prev_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  next_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  zoom_in_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  zoom_out_button_clicked_cb(GtkWidget* widget, calendari::Calendari* cal);

  // -- calendar list --

  G_MODULE_EXPORT void
  calendar_toggle_cb(
      GtkCellRendererToggle*  cell_renderer,
      gchar*                  path,
      calendari::Calendari*   cal
    );

  G_MODULE_EXPORT void
  calendar_deleted_cb(
      GtkTreeModel*          tree_model,
      GtkTreePath*           path,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  calendar_inserted_cb(
      GtkTreeModel*          tree_model,
      GtkTreePath*           path,
      GtkTreeIter*           iter,
      calendari::Calendari*  cal
    );

  // -- detail view --

  G_MODULE_EXPORT gboolean
  detail_entry_focus_event_cb(
      GtkWidget*             e,
      GdkEventFocus*         event,
      calendari::Calendari*  cal
    );

  /** ?? NOT CURRENTLY USED */
  G_MODULE_EXPORT void
  detail_entry_done_event_cb(GtkCellEditable* e, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  detail_combobox_changed_cb(GtkComboBox* cb, calendari::Calendari* cal);

  G_MODULE_EXPORT gboolean
  detail_text_focus_event_cb(GtkTextView*,GdkEventFocus*,calendari::Calendari*);


} // end extern "C"

#endif // CALENDARI__CALLBACK_H
