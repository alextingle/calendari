#include "calendari.h"

#include "calendarlist.h"
#include "detailview.h"
#include "db.h"
#include "monthview.h"

#include <gtk/gtk.h>
#include <gmodule.h>
#include <stdio.h>

namespace calendari {


void
Calendari::load(const char* dbname)
{
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
Calendari::select(Occurrence* occ)
{
  _occurrence = occ;
  main_view->select( occ );
  detail_view->select( occ );
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
  if(calendar)
  {
    char buf[256];
    ::snprintf(buf, sizeof(buf),"?? make uid %ld - %ld ??",dtstart,time(NULL));
    Occurrence* occ = db->make_occurrence(
        buf, //  uid,
        dtstart,
        dtend,
        "New Event", // summary,
        false, // all_day,
        calendar->calid.c_str() // calid
      );
    moved(occ);
    select(occ);
    gtk_window_set_focus(GTK_WINDOW(window),GTK_WIDGET(detail_view->title_entry));
  }
}


void
Calendari::erase_selected(void)
{
  if(_occurrence)
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

  // Create new GtkBuilder object
  builder = gtk_builder_new();

  // Load UI from file. If error occurs, report it and quit application.
  if(!gtk_builder_add_from_file(builder, "calendari.glade", &error) )
  {
      g_warning("%s", error->message);
      g_free(error);
      return 1;
  }

  calendari::Calendari* cal = g_slice_new( calendari::Calendari );
  cal->load(argv[1]);
  cal->build(builder);

  // Destroy builder, since we don't need it anymore
  g_object_unref( G_OBJECT(builder) );

  // Show window. All other widgets are automatically shown by GtkBuilder
  gtk_widget_show( cal->window );

  // Start main loop
  gtk_main();

  return 0;
}
