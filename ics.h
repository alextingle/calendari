#ifndef CALENDARI__ICS_H
#define CALENDARI__ICS_H 1

#include <string>

struct icalcomponent_impl;
typedef struct icalcomponent_impl icalcomponent;

namespace calendari {
  struct Calendari;
  class Db;
}


namespace calendari {
namespace ics {


/** Generate a new unique event ID. */
std::string generate_uid(void);

/** Parse ical_filename and write the result to db. Calid must be new.
*   Return the 'calnum' of the calendar we read in, or -1 in the case of
*   failure. */
int subscribe(
    Calendari*   app,
    const char*  ical_filename,
    Db&          db,
    int          version=1
  );

/** Parse ical_filename and write the result to db. Calid is *made* unique.
*   Return the 'calnum' of the calendar we read in, or -1 in the case of
*   failure. */
int import(
    Calendari*   app,
    const char*  ical_filename,
    Db&          db,
    int          version=1
  );

/** Reread an existing calendar from ical_filename and write the result to db.
*   Return the 'calnum' of the calendar we read in, or -1 in the case of
*   failure. */
int reread(
    Calendari*   app,
    const char*  ical_filename,
    Db&          db,
    const char*  calid,
    int          version=1
  );

/** Write from the db to ical_filename. */
void write(const char* ical_filename, Db& db, const char* calid, int version=1);

/** Construct a whole new, empty VEVENT. Ownership is passed to the caller. */
icalcomponent* make_new_vevent(const char* uid);


} } // end namespace calendari::ics

#endif // CALENDARI__ICS_H
