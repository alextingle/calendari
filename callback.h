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
  cali_menu_new_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  app
    );

  G_MODULE_EXPORT void
  cali_menu_subscribe_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  app
    );

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
  cali_menu_cut_cb(GtkMenuItem*, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  cali_menu_copy_cb(GtkMenuItem*, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  cali_menu_paste_cb(GtkMenuItem*, calendari::Calendari* cal);

  G_MODULE_EXPORT void
  cali_menu_delete_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_menu_today_cb(
      GtkMenuItem*           menuitem,
      calendari::Calendari*  cal
    );

  /** General callback that summons an arbitrary dialogue. */
  G_MODULE_EXPORT void
  cali_menu_dialogue_cb(
      // GtkMenuItem*, // ?? Eliminated by Glade?
      GtkDialog* dialog
    );

  G_MODULE_EXPORT void
  cali_dialog_response_cancel_cb(GtkDialog* dialog);

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

  G_MODULE_EXPORT gboolean
  cali_drawingarea_drag_drop_cb(
      GtkWidget*      widget,
      GdkDragContext* drag_context,
      gint            x,
      gint            y,
      guint           time,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_drawingarea_drag_data_get_cb(
      GtkWidget*        widget,
      GdkDragContext*   drag_context,
      GtkSelectionData* data,
      guint             info,
      guint             time,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_drawingarea_drag_data_received_cb(
      GtkWidget*        widget,
      GdkDragContext*   drag_context,
      gint              x,
      gint              y,
      GtkSelectionData* data,
      guint             info,
      guint             time,
      calendari::Calendari*  cal
    );

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

  G_MODULE_EXPORT gboolean
  cali_cals_treeview_key_press_event_cb(
      GtkWidget*             widget,
      GdkEventKey*           event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cali_cals_treeview_row_activated_cb(
      GtkTreeView*       tree_view,
      GtkTreePath*       path,
      GtkTreeViewColumn* column,
      calendari::Calendari*
    );

  G_MODULE_EXPORT gboolean
  cal_entry_focus_event_cb(
      GtkWidget*             e,
      GdkEventFocus*         event,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  cal_file_set_cb(
      GtkFileChooserButton*  fc,
      calendari::Calendari*  app
    );

  G_MODULE_EXPORT void
  cal_color_set_cb(
      GtkColorButton*        cb,
      calendari::Calendari*  app
    );

  G_MODULE_EXPORT void
  cal_pub_radiobutton_toggled_cb(
      GtkToggleButton*       tb,
      calendari::Calendari*  app
    );

  G_MODULE_EXPORT void
  cal_button_clicked_cb(
      GtkButton*             b,
      calendari::Calendari*  app
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

  // -- Preferences dialogue box --

  G_MODULE_EXPORT void
  prefs_adj_value_changed_cb(
      GtkAdjustment*         adjustment,
      calendari::Calendari*  cal
    );

  G_MODULE_EXPORT void
  prefs_combobox_changed_cb(GtkComboBox* cb, calendari::Calendari* app);

} // end extern "C"

#endif // CALENDARI__CALLBACK_H
