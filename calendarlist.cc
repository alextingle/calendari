#include "calendarlist.h"

#include "calendari.h"
#include "db.h"
#include "event.h"
#include "event.h"
#include "ics.h"
#include "monthview.h"

#include <map>
#include <string>
#include <vector>

namespace calendari {


void
CalendarList::build(Calendari* cal, GtkBuilder* builder)
{
  liststore_cal=GTK_LIST_STORE(gtk_builder_get_object(builder,"liststore_cal"));
  treeview =GTK_TREE_VIEW(gtk_builder_get_object(builder,"cali_cals_treeview"));

  typedef std::map<int,Calendar*> CalMap;
  const CalMap& cc = cal->db->calendars();

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
CalendarList::toggle(gchar* path, calendari::Calendari* cal)
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
    gtk_widget_queue_draw(GTK_WIDGET(cal->main_drawingarea));
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
CalendarList::refresh(calendari::Calendari* cal)
{
  Calendar* curr = current();
  if(curr && !curr->path().empty())
  {
    if(curr->readonly())
    {
      printf("read %s at %s\n",curr->name().c_str(),curr->path().c_str());
      if(cal->selected() && cal->selected()->event.calendar()==*curr)
          cal->select(NULL);
      ics::read(curr->path().c_str(), *cal->db, 2);
      cal->db->refresh_cal(curr->calnum,2);
      cal->main_view->reload();
      gtk_widget_queue_draw(GTK_WIDGET(cal->main_drawingarea));
    }
    else
    {
      printf("write %s at %s\n",curr->name().c_str(),curr->path().c_str());
      ics::write(curr->path().c_str(), *cal->db, curr->calid.c_str());
    }
  }
}


} // end namespace calendari
