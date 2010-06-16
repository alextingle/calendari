#ifndef CALENDARI__DB_H
#define CALENDARI__DB_H 1

#include "event.h"

#include <map>
#include <sqlite3.h>
#include <string>
#include <sstream>

struct icalcomponent_impl;
typedef struct icalcomponent_impl icalcomponent;

namespace calendari {


struct Version
{
  /** CALENDAR, indexed by CALNUM. */
  std::map<int,Calendar*>                     _calendar;
  std::map<std::string,Event*>                _event;
  std::map<Occurrence::key_type,Occurrence*>  _occurrence;

  /** Clear away all events and occurrences for the given calender. */
  void purge(int calnum);
  /** Clear away all calendars, events and occurrences. */
  void destroy(void);
};


class Db
{
public:
  explicit Db(const char* dbname);
  ~Db(void);

  /** Creates tables and indices in the database. */
  void create_db(void);

  void refresh_cal(int calnum, int from_version, int to_version=1);

  /** Initial load of all calendar information. */
  void load_calendars(int version=1);

  /** Find all occurrences between the specified (begin,end] times. */
  std::multimap<time_t,Occurrence*> find(time_t begin,time_t end,int version=1);

  /** Look up the calnum of the given calid, or generate a new unique number. */
  int calnum(const char* calid);

  Calendar* calendar(int calnum, int version=1)
    {
      const std::map<int,Calendar*>& m( calendars(version) );
      std::map<int,Calendar*>::const_iterator it = m.find(calnum);
      return( it==m.end()? NULL: it->second );
    }

  const std::map<int,Calendar*>& calendars(int version=1)
    { return _ver[version]._calendar; }

  Occurrence* create_event(
      const char*  uid,
      time_t       dtstart,
      time_t       dtend,
      const char*  summary,
      bool         all_day,
      int          calnum,
      int          version=1
    );

  /** Read a VEVENT from the database, or create a new, empty one. */
  icalcomponent* vevent(const char* uid, int version=1);

  void moved(Occurrence* occ, int version=1);
  void erase(Occurrence* occ, int version=1);

  template<class T>
  T setting(const char* key, const T& dflt) const;

  template<class T>
  void set_setting(const char* key, const T& val) const;

  operator sqlite3* (void) const
    { return _sdb; }

private:
  sqlite3*               _sdb;
  std::map<int,Version>  _ver;

  Occurrence* make_occurrence(
      int          calnum,
      const char*  uid,
      const char*  summary,
      int          sequence,
      bool         all_day,
      bool         recurs,
      time_t       dtstart,
      time_t       dtend,
      int          version
    );

  /** Returns TRUE and sets value if key exists, else returns FALSE.  */
  bool _setting(const char* key, std::string& val) const;
  void _set_setting(const char* key, const char* val) const;
};


template<class T>
T Db::setting(const char* key, const T& dflt) const
{
  std::string valstr;
  if(_setting(key,valstr))
  {
    T result;
    std::istringstream is(valstr);
    is>>result;
    return result;
  }
  else
  {
    return dflt;
  }
}


template<class T>
void Db::set_setting(const char* key, const T& val) const
{
  std::ostringstream os;
  os<<val;
  _set_setting(key,os.str().c_str());
}


} // end namespace

#endif // CALENDARI__DB_H
