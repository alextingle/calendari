#include "setting.h"

#include "calendari.h"
#include "calendarlist.h"
#include "db.h"

#include <cassert>
#include <cstdio>

namespace calendari {


Setting::Setting(Calendari& app_)
  : app(app_),
    _timeout_source_tag(0),
    _auto_refresh_minutes(0)
{
  // ?? This would be intialised from the DB...
  set_auto_refresh_minutes(10);
}


void
Setting::set_auto_refresh_minutes(int v)
{
  if(_auto_refresh_minutes==v)
      return;
  printf("set_auto_refresh_minutes: %d\n",v);
  if(_timeout_source_tag!=0)
  {
    printf("Removing existing timeout.\n");
    bool done = g_source_remove(_timeout_source_tag);
    assert(done);
    (void)done;
  }
  _auto_refresh_minutes = v;
  _timeout_source_tag = g_timeout_add(
      _auto_refresh_minutes * 60 * 1000, // in milliseconds.
      (GSourceFunc)calendari::CalendarList::timeout_refresh_all,
      (gpointer)(&app)
    );
}


} // end namespace calendari
