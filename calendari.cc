/*
 * Compile me with:
 *   gcc -o tut tut.c $(pkg-config --cflags --libs gtk+-2.0 gmodule-2.0)
 */
 
#include <gtk/gtk.h>
#include <gmodule.h>
#include <stdio.h>

struct CaliData
{
    // Widgets
    GtkWidget*  window;  ///< Main application window
};


G_MODULE_EXPORT void
cali_cb_clicked(
    GtkButton *button,
    gpointer   data )
{
    printf("Hello world.\n");
}


extern "C"
{
  G_MODULE_EXPORT gboolean
  cali_cb_expose_detail(
      GtkWidget*       widget,
      GdkEventExpose*  event,
      CaliData*          data
    );
}

G_MODULE_EXPORT gboolean
cali_cb_expose_detail(
    GtkWidget*       widget,
    GdkEventExpose*  event,
    CaliData*          data
  )
{
    printf("Hello world.\n");
    return true;
}


int
main(int argc, char* argv[])
{
    GtkBuilder*  builder;
    GError*  error = NULL;
 
    /* Init GTK+ */
    gtk_init( &argc, &argv );
 
    /* Create new GtkBuilder object */
    builder = gtk_builder_new();
    /* Load UI from file. If error occurs, report it and quit application.
     * Replace "tut.glade" with your saved project. */
    if( ! gtk_builder_add_from_file( builder, "calendari.glade", &error ) )
    {
        g_warning( "%s", error->message );
        g_free( error );
        return 1;
    }
 
    CaliData*  cal = g_slice_new( CaliData );
 
    /* Get main window pointer from UI */
    cal->window = GTK_WIDGET(gtk_builder_get_object(builder,"cali_window"));
 
    /* Connect signals */
    gtk_builder_connect_signals( builder, cal );
 
    /* Destroy builder, since we don't need it anymore */
    g_object_unref( G_OBJECT( builder ) );
 
    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( cal->window );
 
    /* Start main loop */
    gtk_main();
 
    return 0;
}
