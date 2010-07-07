#ifndef CALENDARI__ICS__READER_H
#define CALENDARI__ICS__READER_H 1

#include <libical/ical.h>
#include <string>

namespace calendari {
  struct Calendari;
  class Db;
}

namespace calendari {
namespace ics {


/** Helper class used by ics functions that read calendars. */
class Reader
{
  icalcomponent*     _ical;
  const std::string  _ical_filename;
  bool               _discard_ids;

public:
  std::string     calid;
  std::string     calname;
  std::string     path;
  bool            readonly;

  /** Read _ical from 'ical_filename' and initialise members. */
  Reader(const char* ical_filename);
  ~Reader(void);

  /** Returns FALSE if 'calid' is already in use in database 'db'. */
  bool calid_is_unique(Db& db) const;

  /** Tell the object to throw away all of the unique IDs (calid & UIDs)
  *   read from the .ics file, and create new ones. */
  void discard_ids(void);

  /** Load the calendar into database 'db'. */
  int load(
      Calendari*   app,
      Db&          db,
      int          version
    );

private:
  Reader(Reader&);
  Reader& operator = (Reader&);
};


} } // end namespace calendari::ics

#endif // CALENDARI__ICS__READER_H
