#ifndef CALENDARI__DB_H
#define CALENDARI__DB_H 1

#include "event.h"

#include <map>
#include <sqlite3.h>
#include <string>

namespace calendari {


class Db
{
public:
  explicit Db(const char* dbname);
  ~Db(void);

  /** Creates tables and indices in the database. */
  void create_db(void);

  void refresh_cal(const char* calid, int version);

  /** Execute arbitrary SQL. Discards any resulting columns. */
  void exec(const char* sql);

  /** Execute arbitrary SQL. Discards any resulting columns. */
  void execf(const char* format, ...);

  void load_calendars(void);  
  std::multimap<time_t,Occurrence*> find(time_t begin, time_t end);

  const std::map<std::string,Calendar*>& calendars(void) const
    { return _calendar; }

  Occurrence* make_occurrence(
      const char*  uid,
      time_t       dtstart,
      time_t       dtend,
      const char*  summary,
      bool         all_day,
      const char*  calid
    );

  void moved(Occurrence* occ);
  void erase(Occurrence* occ);

  sqlite3* sqlite_db(void) const
    { return _db; }

private:
  sqlite3* _db;

  std::map<std::string,Calendar*>             _calendar;
  std::map<std::string,Event*>                _event;
  std::map<Occurrence::key_type,Occurrence*>  _occurrence;
};


} // end namespace

#endif // CALENDARI__DB_H
