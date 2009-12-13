#ifndef CALENDARI__ICS_H
#define CALENDARI__ICS_H 1

namespace calendari {
  class Db;
}


namespace calendari {
namespace ics {


/** Parse ical_filename and write the result to db. */
void read(const char* ical_filename, Db& db, int version=1);


} } // end namespace calendari::ics

#endif // CALENDARI__ICS_H
