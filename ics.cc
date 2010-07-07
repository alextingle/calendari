#include "ics.h"

#include "calendari.h"
#include "db.h"
#include "err.h"
#include "event.h"
#include "util.h"
#include "queue.h"
#include "sql.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <error.h>
#include <fstream>
#include <iostream>
#include <libical/ical.h>
#include <set>
#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sysexits.h>
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



/** Helper class used by functions that read calendars. */
class CalendarReader
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
  CalendarReader(const char* ical_filename);
  ~CalendarReader(void);

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
  CalendarReader(CalendarReader&);
  CalendarReader& operator = (CalendarReader&);
};


CalendarReader::CalendarReader(const char* ical_filename)
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


CalendarReader::~CalendarReader(void)
{
  if(_ical)
      icalcomponent_free(_ical);
}


bool
CalendarReader::calid_is_unique(Db& db) const
{
  int calid_count;
  sql::query_val(CALI_HERE,db,calid_count,
      "select count(0) from CALENDAR where CALID='%s'",
      sql::quote(calid).c_str()
    );
  return( calid_count == 0 );
}


void
CalendarReader::discard_ids(void)
{
  calid = uuids();
  _discard_ids = true;
}


int
CalendarReader::load(
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

    // -- recurs --
    bool recurs = (
         icalcomponent_get_first_property(ievt,ICAL_RRULE_PROPERTY)!=NULL ||
         icalcomponent_get_first_property(ievt,ICAL_RDATE_PROPERTY)!=NULL
       );

    // Bind these values to the statements.
    sql::bind_int( CALI_HERE,db,insert_evt,1,version);
    sql::bind_int( CALI_HERE,db,insert_evt,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_evt,3,uid.c_str());
    sql::bind_text(CALI_HERE,db,insert_evt,4,summary);
    sql::bind_int( CALI_HERE,db,insert_evt,5,sequence);
    sql::bind_int( CALI_HERE,db,insert_evt,6,all_day);
    sql::bind_int( CALI_HERE,db,insert_evt,7,recurs);
    sql::bind_text(CALI_HERE,db,insert_evt,8,vevent);
    sql::step_reset(CALI_HERE,db,insert_evt);

    // Bind values common to all occurrences.
    sql::bind_int( CALI_HERE,db,insert_occ,1,version);
    sql::bind_int( CALI_HERE,db,insert_occ,2,calnum);
    sql::bind_text(CALI_HERE,db,insert_occ,3,uid.c_str());
    // Generate occurrences.
    process_rrule(ievt,dtstart,dtend,db,insert_occ);
  }
  CALI_SQLCHK(db, ::sqlite3_exec(db, "commit", 0, 0, 0) );
  return calnum;
}


// -- public --

std::string generate_uid(void)
{
  // Format buf as <UUID>-cali@<hostname>
  char buf[256];
  char* s = uuidp(buf,sizeof(buf));
  s = ::stpcpy(s,"-cali@");
  ::gethostname(s, sizeof(buf) - (s-buf));
  return buf;
}


int subscribe(
    Calendari*   app,
    const char*  ical_filename,
    Db&          db,
    int          version
  )
{
  CalendarReader reader(ical_filename);
  reader.readonly = true;
  if(!reader.calid_is_unique(db))
  {
    // We need to preserve the calid (subscribing), so we can't proceed.
    CALI_WARN(0,"Calendar in file %s is already loaded.",ical_filename);
    return -1;
  }
  return reader.load(app, db, version);
}


int import(
    Calendari*   app,
    const char*  ical_filename,
    Db&          db,
    int          version
  )
{
  CalendarReader reader(ical_filename);
  if(reader.calid_is_unique(db))
  {
    if(reader.readonly)
        reader.path = "";
  }
  else
  {
     // We don't care about preserving the calid (importing),
     // so just make up a new one.
     reader.discard_ids();
     printf("Generated new calendar ID: %s\n",reader.calid.c_str());
     reader.path = "";
  }
  reader.readonly = false;
  return reader.load(app, db, version);
}


int reread(
    Calendari*   app,
    const char*  ical_filename,
    Db&          db,
    const char*  reread_calid,
    int          version
  )
{
  CalendarReader reader(ical_filename);
  reader.readonly = true;
  // We are re-reading an existing calendar - calids must match.
  if(reader.calid != reread_calid)
  {
    CALI_WARN(0,"File %s does not match subscription.",ical_filename);
    CALI_SQLCHK(db, ::sqlite3_exec(db, "rollback", 0, 0, 0) );
    return -1; // FAIL
  }
  return reader.load(app, db, version);
}


void write(const char* ical_filename, Db& db, const char* calid, int version)
{
  assert(calid);
  assert(calid[0]);

  icalproperty* prop;
  icalparameter* param;
  Queue& q( Queue::inst() );

  // Make sure the database is up-to-date before we start.
  q.flush();

  // Load calendar from database.
  const char* sql =
      "select CALNUM,CALNAME,DTSTAMP,PATH,READONLY "
      "from CALENDAR "
      "where VERSION=? and CALID=? ";
  sql::Statement select_cal(CALI_HERE,db,sql);
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
  time_t      dtstamp  =         ::sqlite3_column_int( select_cal,2);
  const char* path     = safestr(::sqlite3_column_text(select_cal,3));
  bool        readonly =         ::sqlite3_column_int( select_cal,4);
  const char* tzid     = system_timezone();

  // Check that we are allowed to write this calendar.
  if(readonly && 0==::strcmp(ical_filename,path)) // ?? use proper path compare
  {
    util::error(CALI_HERE,0,0,"Calendar is read only: %s",path);
    return;
  }
  if(dtstamp==0)
  {
      q.pushf(
          "update CALENDAR set DTSTAMP=%lu where VERSION=%d and CALNUM=%d",
          ::time(NULL),version,calnum
        );
  }
  else
  {
    struct stat st;
    if(0==::stat(ical_filename,&st))
    {
      // Don't overwrite a file that's newer than the calendar's date stamp.
      // Allow a margin for files on badly synced NFS shares.
      if(st.st_mtime > (dtstamp+10))
          return;
    }
    else if(errno!=ENOENT)
    {
      ::error(0,errno,"Failed to stat output iCalendar file %s",ical_filename);
      return;
    }
  }

  printf("write %s at %s\n",calname,ical_filename);

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
  // system timezone => X-WR-TIMEZONE
  prop = icalproperty_new_x( tzid );
  icalproperty_set_x_name(prop,"X-WR-TIMEZONE");
  icalcomponent_add_property(ical.get(),prop);

  // VTIMEZONE component
  icaltimezone* zone =icaltimezone_get_builtin_timezone(tzid);
  if(!zone)
      ::error(EX_OSFILE,0,"System timezone is unrecognised: %s",tzid);
  icalcomponent* vtimezone =
      icalcomponent_new_clone( icaltimezone_get_component(zone) );
  // Replace the libical TZID with the Olsen location.
  prop = icalcomponent_get_first_property(vtimezone,ICAL_TZID_PROPERTY);
  icalproperty_set_tzid(prop,tzid);
  icalcomponent_add_component(ical.get(),vtimezone);


  // Read in VEVENTS from the database...
  // Load calendar from database.
  // Eek! a self-join to find the *first* occurrence for each event.
  sql = "select E.UID,SUMMARY,SEQUENCE,ALLDAY,VEVENT,O.DTSTART,O.DTEND "
        "from (select UID,min(DTSTART) as S from "
               "OCCURRENCE where VERSION=? and CALNUM=? group by UID) K "
        "left join OCCURRENCE O on K.UID=O.UID and K.S=O.DTSTART "
        "left join EVENT E on E.UID=O.UID and E.VERSION=O.VERSION "
        "where E.VERSION=? and E.CALNUM=? "
        "order by O.DTSTART";
  sql::Statement select_evt(CALI_HERE,db,sql);
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

    int old_sequence = -1;
    icalcomponent* vevent;
    if(veventz && veventz[0])
    {
      vevent = icalparser_parse_string(veventz);

      // Find the old sequence number (if any).
      prop = icalcomponent_get_first_property(vevent,ICAL_SEQUENCE_PROPERTY);
      if(prop)
          old_sequence = icalproperty_get_sequence(prop);

      // Eliminate parts that we already have.
      prop = icalcomponent_get_first_property(vevent,ICAL_ANY_PROPERTY);
      while(prop)
      {
        icalproperty* next =
            icalcomponent_get_next_property(vevent,ICAL_ANY_PROPERTY);
        const char* name = icalproperty_get_property_name(prop);
        if( 0==::strcmp(name,"DTSTART") ||
            0==::strcmp(name,"DTEND") ||
           (0==::strcmp(name,"DTSTAMP") && sequence>old_sequence) ||
            0==::strcmp(name,"SUMMARY") ||
            0==::strcmp(name,"SEQUENCE") ||
            0==::strcmp(name,"X-LIC-ERROR") )
        {
          icalcomponent_remove_property(vevent,prop);
          icalproperty_free(prop);
        }
        prop = next;
      }

    }
    else
    {
      vevent = make_new_vevent(uid);
    }
    icalcomponent_add_property(vevent,
        icalproperty_new_summary( summary )
      );
    icalcomponent_add_property(vevent,
        icalproperty_new_sequence( sequence )
      );
    if(sequence>old_sequence)
    {
      prop = icalproperty_new_dtstamp( timet2ical(::time(NULL),false) );
      icalcomponent_add_property(vevent,prop);
    }

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

    // If it's changed, write it back out to the database too.
    if(sequence>old_sequence)
    {
      q.pushf(
          "update EVENT set VEVENT='%s' where VERSION=%d and UID='%s'",
          sql::quote(icalcomponent_as_ical_string(vevent)).c_str(),
          version,
          sql::quote(uid).c_str()
        );
    }
  }
  
  // Flush any changes to the database.
  q.flush();

  // Write out the iCalendar file.
  std::ofstream ofile(ical_filename);
  if(ofile)
      ofile << icalcomponent_as_ical_string( ical.get() );
}


icalcomponent*
make_new_vevent(const char* uid)
{
  return icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
      icalproperty_new_uid(uid),
      icalproperty_new_created( timet2ical(::time(NULL),false) ),
      icalproperty_new_transp(ICAL_TRANSP_OPAQUE), //??
      0
    );
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
