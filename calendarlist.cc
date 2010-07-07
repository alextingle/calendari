#include "calendarlist.h"

#include "caldialog.h"
#include "calendari.h"
#include "db.h"
#include "err.h"
#include "event.h"
#include "ics.h"
#include "monthview.h"
#include "util.h"

#include <cassert>
#include <cstring>
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
  typedef std::multimap<int,Calendar*> Pos_t;
  Pos_t pos;
  for(CalMap::const_iterator c=cc.begin(); c!=cc.end() ; ++c)
      pos.insert( std::make_pair(c->second->position(),c->second) );

  for(Pos_t::const_iterator i=pos.begin(); i!=pos.end() ; ++i)
      add_calendar( *i->second );

  // -- cal_dialog --

  cal_sub_dialog = new CalSubDialog(app,builder);
  cal_pub_dialog = new CalPubDialog(app,builder);
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
  assert(calendar);

  if(calendar->readonly())
      dialog = cal_sub_dialog;
  else
      dialog = cal_pub_dialog;
  dialog->run();
  dialog = NULL;
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


Calendar*
CalendarList::find_writeable(void) const
{
  Calendar* cal = NULL;
  GtkTreeModel* m = GTK_TREE_MODEL(liststore_cal);
  GtkTreeIter iter;
  if(get_selected_iter(iter))
  {
    cal = iter2cal(iter);
    if(cal && !cal->readonly())
        return cal;
  }
  bool ok = gtk_tree_model_get_iter_first(m,&iter);
  while(ok)
  {
    cal = iter2cal(iter);
    if(cal && !cal->readonly())
        return cal;
    ok = gtk_tree_model_iter_next(m,&iter);
  }
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
CalendarList::add_calendar(Calendar& cal)
{
  int len = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(liststore_cal),NULL);
  cal.set_position(len);

  GdkColor col;
  gdk_color_parse( cal.colour().c_str(), &col );
  GdkPixbuf* pixbuf = new_pixbuf_from_col(col,12,12);

  GtkTreeIter* iter =NULL;
  gtk_list_store_insert_with_values(
      this->liststore_cal, iter, 99999,
      0,&cal,
      1,cal.show(),
      2,cal.name().c_str(),
      3,cal.colour().c_str(),
      4,!cal.readonly(),
      5,pixbuf,
      -1
    );
  g_object_unref(G_OBJECT(pixbuf));
}


bool
CalendarList::remove_selected_calendar(void)
{
  GtkTreeIter iter;
  if( !get_selected_iter(iter) )
      return false;
  gtk_list_store_remove(liststore_cal,&iter);
  return true;
}


void
CalendarList::refresh(calendari::Calendari* app, Calendar* cal)
{
  if(cal && !cal->path().empty())
  {
    if(cal->readonly())
    {
      printf("read %s at %s\n",cal->name().c_str(),cal->path().c_str());
      if(app->selected() && app->selected()->event.calendar()==*cal)
          app->select(NULL);
      ics::reread(app, cal->path().c_str(), *app->db, cal->calid.c_str(), 2);
      app->db->refresh_cal(cal->calnum,2);
      app->main_view->reload();
      app->queue_main_redraw();
    }
    else
    {
      ics::write(cal->path().c_str(), *app->db, cal->calid.c_str());
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
  if(occ)
      select( occ->event.calendar().position() );
}


void
CalendarList::select(int position, bool activate)
{
#ifndef NDEBUG
  int len = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(liststore_cal),NULL);
  assert(position < len);
#endif
  GtkTreePath* path = gtk_tree_path_new_from_indices(position,-1);
  gtk_tree_view_set_cursor(
      treeview,
      path,
      NULL, // focus_column,
      false // start_editing
    );
  if(activate)
      row_activated(path);
  gtk_tree_path_free(path);
}


} // end namespace calendari
