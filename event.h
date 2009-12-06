#ifndef CALENDARI__EVENT_H
#define CALENDARI__EVENT_H 1

#include <string>
#include <time.h>

namespace calendari {


class Calendar
{
public:
  std::string calid;
  std::string name;
  int         position;
  std::string colour;
  bool        show;
  
  Calendar(
      const char* calid_,
      const char* name_,
      int         pos_,
      const char* col_,
      int         show_
    )
    : calid(calid_), name(name_), position(pos_), colour(col_), show(show_)
    {}
};


class Event
{
  Calendar*   _calendar;
public:
  std::string uid;
  std::string summary;
  int         sequence;
  bool        all_day;

  Event(Calendar& c): _calendar(&c) {}
  Calendar& calendar(void) const
      { return *_calendar; }
  void calendar(Calendar& c)
      { _calendar = &c; }
};


class Occurrence
{
public:
  typedef std::pair<time_t,std::string> key_type;

  Event&      event;
  time_t      dtstart;
  time_t      dtend;
  
  Occurrence(Event& e, time_t t0, time_t t1):
    event(e), dtstart(t0), dtend(t1), _key(t0,e.uid)
    {}

  const key_type& key(void) const
    { return _key; }

  /** Returns TRUE if dtstart was actually changed. */
  bool set_start(time_t start_);

  /** Returns TRUE if dtend was actually changed. */
  bool set_end(time_t end_);

  /** Reset the _key, and return the new value. */
  const key_type& rekey(void)
    { return _key = key_type(dtstart,event.uid); }

private:
  key_type    _key; ///< Location of this object in Db::_occurrence map.
};


} // end namespace calendari

#endif // CALENDARI__EVENT_H
