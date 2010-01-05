#include "ics.h"

#include "db.h"
#include "err.h"
#include "event.h"
#include "util.h"
#include "sql.h"

#include <cstring>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <libical/ical.h>
#include <sqlite3.h>
#include <string>
#include <unistd.h>

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

typedef scoped<icalparser,icalparser_free> SParser;
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

/** Convert icaltime 'it' into time_t.
*   This function actually works, unlike the libical versions which either
*   assume that icaltime is always in UTC or fail to restore the system
*   timezone properly. */
time_t
ical2timet(icaltimetype& it)
{
  if(it.is_utc)
      return ::icaltime_as_timet(it);

  tm result_tm;
  ::memset(&result_tm,0,sizeof(result_tm));
  result_tm.tm_year = it.year - 1900;
  result_tm.tm_mon  = it.month - 1;
  result_tm.tm_mday = it.day;

  if(it.is_date)
  {
    // For date only, set time_t to be mid-day, UTC.
    // This moment should get the day right, whatever the timezone.
    result_tm.tm_hour = 12;
    return ::timegm(&result_tm);
  }

  result_tm.tm_hour = it.hour;
  result_tm.tm_min  = it.minute;
  result_tm.tm_sec  = it.second;
  result_tm.tm_isdst= -1; // work it out yourself

  time_t result;

  const char* tzid =icaltime_get_tzid(it);
  if(tzid)
  {
    char* tz;
    tz = ::getenv("TZ");
    ::setenv("TZ", tzid, 1);
    ::tzset();

    result = ::mktime(&result_tm);

    if(tz)
      ::setenv("TZ", tz, 1);
    else
      ::unsetenv("TZ");
    ::tzset();
  }
  else
  {
    // Assume that 'it' is in local time.
    result = ::mktime(&result_tm);
  }
  return result;
}


/** Convert time_t 't' into icaltime. */
icaltimetype
timet2ical(time_t t, bool is_date, const char* tzid = NULL)
{
  tm local_tm;
  ::memset(&local_tm,0,sizeof(local_tm));
  if(tzid)
  {
    char* tz;
    tz = ::getenv("TZ");
    ::setenv("TZ", tzid, 1);
    ::tzset();

    ::localtime_r(&t, &local_tm);

    if(tz)
      ::setenv("TZ", tz, 1);
    else
      ::unsetenv("TZ");
    ::tzset();
  }
  else
  {
    ::gmtime_r(&t, &local_tm);
  }

  icaltimetype it;
  ::memset(&it,0,sizeof(it));
  it.year   = local_tm.tm_year + 1900;
  it.month  = local_tm.tm_mon + 1;
  it.day    = local_tm.tm_mday;
  if(is_date)
  {
    it.is_date = 1;
  }
  else
  {
    it.hour   = local_tm.tm_hour;
    it.minute = local_tm.tm_min;
    it.second = local_tm.tm_sec;
  }
  it.is_utc = (tzid? 0: 1);
  return it;
}


void make_occurrence(
    icaltimetype&  dtstart,
    time_t         duration,
    sqlite3*       db,
    sqlite3_stmt*  insert_occ
  )
{
  time_t start_time = ical2timet(dtstart);
  time_t end_time = start_time + duration;
  sql::bind_int(  CALI_HERE,db,insert_occ,4,start_time);
  sql::bind_int(  CALI_HERE,db,insert_occ,5,end_time);
  sql::step_reset(CALI_HERE,db,insert_occ);
}


/** Based on source from libical. */
void process_rrule(
    icalcomponent*  ievt,
    icaltimetype&   dtstart,
    icaltimetype&   dtend,
    sqlite3*        db,
    sqlite3_stmt*   insert_occ
  )
{
  time_t start_time = ical2timet(dtstart);
  time_t end_time   = ical2timet(dtend);
  assert(end_time>=start_time);
  const time_t duration = end_time - start_time;

  make_occurrence(dtstart, duration, db,insert_occ);

  // Cycle through RRULE entries.
  icalproperty* rrule;
  for (rrule = icalcomponent_get_first_property(ievt,ICAL_RRULE_PROPERTY);
       rrule != NULL;
       rrule = icalcomponent_get_next_property(ievt,ICAL_RRULE_PROPERTY))
  {
    struct icalrecurrencetype recur = icalproperty_get_rrule(rrule);
    icalrecur_iterator *rrule_itr  = icalrecur_iterator_new(recur, dtstart);
    struct icaltimetype rrule_time;
    if(rrule_itr)
        rrule_time = icalrecur_iterator_next(rrule_itr);
    // note: icalrecur_iterator_next always returns dtstart the first time...

    while(rrule_itr)
    {
      rrule_time = icalrecur_iterator_next(rrule_itr);
      if(icaltime_is_null_time(rrule_time))
          break;
      if(!icalproperty_recurrence_is_excluded(ievt, &dtstart, &rrule_time))
      {
        make_occurrence(rrule_time, duration, db,insert_occ);
      }
    }
    icalrecur_iterator_free(rrule_itr);
  }

  // Process RDATE entries
  icalproperty* rdate;
  for (rdate = icalcomponent_get_first_property(ievt,ICAL_RDATE_PROPERTY);
       rdate != NULL;
       rdate = icalcomponent_get_next_property(ievt,ICAL_RDATE_PROPERTY))
  {
    struct icaldatetimeperiodtype rdate_period = icalproperty_get_rdate(rdate);

    /** RDATES can specify raw datetimes, periods, or dates.
	we only support raw datetimes for now..

        @todo Add support for other types **/

    if (icaltime_is_null_time(rdate_period.time))
      continue;

    if(!icalproperty_recurrence_is_excluded(ievt, &dtstart,&rdate_period.time))
    {
      make_occurrence(rdate_period.time, duration, db,insert_occ);
    }
  }
}


// -- public --

void read(const char* ical_filename, Db& db, int version)
{
  assert(ical_filename);
  assert(ical_filename[0]);
  // Parse the iCalendar file.
  SParser iparser( ::icalparser_new() );
  FILE* stream = ::fopen(ical_filename,"r");
  if(!stream)
  {
    CALI_ERRO(0,errno,"failed to open calendar file %s",ical_filename);
    return;
  }
  ::icalparser_set_gen_data(iparser.get(),stream);
  SComponent ical( ::icalparser_parse(iparser.get(),read_stream) );
  ::fclose(stream);
  if(!ical)
  {
    CALI_ERRO(0,0,"failed to read calendar file %s",ical_filename);
    return;
  }

  // Prepare the insert statements.
  sqlite3_stmt*  insert_cal;
  const char* sql =
      "insert into CALENDAR "
        "(VERSION,CALNUM,CALID,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW) "
        "values (?,?,?,?,?,?,?,?,1)";
  CALI_SQLCHK(db, ::sqlite3_prepare_v2(db,sql,-1,&insert_cal,NULL) );

  sqlite3_stmt*  insert_evt;
  sql="insert into EVENT "
        "(VERSION,CALNUM,UID,SUMMARY,SEQUENCE,ALLDAY,RECURS,VEVENT) "
        "values (?,?,?,?,?,?,?,?)";
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
  iprop = icalcomponent_get_first_property(ical.get(),ICAL_X_PROPERTY);
  while(iprop)
  {
    const char* x_name = icalproperty_get_x_name(iprop);
    if(0==::strcmp("X-WR-CALNAME",x_name))
        calname = icalproperty_get_x(iprop);
    else if(0==::strcmp("X-WR-RELCALID",x_name))
        calid = icalproperty_get_x(iprop);
    iprop = icalcomponent_get_next_property(ical.get(),ICAL_X_PROPERTY);
  }
  // Get the calnum.
  int calnum = db.calnum(calid);
  assert(calnum);
  // Is it readonly?
  bool readonly =( 0 != ::access(ical_filename,W_OK) );
  // Choose a colour.
  const char* colour =colours[ calnum % (sizeof(colours)/sizeof(char*)) ];
  // Bind these values to the statements.
  sql::bind_int( CALI_HERE,db,insert_cal,1,version);
  sql::bind_int( CALI_HERE,db,insert_cal,2,calnum);
  sql::bind_text(CALI_HERE,db,insert_cal,3,calid);
  sql::bind_text(CALI_HERE,db,insert_cal,4,calname);
  sql::bind_text(CALI_HERE,db,insert_cal,5,ical_filename);
  sql::bind_int( CALI_HERE,db,insert_cal,6,readonly);
  sql::bind_int( CALI_HERE,db,insert_cal,7,-1); // position
  sql::bind_text(CALI_HERE,db,insert_cal,8,colour);
  sql::step_reset(CALI_HERE,db,insert_cal);

  // Iterate through all components (VEVENTs).
  for(icalcompiter e=icalcomponent_begin_component(ical.get(),ICAL_VEVENT_COMPONENT);
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
    int sequence = 1;
    iprop = icalcomponent_get_first_property(ievt,ICAL_SEQUENCE_PROPERTY);
    if(iprop)
        sequence = icalproperty_get_sequence(iprop);
    else
        CALI_WARN(0,"UID:%s missing VEVENT::SEQUENCE property",uid);

    // dtstart + tzid (if any)
    icaltimetype dtstart = icalcomponent_get_dtstart(ievt);
    if(icaltime_is_null_time(dtstart))
    {
      CALI_WARN(0,"UID:%s missing VEVENT::DTSTART property",uid);
      continue;
    }

    // all_day
    int all_day = dtstart.is_date;

    // dtend + tzid (if any)
    icaltimetype dtend = icalcomponent_get_dtend(ievt);
    if(icaltime_is_null_time(dtend))
    {
      CALI_WARN(0,"UID:%s missing VEVENT::DTEND property",uid);
      continue;
    }
    if(dtend.is_date)
      --dtend.day; // iCal allday events end the day after.

    // -- recurs --
    bool recurs = (
         icalcomponent_get_first_property(ievt,ICAL_RRULE_PROPERTY)!=NULL ||
         icalcomponent_get_first_property(ievt,ICAL_RDATE_PROPERTY)!=NULL
       );

    // Bind these values to the statements.
    sql::bind_int( CALI_HERE,db,insert_evt,1,version);
    sql::bind_int( CALI_HERE,db,insert_evt,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_evt,3,uid);
    sql::bind_text(CALI_HERE,db,insert_evt,4,summary);
    sql::bind_int( CALI_HERE,db,insert_evt,5,sequence);
    sql::bind_int( CALI_HERE,db,insert_evt,6,all_day);
    sql::bind_int( CALI_HERE,db,insert_evt,7,recurs);
    sql::bind_text(CALI_HERE,db,insert_evt,8,vevent);
    sql::step_reset(CALI_HERE,db,insert_evt);

    // Bind values common to all occurrences.
    sql::bind_int( CALI_HERE,db,insert_occ,1,version);
    sql::bind_int( CALI_HERE,db,insert_occ,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_occ,3,uid);
    // Generate occurrences.
    process_rrule(ievt,dtstart,dtend,db,insert_occ);
  }
  CALI_SQLCHK(db, ::sqlite3_exec(db, "commit", 0, 0, 0) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_evt) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_occ) );
  CALI_SQLCHK(db, ::sqlite3_finalize(insert_cal) );
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
  const char* tzid     = "Europe/London"; // ??

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

  // VTIMEZONE component
  icaltimezone* zone =icaltimezone_get_builtin_timezone(tzid);
  assert(zone);
  icalcomponent* vtimezone =
      icalcomponent_new_clone( icaltimezone_get_component(zone) );
  // Replace the libical TZID with the Olsen location.
  prop = icalcomponent_get_first_property(vtimezone,ICAL_TZID_PROPERTY);
  icalproperty_set_tzid(prop,tzid);
  icalcomponent_add_component(ical.get(),vtimezone);


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
          icalproperty_new_created( timet2ical(::time(NULL),false) ),
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
    prop = icalproperty_new_dtstamp( timet2ical(::time(NULL),false) );
    icalcomponent_add_property(vevent,prop);

    prop = icalproperty_new_dtstart( timet2ical(dtstart,allday,tzid) );
    param = icalparameter_new_tzid(tzid);
    icalproperty_add_parameter(prop,param);
    icalcomponent_add_property(vevent,prop);

    if(allday)
        dtend += 86400; // iCal allday events end the day after.
    prop = icalproperty_new_dtend( timet2ical(dtend,allday,tzid) );
    param = icalparameter_new_tzid(tzid);
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
