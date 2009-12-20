#include "ics.h"

#include "db.h"
#include "err.h"
#include "event.h"
#include "util.h"
#include "sql.h"

#include <cstring>
#include <fstream>
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
    Db&             db,
    int             version
  )
{
  // Prepare the insert statements.
  sqlite3_stmt*  insert_cal;
  const char* sql =
      "insert into CALENDAR "
        "(VERSION,CALNUM,CALID,CALNAME,PATH,POSITION,COLOUR,SHOW) "
        "values (?,?,?,?,?,?,?,1)";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&insert_cal,NULL) );

  sqlite3_stmt*  insert_evt;
  sql="insert into EVENT "
        "(VERSION,CALNUM,UID,SUMMARY,SEQUENCE,ALLDAY,VEVENT) "
        "values (?,?,?,?,?,?,?)";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&insert_evt,NULL) );

  sqlite3_stmt*  insert_occ;
  sql="insert into OCCURRENCE "
        "(VERSION,CALNUM,UID,DTSTART,DTEND) values (?,?,?,?,?)";
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
  // Get the calnum.
  int calnum = db.calnum(calid);
  assert(calnum);
  // Choose a colour.
  const char* colour =colours[ calnum % (sizeof(colours)/sizeof(char*)) ];
  // Bind these values to the statements.
  sql::bind_int( CALI_HERE,db,insert_cal,1,version);
  sql::bind_int( CALI_HERE,db,insert_cal,2,calnum);
  sql::bind_text(CALI_HERE,db,insert_cal,3,calid);
  sql::bind_text(CALI_HERE,db,insert_cal,4,calname);
  sql::bind_text(CALI_HERE,db,insert_cal,5,ical_filename);
  sql::bind_int( CALI_HERE,db,insert_cal,6,-1); // position
  sql::bind_text(CALI_HERE,db,insert_cal,7,colour);
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
    sql::bind_int( CALI_HERE,db,insert_evt,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_evt,3,uid);
    sql::bind_text(CALI_HERE,db,insert_evt,4,summary);
    sql::bind_int( CALI_HERE,db,insert_evt,5,sequence);
    sql::bind_int( CALI_HERE,db,insert_evt,6,all_day);
    sql::bind_text(CALI_HERE,db,insert_evt,7,vevent);
    sql::step_reset(CALI_HERE,db,insert_evt);

    sql::bind_int( CALI_HERE,db,insert_occ,1,version);
    sql::bind_int( CALI_HERE,db,insert_occ,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_occ,3,uid);
    sql::bind_int( CALI_HERE,db,insert_occ,4,start_time);
    sql::bind_int( CALI_HERE,db,insert_occ,5,end_time);
    sql::step_reset(CALI_HERE,db,insert_occ);
  }
  CALI_SQLCHK(db, ::sqlite3_exec(db, "commit", 0, 0, 0) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_evt) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_occ) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_cal) );
}


// -- public --

void read(const char* ical_filename, Db& db, int version)
{
  assert(ical_filename);
  assert(ical_filename[0]);
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


void write(const char* ical_filename, Db& db, const char* calid, int version)
{
  assert(calid);
  assert(calid[0]);

  icalproperty* prop;
  icalparameter* param;

  // Load calendar from database.
  sqlite3_stmt* select_cal;
  const char* sql =
      "select CALNUM,CALNAME,PATH,READONLY "
      "from CALENDAR "
      "where VERSION=? and CALID=? ";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&select_cal,NULL) );
  sql::bind_int( CALI_HERE,db,select_cal,1,version);
  sql::bind_text(CALI_HERE,db,select_cal,2,calid);

  int return_code = ::sqlite3_step(select_cal);
  if(return_code==SQLITE_DONE)
  {
    util::error(CALI_HERE,0,0,
        "Can't find calendar id %s in the database.",calid);
    return;
  }
  else if(return_code!=SQLITE_ROW)
  {
    calendari::sql::error(CALI_HERE,db);
    return;
  }

  int         calnum   =         ::sqlite3_column_int( select_cal,0);
  const char* calname  = safestr(::sqlite3_column_text(select_cal,1));
  const char* path     = safestr(::sqlite3_column_text(select_cal,2));
  bool        readonly =         ::sqlite3_column_int( select_cal,3);

  // Check that we are allowed to write this calendar.
  // ?? Also check destination file mode.
  if(readonly && 0==::strcmp(ical_filename,path)) // ?? use proper path compare
  {
    util::error(CALI_HERE,0,0,"Calendar is read only: %s",path);
    return;
  }

  SComponent ical(
      icalcomponent_vanew(
        ICAL_VCALENDAR_COMPONENT,
        icalproperty_new_method(ICAL_METHOD_PUBLISH),
        icalproperty_new_prodid("-//firetree.net//Calendari 0.1//EN"),
        icalproperty_new_calscale("GREGORIAN"),
        icalproperty_new_version("2.0"),
        0
    ) );
  // CALID => X-WR-RELCALID
  prop = icalproperty_new_x( calid );
  icalproperty_set_x_name(prop,"X-WR-RELCALID");
  icalcomponent_add_property(ical.get(),prop);
  // CALNAME,1 => X-WR-CALNAME
  prop = icalproperty_new_x( calname );
  icalproperty_set_x_name(prop,"X-WR-CALNAME");
  icalcomponent_add_property(ical.get(),prop);
  // ?? => X-WR-TIMEZONE
  prop = icalproperty_new_x("Europe/London"); // ??
  icalproperty_set_x_name(prop,"X-WR-TIMEZONE");
  icalcomponent_add_property(ical.get(),prop);

  CALI_SQLCHK(db, ::sqlite3_finalize(select_cal) );
  
  // ?? VTIMEZONE component

  // Read in VEVENTS from the database...
  // Load calendar from database.
  sqlite3_stmt* select_evt;
  // Eek! a self-join to find the *first* occurrence for each event.
  sql = "select E.UID,SUMMARY,SEQUENCE,ALLDAY,VEVENT,O.DTSTART,O.DTEND "
        "from (select UID,min(DTSTART) as S from "
               "OCCURRENCE where VERSION=? and CALNUM=? group by UID) K "
        "left join OCCURRENCE O on K.UID=O.UID and K.S=O.DTSTART "
        "left join EVENT E on E.UID=O.UID and E.VERSION=O.VERSION "
        "where E.VERSION=? and E.CALNUM=? "
        "order by SUMMARY";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&select_evt,NULL) );
  sql::bind_int(CALI_HERE,db,select_evt,1,version);
  sql::bind_int(CALI_HERE,db,select_evt,2,calnum);
  sql::bind_int(CALI_HERE,db,select_evt,3,version);
  sql::bind_int(CALI_HERE,db,select_evt,4,calnum);

  while(true)
  {
    int return_code = ::sqlite3_step(select_evt);
    if(return_code==SQLITE_DONE)
    {
      break;
    }
    else if(return_code!=SQLITE_ROW)
    {
      calendari::sql::error(CALI_HERE,db);
      return;
    }
    const char* uid      = safestr(::sqlite3_column_text(select_evt,0));
    const char* summary  = safestr(::sqlite3_column_text(select_evt,1));
    int         sequence =         ::sqlite3_column_int( select_evt,2);
    bool        allday   =         ::sqlite3_column_int( select_evt,3);
    const char* veventz  = safestr(::sqlite3_column_text(select_evt,4));
    time_t      dtstart  =         ::sqlite3_column_int( select_evt,5);
    time_t      dtend    =         ::sqlite3_column_int( select_evt,6);

    // ...and populate them with any modifications.

    icalcomponent* vevent;
    if(veventz && veventz[0])
    {
      vevent = icalparser_parse_string(veventz);
      // Eliminate parts that we already have.
      prop = icalcomponent_get_first_property(vevent,ICAL_ANY_PROPERTY);
      while(prop)
      {
        icalproperty* next =
            icalcomponent_get_next_property(vevent,ICAL_ANY_PROPERTY);
        const char* name = icalproperty_get_property_name(prop);
        if( 0==::strcmp(name,"DTSTART") ||
            0==::strcmp(name,"DTEND") ||
            0==::strcmp(name,"DTSTAMP") ||
            0==::strcmp(name,"SUMMARY") ||
            0==::strcmp(name,"SEQUENCE") )
        {
          icalcomponent_remove_property(vevent,prop);
          icalproperty_free(prop);
        }
        prop = next;
      }

    }
    else
    {
      vevent = icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
          icalproperty_new_uid(uid),
          icalproperty_new_created( icaltime_from_timet(::time(NULL),false) ),
          icalproperty_new_transp(ICAL_TRANSP_OPAQUE), //??
          0
        );
    }
    icalcomponent_add_property(vevent,
        icalproperty_new_summary( summary )
      );
    icalcomponent_add_property(vevent,
        icalproperty_new_sequence( sequence )
      );
    prop = icalproperty_new_dtstamp( icaltime_from_timet(::time(NULL),false) );
    icalcomponent_add_property(vevent,prop);

    prop = icalproperty_new_dtstart( icaltime_from_timet(dtstart,allday) );
    param = icalparameter_new_tzid("Europe/London"); //?? Set this properly
    icalproperty_add_parameter(prop,param);
    icalcomponent_add_property(vevent,prop);

    prop = icalproperty_new_dtend( icaltime_from_timet(dtend,allday) );
    param = icalparameter_new_tzid("Europe/London"); //?? Set this properly
    icalproperty_add_parameter(prop,param);
    icalcomponent_add_property(vevent,prop);

    // Add this VEVENT to our calendar
    icalcomponent_add_component(ical.get(),vevent);
  }
  CALI_SQLCHK(db, ::sqlite3_finalize(select_evt) );

  // Write out the iCalendar file.
  std::ofstream ofile(ical_filename);
  if(ofile)
      ofile << icalcomponent_as_ical_string( ical.get() );
}


} } // end namespace calendari::ical


#ifdef CALENDARI__ICAL__READ__TEST
int main(int argc, char* argv[])
{
  // Syntax: test_ics_read DB [ICS_FNAME ...]
  const char* sqlite_filename = argv[1];
  calendari::Db db(sqlite_filename);
  for(int i=2; i<argc; ++i)
  {
    const char* ics_filename = argv[i];
    calendari::ics::read(ics_filename,db);
  }
}
#endif // test


#ifdef CALENDARI__ICAL__WRITE__TEST
int main(int argc, char* argv[])
{
  // Syntax: test_ics_write DB [CALID ICS_FNAME ...]
  const char* sqlite_filename = argv[1];
  calendari::Db db(sqlite_filename);
  for(int i=3; i<argc; i+=2)
  {
    const char* calid = argv[i-1];
    const char* ics_filename = argv[i];
    calendari::ics::write(ics_filename,db,calid,1);
  }
}
#endif // test
