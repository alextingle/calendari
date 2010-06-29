#include "callback.h"

#include "calendari.h"
#include "calendarlist.h"
#include "detailview.h"
#include "monthview.h"
#include "prefview.h"

#include <cstdio>
#include <gdk/gdkkeysyms.h>


// -- menus --

G_MODULE_EXPORT void
cali_menu_new_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  app->create_calendar();
}


G_MODULE_EXPORT void
cali_menu_refresh_cb(
    GtkMenuItem*,
    calendari::Calendari*  cal
  )
{
  cal->calendar_list->refresh_selected(cal);
}


G_MODULE_EXPORT void
cali_menu_refreshall_cb(
    GtkMenuItem*,
    calendari::Calendari*  cal
  )
{
  cal->calendar_list->refresh_all(cal);
}


G_MODULE_EXPORT void
cali_menu_cut_cb(GtkMenuItem*, calendari::Calendari* app)
{
  if(gtk_widget_has_focus(app->main_drawingarea))
      app->cut_clipboard();
  GtkWidget* widget = app->window;
  while(GTK_IS_CONTAINER(widget))
  {
    widget = gtk_container_get_focus_child(GTK_CONTAINER(widget));
    if(widget)
    {
      if(GTK_IS_TEXT_VIEW(widget))
          g_signal_emit_by_name(widget, "cut-clipboard", NULL);
      else if(GTK_IS_EDITABLE(widget))
          gtk_editable_cut_clipboard(GTK_EDITABLE(widget));
      else
          continue;
    }
    return;
  }
}


G_MODULE_EXPORT void
cali_menu_copy_cb(GtkMenuItem*, calendari::Calendari* app)
{
  if(gtk_widget_has_focus(app->main_drawingarea))
      app->copy_clipboard();
  GtkWidget* widget = app->window;
  while(GTK_IS_CONTAINER(widget))
  {
    widget = gtk_container_get_focus_child(GTK_CONTAINER(widget));
    if(widget)
    {
      if(GTK_IS_TEXT_VIEW(widget))
          g_signal_emit_by_name(widget, "copy-clipboard", NULL);
      else if(GTK_IS_EDITABLE(widget))
          gtk_editable_copy_clipboard(GTK_EDITABLE(widget));
      else
          continue;
    }
    return;
  }
}


G_MODULE_EXPORT void
cali_menu_paste_cb(GtkMenuItem*, calendari::Calendari* app)
{
  // ?? Need to clear clipboard
  if(gtk_widget_has_focus(app->main_drawingarea))
      app->paste_clipboard();
  GtkWidget* widget = app->window;
  while(GTK_IS_CONTAINER(widget))
  {
    widget = gtk_container_get_focus_child(GTK_CONTAINER(widget));
    if(widget)
    {
      if(GTK_IS_TEXT_VIEW(widget))
          g_signal_emit_by_name(widget, "paste-clipboard", NULL);
      else if(GTK_IS_EDITABLE(widget))
          gtk_editable_paste_clipboard(GTK_EDITABLE(widget));
      else
          continue;
    }
    return;
  }
}


G_MODULE_EXPORT void
cali_menu_delete_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  if(gtk_widget_has_focus(app->main_drawingarea))
      app->erase_selected();
  else if(gtk_widget_has_focus(GTK_WIDGET(app->calendar_list->treeview)))
      app->delete_selected_calendar();
}


G_MODULE_EXPORT void
cali_menu_today_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  app->main_view = app->main_view->go_today();
}


G_MODULE_EXPORT void
cali_menu_dialogue_cb(
    // GtkMenuItem* menuitem, // ?? Eliminated by Glade?
    GtkDialog* dialog
  )
{
  int response = gtk_dialog_run(dialog);
  switch(response)
  {
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CANCEL:
      gtk_widget_hide(GTK_WIDGET(dialog));
      break;
    default:
      break;
  }
}


G_MODULE_EXPORT void
cali_dialog_response_cancel_cb(GtkDialog* dialog)
{
  gtk_dialog_response(dialog,GTK_RESPONSE_CANCEL);
}


// -- cali_main_drawingarea --

G_MODULE_EXPORT gboolean
cali_drawingarea_button_press_event_cb(
    GtkWidget*             widget,
    GdkEventButton*        event,
    calendari::Calendari*  cal
  )
{
  if(event->type == GDK_BUTTON_RELEASE)
  {
    cal->main_view->release();
  }
  else
  {
    gtk_widget_grab_focus(widget);
    cal->main_view->click(event->type, event->x, event->y);
  }
  return true;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_motion_notify_event_cb(
    GtkWidget*             widget,
    GdkEventMotion*        event,
    calendari::Calendari*  cal
  )
{
  cal->main_view->motion(widget, event->x, event->y);
  return false;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_leave_notify_event_cb(
    GtkWidget*,
    GdkEventCrossing*,
    calendari::Calendari*  cal
  )
{
  cal->main_view->leave();
  return false;
}


G_MODULE_EXPORT gboolean
cali_drawingarea_expose_event_cb(
    GtkWidget*             widget,
    GdkEventExpose*        event,
    calendari::Calendari*  cal
  )
{
  cal->main_drawingarea_redraw_queued = false;
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
    GtkWidget*,
    GdkEventKey*           event,
    calendari::Calendari*  cal
  )
{
  if(event->type==GDK_KEY_PRESS)
  {
    switch(event->keyval)
    {
      case GDK_Up:
          cal->main_view = cal->main_view->go_up();
          break;
      case GDK_Right:
          cal->main_view = cal->main_view->go_right();
          break;
      case GDK_Down:
          cal->main_view = cal->main_view->go_down();
          break;
      case GDK_Left:
          cal->main_view = cal->main_view->go_left();
          break;
      case GDK_Return:
      case GDK_KP_Enter:
      case GDK_space:
          cal->main_view->ok();
          break;
      case GDK_Escape:
          cal->main_view->cancel();
          break;
      case GDK_Insert:
          cal->main_view->create_event();
          break;
      case GDK_BackSpace:
      case GDK_Delete:
      case GDK_KP_Delete:
          cal->erase_selected();
          break;
      case GDK_Home:
          cal->main_view = cal->main_view->go_today();
          break;
      case GDK_Page_Up:
          cal->main_view = cal->main_view->prev();
          break;
      case GDK_Page_Down:
          cal->main_view = cal->main_view->next();
          break;
      default:
          return false; // Propagate on.
    }
  }
  return true;
}


G_MODULE_EXPORT void
prev_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->prev();
}


G_MODULE_EXPORT void
next_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->next();
}


G_MODULE_EXPORT void
zoom_in_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->zoom_in();
}


G_MODULE_EXPORT void
zoom_out_button_clicked_cb(GtkWidget*, calendari::Calendari* cal)
{
  cal->main_view = cal->main_view->zoom_out();
}

G_MODULE_EXPORT gboolean
cali_drawingarea_drag_drop_cb(
    GtkWidget*      widget,
    GdkDragContext* drag_context,
    gint            x,
    gint            y,
    guint           time,
    calendari::Calendari*  cal
  )
{
  // We are the destination of a drag/drop.
  return cal->main_view->drag_drop(widget,drag_context,x,y,time);
}

G_MODULE_EXPORT void
cali_drawingarea_drag_data_get_cb(
    GtkWidget*        widget,
    GdkDragContext*   drag_context,
    GtkSelectionData* data,
    guint             info,
    guint             time,
    calendari::Calendari*  cal
  )
{
  // We are the source of a drag/drop.
  cal->main_view->drag_data_get(data,info);
}

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
  )
{
  // We are the destination of a drag/drop.
  cal->main_view->drag_data_received(drag_context,x,y,data,info,time);
}


// -- calendar list --

G_MODULE_EXPORT void
calendar_toggle_cb(
    GtkCellRendererToggle*,
    gchar*                  path,
    calendari::Calendari*   cal
  )
{
  cal->calendar_list->toggle(path,cal);
}


G_MODULE_EXPORT void
calendar_deleted_cb(
    GtkTreeModel*,
    GtkTreePath*,
    calendari::Calendari* cal
  )
{
  printf("deleted\n");
  cal->calendar_list->reorder();
}


G_MODULE_EXPORT void
calendar_inserted_cb(
    GtkTreeModel*,
    GtkTreePath*,
    GtkTreeIter*,
    calendari::Calendari*
  )
{
  printf("inserted\n");
}


G_MODULE_EXPORT gboolean
cali_cals_treeview_key_press_event_cb(
    GtkWidget*,
    GdkEventKey*           event,
    calendari::Calendari*  app
  )
{
  if(event->type==GDK_KEY_PRESS)
  {
    switch(event->keyval)
    {
      case GDK_BackSpace:
      case GDK_Delete:
      case GDK_KP_Delete:
          app->delete_selected_calendar();
          break;
      default:
          return false; // Propagate on.
    }
  }
  return true;
}


G_MODULE_EXPORT void
cali_cals_treeview_row_activated_cb(
    GtkTreeView*,
    GtkTreePath*           path,
    GtkTreeViewColumn*,
    calendari::Calendari*  app
  )
{
  app->calendar_list->row_activated(path);
}


G_MODULE_EXPORT gboolean
cal_entry_focus_event_cb(
    GtkWidget*             e,
    GdkEventFocus*,
    calendari::Calendari*  app
  )
{
  app->calendar_list->entry_cb(GTK_ENTRY(e),app);
  return false;
}


G_MODULE_EXPORT void
cal_file_set_cb(
    GtkFileChooserButton*  fc,
    calendari::Calendari*  app
  )
{
  app->calendar_list->file_set_cb(fc,app);
}


G_MODULE_EXPORT void
cal_color_set_cb(
    GtkColorButton*        cb,
    calendari::Calendari*  app
  )
{
  app->calendar_list->color_set_cb(cb,app);
}


// -- detail view --

G_MODULE_EXPORT gboolean
detail_entry_focus_event_cb(
    GtkWidget*             e,
    GdkEventFocus*,
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


G_MODULE_EXPORT gboolean
detail_text_focus_event_cb(
    GtkTextView*           tv,
    GdkEventFocus*,
    calendari::Calendari*  cal
  )
{
  cal->detail_view->textview_cb(tv,cal);
  return false;
}


// -- Preferences dialogue box --

G_MODULE_EXPORT void
prefs_adj_value_changed_cb(
    GtkAdjustment*         adjustment,
    calendari::Calendari*  cal
  )
{
  cal->pref_view->adj_value_changed_cb(adjustment);
}


G_MODULE_EXPORT void
prefs_combobox_changed_cb(GtkComboBox* cb, calendari::Calendari* app)
{
  app->pref_view->combobox_cb(cb);
}
