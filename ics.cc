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


const char* colours[] = {
    "#aa0000",
    "#0000aa",
    "#00aa00",
    "#006699",
    "#669900",
    "#990066",
    "#660099",
    "#009966",
    "#996600"
  };


// -- private --

int parse(
    const char*     ical_filename,
    icalcomponent*  ical,
    Db*             db,
    int             version
  )
{
  sqlite3* sdb = db->sqlite_db();

  // Prepare the insert statements.
  sqlite3_stmt*  insert_cal;
  const char* sql =
      "insert into CALENDAR "
        "(VERSION,CALNUM,CALID,CALNAME,PATH,POSITION,COLOUR,SHOW) "
        "values (?,?,?,?,?,?,?,1)";
  CALI_SQLCHK(sdb, ::sqlite3_prepare_v2(sdb,sql,-1,&insert_cal,NULL) );

  sqlite3_stmt*  insert_evt;
  sql="insert into EVENT (VERSION,UID,SUMMARY,CALID,SEQUENCE,ALLDAY,VEVENT) "
      "values (?,?,?,?,?,?,?)";
  CALI_SQLCHK(sdb, ::sqlite3_prepare_v2(sdb,sql,-1,&insert_evt,NULL) );

  sqlite3_stmt*  insert_occ;
  sql="insert into OCCURRENCE (VERSION,UID,DTSTART,DTEND) values (?,?,?,?)";
  CALI_SQLCHK(sdb, ::sqlite3_prepare_v2(sdb,sql,-1,&insert_occ,NULL) );

  CALI_SQLCHK(sdb, ::sqlite3_exec(sdb, "begin", 0, 0, 0) );

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
  // Get the calnum.
  int calnum = db->calnum(calid);
  assert(calnum);
  // Choose a colour.
  const char* colour =colours[ calnum % (sizeof(colours)/sizeof(char*)) ];
  // Bind these values to the statements.
  sql::bind_int( CALI_HERE,sdb,insert_cal,1,version);
  sql::bind_int( CALI_HERE,sdb,insert_cal,2,calnum);
  sql::bind_text(CALI_HERE,sdb,insert_cal,3,calid);
  sql::bind_text(CALI_HERE,sdb,insert_cal,4,calname);
  sql::bind_text(CALI_HERE,sdb,insert_cal,5,ical_filename);
  sql::bind_int( CALI_HERE,sdb,insert_cal,6,-1); // position
  sql::bind_text(CALI_HERE,sdb,insert_cal,7,colour);
  sql::step_reset(CALI_HERE,sdb,insert_cal);

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
    sql::bind_int( CALI_HERE,sdb,insert_evt,1,version);
    sql::bind_text(CALI_HERE,sdb,insert_evt,2,uid);
    sql::bind_text(CALI_HERE,sdb,insert_evt,3,summary);
    sql::bind_text(CALI_HERE,sdb,insert_evt,4,calid);
    sql::bind_int( CALI_HERE,sdb,insert_evt,5,sequence);
    sql::bind_int( CALI_HERE,sdb,insert_evt,6,all_day);
    sql::bind_text(CALI_HERE,sdb,insert_evt,7,vevent);
    sql::step_reset(CALI_HERE,sdb,insert_evt);

    sql::bind_int( CALI_HERE,sdb,insert_occ,1,version);
    sql::bind_text(CALI_HERE,sdb,insert_occ,2,uid);
    sql::bind_int( CALI_HERE,sdb,insert_occ,3,start_time);
    sql::bind_int( CALI_HERE,sdb,insert_occ,4,end_time);
    sql::step_reset(CALI_HERE,sdb,insert_occ);
  }
  CALI_SQLCHK(sdb, ::sqlite3_exec(sdb, "commit", 0, 0, 0) );
  CALI_SQLCHK(sdb, ::sqlite3_finalize(insert_evt) );
  CALI_SQLCHK(sdb, ::sqlite3_finalize(insert_occ) );
}


// -- public --

void read(const char* ical_filename, Db* db, int version)
{
  assert(ical_filename);
  assert(ical_filename[0]);
  assert(db);
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


} } // end namespace calendari::ical


#ifdef CALENDARI__ICAL__READ__TEST
int main(int argc, char* argv[])
{
  const char* sqlite_filename = argv[1];
  calendari::Db db(sqlite_filename);
  for(int i=2; i<argc; ++i)
  {
    const char* ics_filename = argv[i];
    calendari::ics::read(ics_filename,&db);
  }
}
#endif // test
