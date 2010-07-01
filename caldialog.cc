#include "caldialog.h"

#include "calendari.h"
#include "calendarlist.h"
#include "event.h"
#include "util.h"

#include <cassert>
#include <cstring>

namespace calendari {


// -- class CalDialog

CalDialog::CalDialog(
    calendari::Calendari*  app,
    GtkBuilder*            builder,
    const char*            root
  )
  : _app( *app ),
    _cl( *app->calendar_list ),
    _cal(NULL)
{
  std::string r = root;
  _dialog = GTK_DIALOG(
      gtk_builder_get_object(builder,(r+"dialog").c_str()));
  _name_entry = GTK_ENTRY(
      gtk_builder_get_object(builder,(r+"name_entry").c_str()));
  _colorbutton = GTK_COLOR_BUTTON(
      gtk_builder_get_object(builder,(r+"colorbutton").c_str()));
}


bool
CalDialog::setup(void)
{
  assert(_cal==NULL);
  if(!_cl.get_selected_iter(_it))
    return false;
  _cal = _cl.iter2cal(_it);
  assert(_cal);
  return bool(_cal);
}


void
CalDialog::run(void)
{
  if(!_cal && !setup())
      return;

  // set-up cal_dialog from calendar.
  gtk_entry_set_text(_name_entry,_cal->name().c_str());
  GdkColor col;
  gdk_color_parse(_cal->colour().c_str(),&col);
  gtk_color_button_set_color(_colorbutton,&col);

  // Show dialogue box.
  int response = gtk_dialog_run(_dialog);
  switch(response)
  {
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CANCEL:
      gtk_widget_hide(GTK_WIDGET(_dialog));
      break;
    default:
      break;
  }
  _cal = NULL;
}


void
CalDialog::entry_cb(GtkEntry* entry)
{
  assert(_cal);
  assert(entry==_name_entry);
  const char* newval = gtk_entry_get_text(entry);
  if(_cal->name()!=newval && ::strlen(newval))
  {
    _cal->set_name( newval );
    GValue gv;
    ::memset(&gv,0,sizeof(GValue));
    g_value_init(&gv,G_TYPE_STRING);
    g_value_set_string(&gv,newval);
    gtk_list_store_set_value(_cl.liststore_cal,&_it,2,&gv);
  }
}


void
CalDialog::color_set_cb(GtkColorButton* cb)
{
  assert(_cal);
  assert(cb==_colorbutton);
  GdkColor col;
  gtk_color_button_get_color(cb,&col);
  g_scoped<gchar> c( gdk_color_to_string(&col) );
  if(_cal->colour()!=c.get() && ::strlen(c.get()))
  {
    _cal->set_colour(c.get());
    GdkPixbuf* pixbuf = new_pixbuf_from_col(col,12,12);
    gtk_list_store_set(_cl.liststore_cal,&_it, 3,c.get(), 5,pixbuf, -1);
    g_object_unref(G_OBJECT(pixbuf));
    _app.queue_main_redraw();
  }
}


// -- class CalSubDialog

CalSubDialog::CalSubDialog(Calendari* app, GtkBuilder* builder)
  : CalDialog(app,builder,"cal_sub_")
{
  _filechooserbutton = GTK_FILE_CHOOSER_BUTTON(
      gtk_builder_get_object(builder,"cal_sub_filechooserbutton"));
}


void
CalSubDialog::run(void)
{
  if(!_cal && !setup())
      return;
  (void)gtk_file_chooser_set_filename(
      GTK_FILE_CHOOSER(_filechooserbutton),
      _cal->path().c_str()
    );
  CalDialog::run();
}


void
CalSubDialog::file_set_cb(GtkFileChooserButton* fc)
{
  assert(_cal);
  assert(fc==_filechooserbutton);
  g_scoped<gchar> p( gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc)) );
  if(_cal->path()!=p.get() && ::strlen(p.get()))
      _cal->set_path(p.get());
}


// -- class CalPubDialog

CalPubDialog::CalPubDialog(Calendari* app, GtkBuilder* builder)
  : CalDialog(app,builder,"cal_pub_")
{
  _private_togglebutton = GTK_TOGGLE_BUTTON(
      gtk_builder_get_object(builder,"cal_private_radiobutton"));
  _public_togglebutton = GTK_TOGGLE_BUTTON(
      gtk_builder_get_object(builder,"cal_public_radiobutton"));
  _path_entry = GTK_ENTRY(
      gtk_builder_get_object(builder,"cal_path_entry"));
  _browse_button = GTK_BUTTON(
      gtk_builder_get_object(builder,"cal_browse_button"));
}


void
CalPubDialog::run(void)
{
  if(!_cal && !setup())
      return;
  const std::string& path( _cal->path() );
  bool is_public = !path.empty();
  gtk_widget_set_sensitive(GTK_WIDGET(_path_entry),   is_public);
  gtk_widget_set_sensitive(GTK_WIDGET(_browse_button),is_public);
  if(is_public)
  {
    gtk_entry_set_text(_path_entry,path.c_str());
    gtk_entry_set_position(_path_entry,path.size());
    gtk_toggle_button_set_active(_public_togglebutton,true);
  }
  else
  {
    gtk_entry_set_text(_path_entry,"");
    gtk_toggle_button_set_active(_private_togglebutton,true);
  }
  CalDialog::run();
}



void
CalPubDialog::entry_cb(GtkEntry* entry)
{
  assert(_cal);
  if(entry==_name_entry)
  {
    CalDialog::entry_cb(entry);
    return;
  }
  assert(entry==_path_entry);
  _cal->set_path( gtk_entry_get_text(entry) );
}


void
CalPubDialog::activated(GtkToggleButton* tb)
{
  assert(_cal);
  gtk_widget_set_sensitive(GTK_WIDGET(_path_entry),   tb==_public_togglebutton);
  gtk_widget_set_sensitive(GTK_WIDGET(_browse_button),tb==_public_togglebutton);
  if(tb==_private_togglebutton)
      _cal->set_path(""); // private
  else if(tb==_public_togglebutton)
      _cal->set_path( gtk_entry_get_text(_path_entry) ); // public
}


void
CalPubDialog::button_clicked_cb(GtkButton*)
{
  assert(_cal);
  std::string filename = _cal->path();
  // Pop up a file chooser dialogue -> sets filename.
  // ?? Very similar to Calendari::subscribe_calendar() - fix duplication
  {
    GtkWidget* dialog =
      gtk_file_chooser_dialog_new(
          "Publish",
          GTK_WINDOW(_app.window),
          GTK_FILE_CHOOSER_ACTION_SAVE,
          GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
          GTK_STOCK_SAVE,  GTK_RESPONSE_ACCEPT,
          NULL
        );
    gtk_file_chooser_set_do_overwrite_confirmation(
        GTK_FILE_CHOOSER(dialog),
        true
      );
    if(filename.empty())
    {
      gtk_file_chooser_set_current_name(
          GTK_FILE_CHOOSER(dialog),
          (_cal->name() + ".ics").c_str()
        );
    }
    else
    {
      gtk_file_chooser_set_filename(
          GTK_FILE_CHOOSER(dialog),
          filename.c_str()
        );
    }
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter,"Calendars");
    gtk_file_filter_add_mime_type(filter,"text/calendar");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter,"All files");
    gtk_file_filter_add_pattern(filter,"*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),filter);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
      char* f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      filename = f;
      g_free(f);
    }
    gtk_widget_destroy(dialog);
  }
  if(filename.empty())
      return;
  printf("Publish to %s\n",filename.c_str());
  _cal->set_path( filename.c_str() );
  gtk_entry_set_text(_path_entry,filename.c_str());
  gtk_entry_set_position(_path_entry,filename.size());
}


} // end namespace calendari
