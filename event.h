#ifndef CALENDARI__EVENT_H
#define CALENDARI__EVENT_H 1

#include <string>
#include <time.h>

struct icalcomponent_impl;
typedef struct icalcomponent_impl icalcomponent;

namespace calendari {


class Calendar
{
public:
  const int version;
  const std::string calid;
  const int calnum;
  
  Calendar(int v, const char* id, int cn, const char* nm, const char* pa,
      int rp, int ps, const char* cl, int sh);
  /** Write this to a new row in the database. */
  void create(void);

  const std::string& name(void)     const { return _name; }
  const std::string& path(void)     const { return _path; }
  bool               readonly(void) const { return _readonly; }
  int                position(void) const { return _position; }
  const std::string& colour(void)   const { return _colour; }
  bool               show(void)     const { return _show; }

  void set_name(const std::string& s);
  void set_path(const std::string& s);
  void set_position(int p);
  void set_colour(const std::string& s);
  void toggle_show(void);
  void touch(void); ///< Touch the calendar's datestamp (in the database).

  bool operator == (const Calendar& right) const
    { return version==right.version && calnum==right.calnum; }

private:
  std::string _name;
  std::string _path;
  bool        _readonly;
  int         _position;
  std::string _colour;
  bool        _show;
};


class Event
{
public:
  const std::string  uid;

  Event(Calendar& c, const char* u, const char* s, int q, bool a, bool r);
  ~Event(void);
  /** Write this to a new row in the database. */
  void create(void);

  Calendar& calendar(void) const         { return *_calendar; }
  const std::string& summary(void) const { return _summary; }
  bool all_day(void) const               { return _all_day; }
  bool recurs(void) const                { return _recurs; }
  bool readonly(void) const;
  const char* description(void) const;

  void set_calendar(Calendar& c);
  void set_summary(const std::string& s);
  void set_all_day(bool v);
  void set_description(const char* s);
  void increment_sequence(void);

private:
  Calendar*          _calendar;
  std::string        _summary;
  int                _sequence;
  bool               _all_day;
  bool               _recurs; ///< Event has an RRULE or RDATE property.

  /**  VEVENT ical component. If NULL, then not cached. Owned by 'this'. */
  mutable icalcomponent* _vevent;

  /** Read _vevent from database, if it is not already loaded. Creates
  *   a new VEVENT if there is none in the database. */
  void load_vevent(void) const;

  friend class Occurrence; // Allows _ref_count to be set.
  size_t  _ref_count; ///< Number of Occurrences that refer to this.
};


class Occurrence
{
public:
  typedef std::pair<std::string,time_t> key_type;

  Event&      event;
  
  Occurrence(Event& e, time_t t0, time_t t1);
  ~Occurrence(void);
  /** Write this to a new row in the database. */
  void create(void);

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
    { return _key = key_type(event.uid,_dtstart); }

  /** Notify this occurrence has been removed. */
  void destroy(void);

private:
  time_t      _dtstart;
  time_t      _dtend;
  key_type    _key; ///< Location of this object in Db::_occurrence map.
};


} // end namespace calendari

#endif // CALENDARI__EVENT_H
