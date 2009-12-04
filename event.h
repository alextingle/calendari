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
public:
  Calendar&   calendar;
  std::string uid;
  std::string summary;
  int         sequence;
  bool        all_day;

  Event(Calendar& c): calendar(c) {}
};


class Occurrence
{
public:
  Event&      event;
  time_t      dtstart;
  time_t      dtend;
  
  Occurrence(Event& e): event(e) {}

  /** Returns TRUE if dtstart was actually changed. */
  bool set_start(time_t start_);

  /** Returns TRUE if dtend was actually changed. */
  bool set_end(time_t end_);
};


} // end namespace calendari

#endif // CALENDARI__EVENT_H
