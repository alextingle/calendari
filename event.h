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
  Calendar*          _calendar;
public:
  const std::string  uid;
  std::string        _summary;
  int                sequence;
  bool               all_day;

  Event(Calendar& c, const char* u): _calendar(&c), uid(u) {}

  Calendar& calendar(void) const         { return *_calendar; }
  const std::string& summary(void) const { return _summary; }

  void set_calendar(Calendar& c)         { _calendar = &c; }
  void set_summary(const std::string& s) { _summary = s; }
};


class Occurrence
{
public:
  typedef std::pair<time_t,std::string> key_type;

  Event&      event;
  time_t      _dtstart;
  time_t      _dtend;
  
  Occurrence(Event& e, time_t t0, time_t t1):
    event(e), _dtstart(t0), _dtend(t1), _key(t0,e.uid)
    {}

  const time_t& dtstart(void) const
    { return _dtstart; }

  const time_t& dtend(void) const
    { return _dtend; }

  const key_type& key(void) const
    { return _key; }

  /** Returns TRUE if dtstart was actually changed. */
  bool set_start(time_t start_);

  /** Returns TRUE if dtend was actually changed. */
  bool set_end(time_t end_);

  /** Reset the _key, and return the new value. */
  const key_type& rekey(void)
    { return _key = key_type(_dtstart,event.uid); }

private:
  key_type    _key; ///< Location of this object in Db::_occurrence map.
};


} // end namespace calendari

#endif // CALENDARI__EVENT_H
