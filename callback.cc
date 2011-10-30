#include "callback.h"

#include "caldialog.h"
#include "calendari.h"
#include "calendarlist.h"
#include "detailview.h"
#include "monthview.h"
#include "prefview.h"
#include "setting.h"

#include <cstdio>
#include <gdk/gdkkeysyms.h>


// -- menus --

G_MODULE_EXPORT void
cali_menu_new_event_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  app->main_view->create_event();
}


G_MODULE_EXPORT void
cali_menu_new_cal_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  app->create_calendar();
}


G_MODULE_EXPORT void
cali_menu_subscribe_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  app->import_calendar(true);
}


G_MODULE_EXPORT void
cali_menu_import_cb(
    GtkMenuItem*,
    calendari::Calendari*  app
  )
{
  app->import_calendar(false);
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
cali_menu_view_cals_cb(
    GtkCheckMenuItem*      checkmenuitem,
    calendari::Calendari*  app
  )
{
  assert(checkmenuitem == app->view_cals_menuitem);
  bool val = gtk_check_menu_item_get_active(app->view_cals_menuitem);
  if(val == app->setting->view_calendars())
      return;
  app->setting->set_view_calendars(val);
  gtk_paned_set_position(
      GTK_PANED(app->sidebar_vpaned),
      (val? app->setting->cal_vpaned_pos(): 0 )
    );
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


// -- structural --

G_MODULE_EXPORT void
cali_paned_notify_cb(
    GObject*               obj,
    GParamSpec*         /* pspec */,
    calendari::Calendari*  app
  )
{
  int newpos = gtk_paned_get_position(GTK_PANED(obj));
  app->setting->set_cal_vpaned_pos(newpos);
  // Make "view_calendars" consistent.
  bool view_calendars = app->setting->view_calendars();
  if( (newpos>0) != view_calendars )
  {
    app->setting->set_view_calendars(!view_calendars);
    gtk_check_menu_item_set_active(app->view_cals_menuitem,!view_calendars);
  }
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
cali_drawingarea_scroll_event_cb(
    GtkWidget*,
    GdkEventScroll*        event,
    calendari::Calendari*  cal
  )
{
  switch(event->direction)
  {
    case GDK_SCROLL_UP:
          cal->main_view = cal->main_view->go_up();
          break;
    case GDK_SCROLL_DOWN:
          cal->main_view = cal->main_view->go_down();
          break;
    case GDK_SCROLL_LEFT:
          cal->main_view = cal->main_view->go_left();
          break;
    case GDK_SCROLL_RIGHT:
          cal->main_view = cal->main_view->go_right();
          break;
    default:
          return false; // Propagate on.
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
    GtkWidget*,
    GdkDragContext* /*drag_context*/,
    GtkSelectionData* data,
    guint             info,
    guint           /*time*/,
    calendari::Calendari*  cal
  )
{
  // We are the source of a drag/drop.
  cal->main_view->drag_data_get(data,info);
}

G_MODULE_EXPORT void
cali_drawingarea_drag_data_received_cb(
    GtkWidget*,
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
  if(app->calendar_list->dialog)
      app->calendar_list->dialog->entry_cb(GTK_ENTRY(e));
  return false;
}


G_MODULE_EXPORT void
cal_file_set_cb(
    GtkFileChooserButton*  fc,
    calendari::Calendari*  app
  )
{
  if(app->calendar_list->dialog)
      app->calendar_list->dialog->file_set_cb(fc);
}


G_MODULE_EXPORT void
cal_color_set_cb(
    GtkColorButton*        cb,
    calendari::Calendari*  app
  )
{
  if(app->calendar_list->dialog)
      app->calendar_list->dialog->color_set_cb(cb);
}


G_MODULE_EXPORT void
cal_pub_radiobutton_toggled_cb(
    GtkToggleButton*       tb,
    calendari::Calendari*  app
  )
{
  if(gtk_toggle_button_get_active(tb) && app->calendar_list->dialog)
      app->calendar_list->dialog->activated(tb);
}


G_MODULE_EXPORT void
cal_button_clicked_cb(
    GtkButton*             b,
    calendari::Calendari*  app
  )
{
  if(app->calendar_list->dialog)
      app->calendar_list->dialog->button_clicked_cb(b);
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
detail_entry_activate_cb(GtkEntry* e, calendari::Calendari* cal)
{
  // The user has pressed <return> in a detail entry.
  // Save the entry, and return focus to the detail pane.
  cal->detail_view->entry_cb(GTK_ENTRY(e),cal);
  gtk_window_set_focus(
      GTK_WINDOW(cal->window),
      GTK_WIDGET(cal->main_drawingarea)
    );
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
