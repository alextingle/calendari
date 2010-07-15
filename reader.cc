#include "reader.h"

#include "calendari.h"
#include "db.h"
#include "err.h"
#include "ics.h"
#include "recur.h"
#include "util.h"
#include "sql.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <libical/ical.h>
#include <set>
#include <sqlite3.h>
#include <sys/types.h>
#include <sys/stat.h>

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


/** Based on source from libical.
*   Returns the recurrance type for this event. */
RecurType process_rrule(
    icalcomponent*  ievt,
    icaltimetype&   dtstart,
    icaltimetype&   dtend,
    sqlite3*        db,
    sqlite3_stmt*   insert_occ
  )
{
  RecurType result = RECUR_NONE;

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
        result = add_recurrence(result,recur.freq);
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
      result = RECUR_CUSTOM;
      make_occurrence(rdate_period.time, duration, db,insert_occ);
    }
  }
  return result;
}


// -- class Reader --

Reader::Reader(const char* ical_filename)
  : _ical(NULL),
    _ical_filename(ical_filename),
    _discard_ids(false),
    calid(),
    calname(),
    path(ical_filename),
    readonly( 0!= ::access(ical_filename,W_OK) )
{
  assert(!_ical_filename.empty());
  // Parse the iCalendar file.
  SParser iparser( ::icalparser_new() );
  FILE* stream = ::fopen(ical_filename,"r");
  if(!stream)
  {
    CALI_ERRO(0,errno,"failed to open calendar file %s",ical_filename);
    return; // ?? throw
  }
  ::icalparser_set_gen_data(iparser.get(),stream);
  SComponent ical( ::icalparser_parse(iparser.get(),read_stream) );
  ::fclose(stream);
  if(!ical)
  {
    CALI_ERRO(0,0,"failed to read calendar file %s",ical_filename);
    return; // ?? throw
  }

  // Initialise calid & calname.
  icalproperty* iprop =
      icalcomponent_get_first_property(ical.get(),ICAL_X_PROPERTY);
  while(iprop && (calid.empty() || calname.empty()))
  {
    const char* x_name = icalproperty_get_x_name(iprop);
    if(0==::strcmp("X-WR-CALNAME",x_name))
        calname = icalproperty_get_x(iprop);
    else if(0==::strcmp("X-WR-RELCALID",x_name))
        calid = icalproperty_get_x(iprop);
    iprop = icalcomponent_get_next_property(ical.get(),ICAL_X_PROPERTY);
  }
  if(calid.empty() || calname.empty())
  {
    // Fall back to using the file name.
    char* p = ::strdup(_ical_filename.c_str()); //     E.g. "path/to/mycal.ics"
    std::string bname = ::basename(p);
    ::free(p);
    if(calid.empty())
        calid = bname; //                              E.g. "mycal.ics"
    if(calname.empty())
    {
      std::string::size_type pos = bname.find_last_of(".");
      if(pos==std::string::npos || pos==0)
          calname = bname;
      else
          calname = bname.substr(0,pos); //            E.g. "mycal"
    }
  }
  _ical = ical.release();
}


Reader::~Reader(void)
{
  if(_ical)
      icalcomponent_free(_ical);
}


bool
Reader::calid_is_unique(Db& db) const
{
  int calid_count;
  sql::query_val(CALI_HERE,db,calid_count,
      "select count(0) from CALENDAR where CALID='%s'",
      sql::quote(calid).c_str()
    );
  return( calid_count == 0 );
}


void
Reader::discard_ids(void)
{
  calid = uuids();
  _discard_ids = true;
}


int
Reader::load(
    Calendari*   app,
    Db&          db,
    int          version
  )
{
  // Prepare the insert statements.
  const char* sql =
      "insert into CALENDAR "
        "(VERSION,CALNUM,CALID,CALNAME,PATH,READONLY,POSITION,COLOUR,SHOW) "
        "values (?,?,?,?,?,?,?,?,1)";
  sql::Statement insert_cal(CALI_HERE,db,sql);

  sql="insert into EVENT "
        "(VERSION,CALNUM,UID,SUMMARY,SEQUENCE,ALLDAY,RECURS,VEVENT) "
        "values (?,?,?,?,?,?,?,?)";
  sql::Statement insert_evt(CALI_HERE,db,sql);

  sql="insert into OCCURRENCE "
        "(VERSION,CALNUM,UID,DTSTART,DTEND) values (?,?,?,?,?)";
  sql::Statement insert_occ(CALI_HERE,db,sql);

  CALI_SQLCHK(db, ::sqlite3_exec(db, "begin", 0, 0, 0) );

  // Get the calnum.
  int calnum = db.calnum(calid.c_str());
  assert(calnum);
  // Choose a colour.
  const char* colour =colours[ calnum % (sizeof(colours)/sizeof(char*)) ];
  // Bind these values to the statements.
  sql::bind_int( CALI_HERE,db,insert_cal,1,version);
  sql::bind_int( CALI_HERE,db,insert_cal,2,calnum);
  sql::bind_text(CALI_HERE,db,insert_cal,3,calid.c_str());
  sql::bind_text(CALI_HERE,db,insert_cal,4,calname.c_str());
  sql::bind_text(CALI_HERE,db,insert_cal,5,path.c_str());
  sql::bind_int( CALI_HERE,db,insert_cal,6,readonly);
  sql::bind_int( CALI_HERE,db,insert_cal,7,-1); // position
  sql::bind_text(CALI_HERE,db,insert_cal,8,colour);
  sql::step_reset(CALI_HERE,db,insert_cal);

  // Remember events' UIDs, so that we can reject duplicates.
  std::set<std::string> uids_seen;

  // Iterate through all components (VEVENTs).
  for(icalcompiter e=icalcomponent_begin_component(_ical,ICAL_VEVENT_COMPONENT);
      icalcompiter_deref(&e)!=NULL;
      icalcompiter_next(&e))
  {
    icalproperty* iprop;
    icalcomponent* ievt = ::icalcompiter_deref(&e);
    const char* vevent = ::icalcomponent_as_ical_string(ievt);

    // -- uid --
    std::string uid;
    if(_discard_ids)
    {
      uid = generate_uid();
    }
    else
    {
      iprop = icalcomponent_get_first_property(ievt,ICAL_UID_PROPERTY);
      if(!iprop)
      {
        CALI_WARN(0,"missing VEVENT::UID property");
        continue;
      }
      uid = safestr( icalproperty_get_uid(iprop) );
      if(uid.empty())
      {
        CALI_WARN(0,"VEVENT::UID property has no value");
        continue;
      }
      if(!uids_seen.insert(uid).second)
      {
        if(!app || app->debug)
            CALI_WARN(0,"VEVENT::UID property not unique: %s",uid.c_str());
        continue;
      }
    }

    // -- summary --
    iprop = icalcomponent_get_first_property(ievt,ICAL_SUMMARY_PROPERTY);
    if(!iprop)
    {
      CALI_WARN(0,"UID:%s missing VEVENT::SUMMARY property",uid.c_str());
      continue;
    }
    const char* summary = icalproperty_get_summary(iprop);
    if(!summary)
    {
      CALI_WARN(0,"UID:%s VEVENT::SUMMARY property has no value",uid.c_str());
      continue;
    }

    // -- sequence --
    int sequence = 1;
    iprop = icalcomponent_get_first_property(ievt,ICAL_SEQUENCE_PROPERTY);
    if(iprop)
        sequence = icalproperty_get_sequence(iprop);

    // dtstart + tzid (if any)
    icaltimetype dtstart = icalcomponent_get_dtstart(ievt);
    if(icaltime_is_null_time(dtstart))
    {
      CALI_WARN(0,"UID:%s missing VEVENT::DTSTART property",uid.c_str());
      continue;
    }

    // all_day
    int all_day = dtstart.is_date;

    // dtend + tzid (if any)
    icaltimetype dtend = icalcomponent_get_dtend(ievt);
    if(icaltime_is_null_time(dtend))
    {
      CALI_WARN(0,"UID:%s missing VEVENT::DTEND property",uid.c_str());
      continue;
    }
    if(dtend.is_date)
      --dtend.day; // iCal allday events end the day after.

    // Bind values common to all occurrences.
    sql::bind_int( CALI_HERE,db,insert_occ,1,version);
    sql::bind_int( CALI_HERE,db,insert_occ,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_occ,3,uid.c_str());
    // Generate occurrences.
    RecurType recurs = process_rrule(ievt,dtstart,dtend,db,insert_occ);

    // Make the EVENT row.
    // Note: Delay making the event until after we've processed the RRULEs,
    // makes live easier, atthe expense of allowing the DB to temporarily
    // contain OCCURRENCEs without a corresponding EVENT.
    sql::bind_int( CALI_HERE,db,insert_evt,1,version);
    sql::bind_int( CALI_HERE,db,insert_evt,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_evt,3,uid.c_str());
    sql::bind_text(CALI_HERE,db,insert_evt,4,summary);
    sql::bind_int( CALI_HERE,db,insert_evt,5,sequence);
    sql::bind_int( CALI_HERE,db,insert_evt,6,all_day);
    sql::bind_int( CALI_HERE,db,insert_evt,7,recur2int(recurs));
    sql::bind_text(CALI_HERE,db,insert_evt,8,vevent);
    sql::step_reset(CALI_HERE,db,insert_evt);
  }
  CALI_SQLCHK(db, ::sqlite3_exec(db, "commit", 0, 0, 0) );
  return calnum;
}


} } // end namespace calendari::ical
