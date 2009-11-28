#include "calendari.h"

#include "calendarlist.h"
#include "db.h"

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

  calendar_list = new CalendarList();
  calendar_list->build(this,builder);

  // Connect signals
  gtk_builder_connect_signals(builder,this);
}


} // end namespace calendari


int
main(int argc, char* argv[])
{
  GtkBuilder*  builder;
  GError*  error = NULL;

  // Init GTK+
  gtk_init( &argc, &argv );

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
