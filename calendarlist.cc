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
CalendarList::idle(void* param)
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


Calendar*
CalendarList::current(void) const
{
  GtkTreeSelection* s = gtk_tree_view_get_selection(treeview);
  GtkTreeModel* m = GTK_TREE_MODEL(liststore_cal);
  GtkTreeIter iter;
  if(gtk_tree_selection_get_selected(s,NULL,&iter) ||
     gtk_tree_model_get_iter_first(m,&iter))
  {
    Calendar* result;
    gtk_tree_model_get(m,&iter, 0,&result, -1);
    return result;
  }
  else
  {
    return NULL;
  }
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
      ics::read(calendar->path().c_str(), *app->db, 2);
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
  (void)g_idle_add((GSourceFunc)idle,(gpointer)app);
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


} // end namespace calendari
