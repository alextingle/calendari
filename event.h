#ifndef CALENDARI__EVENT_H
#define CALENDARI__EVENT_H 1

#include <string>
#include <time.h>

namespace calendari {


class Calendar
{
public:
  const std::string calid;
  
  Calendar(const char* i, const char* n, int p, const char* c, int s);

  const std::string& name(void)     const { return _name; }
  int                position(void) const { return _position; }
  const std::string& colour(void)   const { return _colour; }
  bool               show(void)     const { return _show; }

  void toggle_show(void) { _show = !_show; }

private:
  std::string _name;
  int         _position;
  std::string _colour;
  bool        _show;
};


class Event
{
public:
  const std::string  uid;

  Event(Calendar& c, const char* u, const char* s, int q, bool a);

  Calendar& calendar(void) const         { return *_calendar; }
  const std::string& summary(void) const { return _summary; }
  bool all_day(void) const               { return _all_day; }

  void set_calendar(Calendar& c)         { _calendar = &c; }
  void set_summary(const std::string& s) { _summary = s; }

private:
  Calendar*          _calendar;
  std::string        _summary;
  int                _sequence;
  bool               _all_day;
};


class Occurrence
{
public:
  typedef std::pair<time_t,std::string> key_type;

  Event&      event;
  
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
  time_t      _dtstart;
  time_t      _dtend;
  key_type    _key; ///< Location of this object in Db::_occurrence map.
};


} // end namespace calendari

#endif // CALENDARI__EVENT_H
