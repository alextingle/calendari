#include "ics.h"

#include "db.h"
#include "err.h"
#include "event.h"
#include "util.h"
#include "sql.h"

#include <cstring>
#include <iostream>
#include <libical/ical.h>
#include <sqlite3.h>
#include <string>

namespace
{
  /** Stream reader function for iCalendar parser. */
  char* read_stream(char *s, size_t size, void *d) 
  { 
    char *c = ::fgets(s,size, (FILE*)d);
    return c;
  }
}

namespace calendari {
namespace ics {

typedef scoped<icalcomponent,icalcomponent_free> SComponent;


// -- private --

int parse(
    const char*     ical_filename,
    icalcomponent*  ical,
    sqlite3*        db,
    int             version
  )
{
  // Prepare the insert statements.
  sqlite3_stmt*  insert_cal;
  const char* sql =
      "insert into CALENDAR (VERSION,CALID,CALNAME,PATH,POSITION,COLOUR,SHOW) "
      "values (?,?,?,?,?,?,1)";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&insert_cal,NULL) );

  sqlite3_stmt*  insert_evt;
  sql="insert into EVENT (VERSION,UID,SUMMARY,CALID,SEQUENCE,ALLDAY,VEVENT) "
      "values (?,?,?,?,?,?,?)";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&insert_evt,NULL) );

  sqlite3_stmt*  insert_occ;
  sql="insert into OCCURRENCE (VERSION,UID,DTSTART,DTEND) values (?,?,?,?)";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&insert_occ,NULL) );

  CALI_SQLCHK(db, ::sqlite3_exec(db, "begin", 0, 0, 0) );

  icalproperty* iprop;

  // Calendar properties
  
  // -- calid --
  const char* calid ="??dummy_id??";
  const char* calname ="??dummy_name??";
  iprop = icalcomponent_get_first_property(ical,ICAL_X_PROPERTY);
  while(iprop)
  {
    const char* x_name = icalproperty_get_x_name(iprop);
    if(0==::strcmp("X-WR-CALNAME",x_name))
        calname = icalproperty_get_x(iprop);
    else if(0==::strcmp("X-WR-RELCALID",x_name))
        calid = icalproperty_get_x(iprop);
    iprop = icalcomponent_get_next_property(ical,ICAL_X_PROPERTY);
  }
  // Bind these values to the statements.
  sql::bind_int( CALI_HERE,db,insert_cal,1,version);
  sql::bind_text(CALI_HERE,db,insert_cal,2,calid);
  sql::bind_text(CALI_HERE,db,insert_cal,3,calname);
  sql::bind_text(CALI_HERE,db,insert_cal,4,ical_filename);
  sql::bind_int( CALI_HERE,db,insert_cal,5,-1); // position
  sql::bind_text(CALI_HERE,db,insert_cal,6,"#cceeff");
  sql::step_reset(CALI_HERE,db,insert_cal);

  // Iterate through all components (VEVENTs).
  for(icalcompiter e=icalcomponent_begin_component(ical,ICAL_VEVENT_COMPONENT);
      icalcompiter_deref(&e)!=NULL;
      icalcompiter_next(&e))
  {
    icalcomponent* ievt = ::icalcompiter_deref(&e);
    const char* vevent = ::icalcomponent_as_ical_string(ievt);

    // -- uid --
    iprop = icalcomponent_get_first_property(ievt,ICAL_UID_PROPERTY);
    if(!iprop)
    {
      CALI_WARN(0,"missing VEVENT::UID property");
      continue;
    }
    const char* uid = icalproperty_get_uid(iprop);
    if(!uid)
    {
      CALI_WARN(0,"VEVENT::UID property has no value");
      continue;
    }

    // -- summary --
    iprop = icalcomponent_get_first_property(ievt,ICAL_SUMMARY_PROPERTY);
    if(!iprop)
    {
      CALI_WARN(0,"UID:%s missing VEVENT::SUMMARY property",uid);
      continue;
    }
    const char* summary = icalproperty_get_summary(iprop);
    if(!summary)
    {
      CALI_WARN(0,"UID:%s VEVENT::SUMMARY property has no value",uid);
      continue;
    }

    // -- sequence --
    iprop = icalcomponent_get_first_property(ievt,ICAL_SEQUENCE_PROPERTY);
    if(!iprop)
    {
      CALI_WARN(0,"UID:%s missing VEVENT::SEQUENCE property",uid);
      continue;
    }
    int sequence = icalproperty_get_sequence(iprop);

    // dtstart
    iprop = ::icalcomponent_get_first_property(ievt,ICAL_DTSTART_PROPERTY);
    if(!iprop)
    {
      CALI_WARN(0,"UID:%s missing VEVENT::DTSTART property",uid);
      continue;
    }
    struct icaltimetype dtstart = ::icalproperty_get_dtstart(iprop);
    time_t start_time = ::icaltime_as_timet(dtstart);

    // all_day
    int all_day = dtstart.is_date;

    // dtend
    iprop = ::icalcomponent_get_first_property(ievt,ICAL_DTEND_PROPERTY);
    if(!iprop)
    {
      CALI_WARN(0,"UID:%s missing VEVENT::DTEND property",uid);
      continue;
    }
    struct icaltimetype dtend = ::icalproperty_get_dtend(iprop);
    time_t end_time = ::icaltime_as_timet(dtend);

    // Bind these values to the statements.
    sql::bind_int( CALI_HERE,db,insert_evt,1,version);
    sql::bind_text(CALI_HERE,db,insert_evt,2,uid);
    sql::bind_text(CALI_HERE,db,insert_evt,3,summary);
    sql::bind_text(CALI_HERE,db,insert_evt,4,calid);
    sql::bind_int( CALI_HERE,db,insert_evt,5,sequence);
    sql::bind_int( CALI_HERE,db,insert_evt,6,all_day);
    sql::bind_text(CALI_HERE,db,insert_evt,7,vevent);
    sql::step_reset(CALI_HERE,db,insert_evt);

    sql::bind_int( CALI_HERE,db,insert_occ,1,version);
    sql::bind_text(CALI_HERE,db,insert_occ,2,uid);
    sql::bind_int( CALI_HERE,db,insert_occ,3,start_time);
    sql::bind_int( CALI_HERE,db,insert_occ,4,end_time);
    sql::step_reset(CALI_HERE,db,insert_occ);
  }
  CALI_SQLCHK(db, ::sqlite3_exec(db, "commit", 0, 0, 0) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_evt) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_occ) );
}


void read(const char* ical_filename, sqlite3* db, int version)
{
  // Parse the iCalendar file.
  icalparser* iparser = ::icalparser_new();
  FILE* stream = ::fopen(ical_filename,"r");
  ::icalparser_set_gen_data(iparser,stream);
  SComponent ical( ::icalparser_parse(iparser,read_stream) );
  ::fclose(stream);
  if(ical)
      parse(ical_filename,ical.get(),db,version);
  else
      CALI_ERRO(0,0,"failed to read calendar file %s",ical_filename);
  ::icalparser_free(iparser);
}


// -- public --

void read(const char* ical_filename, Db* db, int version)
{
  assert(ical_filename);
  assert(ical_filename[0]);
  assert(db);
  read(ical_filename,db->sqlite_db(),version);
}


} } // end namespace calendari::ical


#ifdef CALENDARI__ICAL__READ__TEST
int main(int argc, char* argv[])
{
  const char* sqlite_filename = argv[1];
  sqlite3* db;
  if( SQLITE_OK != ::sqlite3_open(sqlite_filename,&db) )
      CALI_ERRO(1,0,"Failed to open database %s",sqlite_filename);

  for(int i=2; i<argc; ++i)
  {
    const char* ics_filename = argv[i];
    calendari::ics::read(ics_filename,db);
  }

  ::sqlite3_close(db);
}
#endif // test
