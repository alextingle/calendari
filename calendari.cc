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

  // Connect signals
  gtk_builder_connect_signals(builder,this);
}


void
Calendari::select(Occurrence* occ)
{
  occurrence = occ;
  main_view->select( occ );
  detail_view->select( occ );
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
