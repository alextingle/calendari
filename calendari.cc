#include "calendari.h"

#include "calendarlist.h"
#include "db.h"
#include "detailview.h"
#include "err.h"
#include "ics.h"
#include "monthview.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <iostream>
#include <sysexits.h>

namespace calendari {


void
Calendari::load(const char* dbname)
{
  if(db)
  {
    CALI_ERRO(1,0,"Database already loaded");
    return;
  }
  db = new Db(dbname);
  db->load_calendars();
  _occurrence = NULL;
}


void
Calendari::build(GtkBuilder* builder)
{
  // Get main window pointer from UI
  window = GTK_WIDGET(gtk_builder_get_object(builder,"cali_window"));
  main_drawingarea =
    GTK_WIDGET(gtk_builder_get_object(builder,"cali_main_drawingarea"));
  main_label =
    GTK_LABEL(gtk_builder_get_object(builder,"main_label"));
  about = GTK_DIALOG(gtk_builder_get_object(builder,"cali_aboutdialog"));

  main_drawingarea_redraw_queued = false;

  calendar_list = new CalendarList();
  calendar_list->build(this,builder);

  detail_view = new DetailView(*this);
  detail_view->build(this,builder);
  
  main_view = new MonthView(*this);
  main_view->set(::time(NULL));
  gtk_widget_grab_focus(main_drawingarea);

  // Connect signals
  gtk_builder_connect_signals(builder,this);
}


void
Calendari::queue_main_redraw(void)
{
  if(!main_drawingarea_redraw_queued)
  {
    main_drawingarea_redraw_queued = true;
    gtk_widget_queue_draw(GTK_WIDGET(main_drawingarea));
  }
}


void
Calendari::select(Occurrence* occ)
{
  _occurrence = occ;
  main_view->select( occ );
  detail_view->select( occ );
  calendar_list->select( occ );
}


void
Calendari::moved(Occurrence* occ)
{
  db->moved( occ );
  main_view->moved( occ );
  if(occ==this->_occurrence) // selected?
      detail_view->moved( occ );
}


void
Calendari::create_event(time_t dtstart, time_t dtend)
{
  Calendar* calendar = calendar_list->current();
  if(calendar && !calendar->readonly())
  {
    char buf[256];
    int len =::snprintf(buf,sizeof(buf),"%ld-cali-%d@",::time(NULL),::getpid());
    ::gethostname(buf+len, sizeof(buf)-len);
    Occurrence* occ = db->create_event(
        buf, //  uid,
        dtstart,
        dtend,
        "New Event", // summary,
        false, // all_day,
        calendar->calnum
      );
    moved(occ);
    select(occ);
    gtk_window_set_focus(GTK_WINDOW(window),GTK_WIDGET(detail_view->title_entry));
  }
}


void
Calendari::erase_selected(void)
{
  if(_occurrence && !_occurrence->event.readonly())
  {
    main_view->erase(_occurrence);
    db->erase(_occurrence);
    _occurrence = NULL;
    detail_view->select( NULL );
  }
}


} // end namespace calendari


int
main(int argc, char* argv[])
{
  GtkBuilder*  builder;
  GError*  error = NULL;

  // Init GTK+
  gtk_init( &argc, &argv );
  gtk_rc_parse("dot.calrc");

  // Command-line options.
  calendari::Calendari* cal = g_slice_new( calendari::Calendari );
  const char* import = NULL;

  option long_options[] = {
      {"debug",  0, 0, 'd'},
      {"file",   1, 0, 'f'},
      {"help",   0, 0, 'h'},
      {"import", 1, 0, 'i'},
      {0, 0, 0, 0}
    };
  int opt;
  while((opt=::getopt_long(argc,argv,"d:f:hi:",long_options,NULL)) != -1)
  {
    switch(opt)
    {
      case 'd':
          cal->debug = true;
          break;
      case 'f':
          cal->load(optarg);
          break;
      case 'i':
          import = optarg;
          break;
      case 'h':
      default:
          std::cout<<"syntax: calendari [--file=DB] [--import=ICS]"<<std::endl;
          exit(EX_USAGE);
    }
  }

  // If not set explicitly, work out which database file to use.
  if(!cal->db)
  {
    std::string dbname ="cali.sqlite";
    const char* home = ::getenv("HOME");
    if(home)
        dbname = home + ("/." + dbname);
    cal->load(dbname.c_str());
  }

  // Import mode.
  if(import)
  {
    assert(cal->db);
    calendari::ics::read(import,*cal->db);
    ::exit(0);
  }

  // Create new GtkBuilder object
  builder = gtk_builder_new();

  // Load UI from file. If error occurs, report it and quit application.
  if(!gtk_builder_add_from_file(builder, "calendari.glade", &error) )
  {
      g_warning("%s", error->message);
      g_free(error);
      return 1;
  }
  cal->build(builder);

  // Destroy builder, since we don't need it anymore
  g_object_unref( G_OBJECT(builder) );

  // Show window. All other widgets are automatically shown by GtkBuilder
  gtk_widget_show( cal->window );

  // Start main loop
  gtk_main();

  return 0;
}
