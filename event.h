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
  
  Calendar(const char* calid_, const char* name_, int pos_, const char* col_)
    : calid(calid_), name(name_), position(pos_), colour(col_)
    {}
};


class Event
{
public:
  Calendar&   calendar;
  std::string uid;
  std::string summary;
  int         sequence;
  
  Event(Calendar& c): calendar(c) {}
};


class Occurrence
{
public:
  Event&      event;
  time_t      dtstart;
  time_t      dtend;
  
  Occurrence(Event& e): event(e) {}
};


} // end namespace calendari

#endif // CALENDARI__EVENT_H
