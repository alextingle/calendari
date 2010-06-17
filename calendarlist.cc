#include "calendarlist.h"

#include "calendari.h"
#include "db.h"
#include "err.h"
#include "event.h"
#include "ics.h"
#include "monthview.h"

#include <map>
#include <string>
#include <vector>

namespace calendari {


bool
CalendarList::timeout_refresh_all(void* param)
{
  Calendari* app = static_cast<Calendari*>(param);
  app->calendar_list->refresh_all(app);
  return true;
}


bool
CalendarList::idle_refresh_next(void* param)
{
  Calendari* app = static_cast<Calendari*>(param);
  return app->calendar_list->refresh_next(app);
}


void
CalendarList::build(Calendari* app, GtkBuilder* builder)
{
  liststore_cal=GTK_LIST_STORE(gtk_builder_get_object(builder,"liststore_cal"));
  treeview =GTK_TREE_VIEW(gtk_builder_get_object(builder,"cali_cals_treeview"));

  typedef std::map<int,Calendar*> CalMap;
  const CalMap& cc = app->db->calendars();

  // Sort by position.
  std::vector<Calendar*> vec(cc.size(),NULL);
  for(CalMap::const_iterator c=cc.begin(); c!=cc.end() ; ++c)
      vec[c->second->position()] = c->second;

  for(std::vector<Calendar*>::const_iterator v=vec.begin(); v!=vec.end() ; ++v)
  {
    GtkTreeIter* iter =NULL;
    gtk_list_store_insert_with_values(
        this->liststore_cal, iter, 99999,
        0,*v,
        1,(*v)->show(),
        2,(*v)->name().c_str(),
        3,(*v)->colour().c_str(),
        4,!(*v)->readonly(),
        -1
      );
  }

  // -- cal_dialog --

  cal_dialog = GTK_DIALOG(gtk_builder_get_object(builder,"cali_cal_dialog"));
  name_entry = GTK_ENTRY(gtk_builder_get_object(builder,"cal_name_entry"));
}


void
CalendarList::reorder(void)
{
  GtkTreeModel* m = GTK_TREE_MODEL(liststore_cal);
  GtkTreeIter iter;
  if(!gtk_tree_model_get_iter_first(m,&iter))
  {
    CALI_WARN(0,"Failed to get first iterator from calendar list store.");
    return;
  }
  int position = 0;
  while(true)
  {
    Calendar* calendar;
    gtk_tree_model_get(m,&iter,0,&calendar,-1);
    calendar->set_position( position++ );
    if(!gtk_tree_model_iter_next(m,&iter))
        break;
  }
}


void
CalendarList::row_activated(GtkTreePath* tp)
{
  GtkTreeIter iter;
  bool ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(liststore_cal),&iter,tp);
  if(!ret)
      return;

  Calendar* calendar;
  gtk_tree_model_get(GTK_TREE_MODEL(liststore_cal),&iter,0,&calendar,-1);

  // set-up cal_dialog from calendar.
  gtk_entry_set_text(name_entry,calendar->name().c_str());
  const bool sensitive = !calendar->readonly();
  gtk_widget_set_sensitive(GTK_WIDGET(name_entry),true);

  // Show dialogue box.
  int response = gtk_dialog_run(cal_dialog);
  switch(response)
  {
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CANCEL:
      gtk_widget_hide(GTK_WIDGET(cal_dialog));
      break;
    default:
      break;
  }
}


Calendar*
CalendarList::iter2cal(GtkTreeIter& iter) const
{
  Calendar* result;
  gtk_tree_model_get(GTK_TREE_MODEL(liststore_cal) ,&iter, 0, &result, -1);
  return result;
}


bool
CalendarList::get_selected_iter(GtkTreeIter& iter) const
{
  GtkTreeSelection* s = gtk_tree_view_get_selection(treeview);
  return gtk_tree_selection_get_selected(s,NULL,&iter);
}


Calendar*
CalendarList::current(void) const
{
  GtkTreeModel* m = GTK_TREE_MODEL(liststore_cal);
  GtkTreeIter iter;
  if(get_selected_iter(iter) || gtk_tree_model_get_iter_first(m,&iter))
      return iter2cal(iter);
  else
      return NULL;
}


void
CalendarList::toggle(gchar* path, calendari::Calendari* app)
{
  GtkTreePath* tp = gtk_tree_path_new_from_string(path);
  GtkTreeIter iter;
  bool ret = gtk_tree_model_get_iter(GTK_TREE_MODEL(liststore_cal),&iter,tp);
  if(ret)
  {
    Calendar* calendar;
    gtk_tree_model_get(GTK_TREE_MODEL(liststore_cal),&iter,0,&calendar,-1);
    calendar->toggle_show();
    gtk_list_store_set(liststore_cal,&iter,1,calendar->show(),-1);
    app->queue_main_redraw();
  }
  gtk_tree_path_free(tp);
}


void
CalendarList::add(void)
{
  // ?? Unused method?
  GtkTreeIter* iter =NULL;
  gtk_list_store_insert_with_values(
      this->liststore_cal, iter, 99999,
      0,NULL,
      1,TRUE, // show
      2,"Foobar",
      3,"#0000bb",
      4,TRUE, // writeable
      -1
    );
}


void
CalendarList::refresh(calendari::Calendari* app, Calendar* calendar)
{
  if(calendar && !calendar->path().empty())
  {
    if(calendar->readonly())
    {
      printf("read %s at %s\n",calendar->name().c_str(),calendar->path().c_str());
      if(app->selected() && app->selected()->event.calendar()==*calendar)
          app->select(NULL);
      ics::read(app, calendar->path().c_str(), *app->db, 2);
      app->db->refresh_cal(calendar->calnum,2);
      app->main_view->reload();
      app->queue_main_redraw();
    }
    else
    {
      printf("write %s at %s\n",calendar->name().c_str(),calendar->path().c_str());
      ics::write(calendar->path().c_str(), *app->db, calendar->calid.c_str());
    }
  }
}


void
CalendarList::refresh_selected(calendari::Calendari* app)
{
  refresh(app,current());
}


void
CalendarList::refresh_all(calendari::Calendari* app)
{
  GtkTreeModel* m = GTK_TREE_MODEL(liststore_cal);
  GtkTreeIter iter;
  if(!gtk_tree_model_get_iter_first(m,&iter))
  {
    CALI_WARN(0,"Failed to get first iterator from calendar list store.");
    return;
  }
  // Enqueue all calendars.
  while(true)
  {
    Calendar* calendar;
    gtk_tree_model_get(m,&iter,0,&calendar,-1);
    if(calendar && !calendar->path().empty())
        refresh_queue.insert( calendar->calnum );
    if(!gtk_tree_model_iter_next(m,&iter))
        break;
  }
  // Hook-in idle processing.
  (void)g_idle_add((GSourceFunc)idle_refresh_next,(gpointer)app);
}


bool
CalendarList::refresh_next(Calendari* app)
{
  if(refresh_queue.empty())
      return false; // Nothing more to do.
  std::set<int>::iterator next_calnum = refresh_queue.begin();
  Calendar* calendar = app->db->calendar(*next_calnum);
  refresh_queue.erase(next_calnum);
  if(!calendar)
      return refresh_next(app);
  refresh(app,calendar);
  return !refresh_queue.empty();
}


void
CalendarList::select(Occurrence* occ)
{
  if(!occ)
      return;
  int pos = occ->event.calendar().position();
  GtkTreePath* path = gtk_tree_path_new_from_indices(pos,-1);
  gtk_tree_view_set_cursor(
      treeview,
      path,
      NULL, // focus_column,
      false // start_editing
    );
  gtk_tree_path_free(path);
}


// -- cal_dialog --

void
CalendarList::entry_cb(GtkEntry* entry, calendari::Calendari*)
{
  GtkTreeIter iter;
  if(!get_selected_iter(iter))
    return;
  Calendar* calendar = iter2cal(iter);

  const char* newval = gtk_entry_get_text(entry);
  if(entry==name_entry)
  {
    if(calendar->name()!=newval && ::strlen(newval))
    {
      calendar->set_name( newval );
      GValue gv = {0};
      g_value_init(&gv,G_TYPE_STRING);
      g_value_set_string(&gv,newval);
      gtk_list_store_set_value(liststore_cal,&iter,2,&gv);
    }
  }
}


} // end namespace calendari
