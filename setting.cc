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
    _auto_refresh_minutes(-1),
    _week_starts(-1)
{
  set_auto_refresh_minutes( app.db->setting("auto_refresh_minutes",10) );
  set_week_starts( app.db->setting("week_starts",1) ); // 1=Monday
}


void
Setting::set_auto_refresh_minutes(int new_val)
{
  if(_auto_refresh_minutes == new_val)
      return;
  if(_auto_refresh_minutes>=0)
      app.db->set_setting("auto_refresh_minutes",new_val);
  if(_timeout_source_tag!=0)
  {
    bool done = g_source_remove(_timeout_source_tag);
    assert(done); (void)done;
    _timeout_source_tag = 0;
  }
  _auto_refresh_minutes = new_val;
  if(new_val>0) // If 0, then auto refresh is OFF.
  {
    _timeout_source_tag = g_timeout_add(
        new_val * 60 * 1000, // in milliseconds.
        (GSourceFunc)calendari::CalendarList::timeout_refresh_all,
        (gpointer)(&app)
      );
  }
}


void
Setting::set_week_starts(int val)
{
  if(_week_starts == val)
      return;
  val %= 7;
  std::swap(_week_starts,val);
  if(val>=0)
  {
    app.db->set_setting("week_starts",_week_starts);
    app.queue_main_redraw(true);
  }
}


} // end namespace calendari
