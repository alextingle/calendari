#ifndef CALENDARI__ICS_H
#define CALENDARI__ICS_H 1

struct icalcomponent_impl;
typedef struct icalcomponent_impl icalcomponent;

namespace calendari {
  struct Calendari;
  class Db;
}


namespace calendari {
namespace ics {


/** Parse ical_filename and write the result to db. */
void read(Calendari* app, const char* ical_filename, Db& db, int version=1);

/** Write from the db to ical_filename. */
void write(const char* ical_filename, Db& db, const char* calid, int version=1);

/** Construct a whole new, empty VEVENT. Ownership is passed to the caller. */
icalcomponent* make_new_vevent(const char* uid);


} } // end namespace calendari::ics

#endif // CALENDARI__ICS_H
