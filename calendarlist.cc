#include "calendarlist.h"

#include "calendari.h"
#include "db.h"
#include "event.h"

#include <map>
#include <string>
#include <vector>

namespace calendari {


void
CalendarList::build(Calendari* cal, GtkBuilder* builder)
{
  liststore_cal=GTK_LIST_STORE(gtk_builder_get_object(builder,"liststore_cal"));

  typedef std::map<std::string,Calendar*> CalMap;
  const CalMap& cc = cal->db->calendars();

  // Sort by position.
  std::vector<Calendar*> vec(cc.size(),NULL);
  for(CalMap::const_iterator c=cc.begin(); c!=cc.end() ; ++c)
      vec[c->second->position] = c->second;

  for(std::vector<Calendar*>::const_iterator v=vec.begin(); v!=vec.end() ; ++v)
  {
    GtkTreeIter* iter =NULL;
    gtk_list_store_insert_with_values(
        this->liststore_cal, iter, 99999,
        0,TRUE,
        1,(*v)->name.c_str(),
        2,(*v)->colour.c_str(),
        -1
      );
  }
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
