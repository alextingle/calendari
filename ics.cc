#include "ics.h"

#include "db.h"
#include "err.h"
#include "util.h"
#include "queue.h"
#include "reader.h"
#include "sql.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <error.h>
#include <fstream>
#include <iostream>
#include <libical/ical.h>
#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sysexits.h>

namespace calendari {
namespace ics {

typedef scoped<icalcomponent,icalcomponent_free> SComponent;


// -- private --

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
  Reader reader(ical_filename);
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
  Reader reader(ical_filename);
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
  Reader reader(ical_filename);
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
    calendari::ics::read(NULL,ics_filename,db);
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
