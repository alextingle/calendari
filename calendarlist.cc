#include "calendarlist.h"

#include "calendari.h"
#include "db.h"
#include "event.h"

#include <map>
#include <string>

namespace calendari {


void
CalendarList::build(Calendari* cal, GtkBuilder* builder)
{
  liststore_cal=GTK_LIST_STORE(gtk_builder_get_object(builder,"liststore_cal"));

  typedef std::map<std::string,Calendar*> CalMap;
  const CalMap& cc = cal->db->calendars();
  for(CalMap::const_iterator c=cc.begin(); c!=cc.end() ; ++c)
  {
    GtkTreeIter* iter =NULL;
    gtk_list_store_insert_with_values(
        this->liststore_cal, iter, 99999,
        0,TRUE,
        1,c->second->name.c_str(),
        2,c->second->colour.c_str(),
        -1
      );
  }


  gint i =gtk_tree_model_get_n_columns(GTK_TREE_MODEL(liststore_cal));
  printf("liststore_cal columns=%i\n",i);
}


void
CalendarList::add(void)
{
  GtkTreeIter* iter =NULL;
  gtk_list_store_insert_with_values(
      this->liststore_cal, iter, 99999,
      0,TRUE,
      1,"Foobar",
      2,"#0000bb",
      -1
    );
}


} // end namespace calendari
