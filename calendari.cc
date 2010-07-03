#include "calendari.h"

#include "calendarlist.h"
#include "db.h"
#include "detailview.h"
#include "dragdrop.h"
#include "err.h"
#include "ics.h"
#include "monthview.h"
#include "prefview.h"
#include "setting.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <iostream>
#include <memory>
#include <sysexits.h>
#include <uuid/uuid.h>

namespace calendari {


/** Helper for callbacks from GTK. */
inline Calendari& ptr2app(void* ptr)
{
  assert(ptr);
  return *static_cast<Calendari*>(ptr);
}


void
Calendari::load(const char* dbname)
{
  _selected_occurrence = NULL;
  if(db)
  {
    CALI_ERRO(1,0,"Database already loaded");
    return;
  }
  db = new Db(dbname);
  setting = new Setting(*this);
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
  statusbar =
    GTK_STATUSBAR(gtk_builder_get_object(builder,"cali_statusbar"));
  sidebar_vpaned =
    GTK_VPANED(gtk_builder_get_object(builder,"cali_sidebar_vpaned"));
  view_cals_menuitem = GTK_CHECK_MENU_ITEM(
    gtk_builder_get_object(builder,"cali_view_cals_menuitem"));
  about = GTK_DIALOG(gtk_builder_get_object(builder,"cali_aboutdialog"));

  // Menu status
  gtk_check_menu_item_set_active(view_cals_menuitem,setting->view_calendars());

  // View calendars?
  gtk_paned_set_position(
      GTK_PANED(sidebar_vpaned),
      (setting->view_calendars()? setting->cal_vpaned_pos(): 0 )
    );

  // Set-up focus order (exclude calendar list from focus chain).
  GtkWidget* detail_scolledwindow =
    GTK_WIDGET(gtk_builder_get_object(builder,"cali_detail_scrolledwindow"));
  GList* list = g_list_append(NULL,detail_scolledwindow);
  gtk_container_set_focus_chain(GTK_CONTAINER(sidebar_vpaned),list);
  g_list_free(list);

  // Set up drag and drop.
  gtk_drag_dest_set(
      main_drawingarea,
      (GtkDestDefaults)(GTK_DEST_DEFAULT_MOTION),//|GTK_DEST_DEFAULT_HIGHLIGHT),
      DragDrop::target_list_dest,
      DragDrop::target_list_dest_len,
      GDK_ACTION_COPY
    );

  // Set up clipboard.
  GdkAtom atom = gdk_atom_intern_static_string("CLIPBOARD");
  clipboard = gtk_clipboard_get(atom);

  main_drawingarea_redraw_queued = false;

  calendar_list = new CalendarList();
  calendar_list->build(this,builder);

  detail_view = new DetailView(*this);
  detail_view->build(this,builder);
  
  main_view = new MonthView(*this);
  main_view->set(::time(NULL));
  gtk_widget_grab_focus(main_drawingarea);

  pref_view = new PrefView(*this);
  pref_view->build(builder);

  // Connect signals
  gtk_builder_connect_signals(builder,this);
}


void
Calendari::queue_main_redraw(bool reload)
{
  if(reload)
  {
    main_view->reload();
  }
  if(!main_drawingarea_redraw_queued)
  {
    main_drawingarea_redraw_queued = true;
    gtk_widget_queue_draw(GTK_WIDGET(main_drawingarea));
  }
}


void
Calendari::select(Occurrence* occ)
{
  if(occ == cut()) // Can't select a cut occurrence.
      occ = NULL;
  _selected_occurrence = occ;
  main_view->select( occ );
  detail_view->select( occ );
  calendar_list->select( occ );
}


void
Calendari::moved(Occurrence* occ)
{
  db->moved( occ );
  main_view->moved( occ );
  if(occ==this->_selected_occurrence) // selected?
      detail_view->moved( occ );
}


void
Calendari::create_calendar(void)
{
  if(_selected_occurrence)
  {
    select(NULL);
    queue_main_redraw();
  }
  char calid[64];
  uuid_t uu;
  uuid_generate(uu);
  uuid_unparse(uu,calid);

  Calendar* new_cal =
    db->create_calendar(
      calid,
      "New Calendar", // calname
      "",        // path
      false,     // readonly
      "#0000aa", // colour ?? choose a different colour here.
      true       // show
    );
  calendar_list->add_calendar(*new_cal);
  calendar_list->select( new_cal->position(), true );
}


void
Calendari::subscribe_calendar(void)
{
  std::string filename = "";
  // Pop up a file chooser dialogue -> sets filename.
  {
    GtkWidget* dialog =
      gtk_file_chooser_dialog_new ("Subscribe",
          GTK_WINDOW(window),
          GTK_FILE_CHOOSER_ACTION_OPEN,
          GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,
          GTK_STOCK_OPEN,  GTK_RESPONSE_ACCEPT,
          NULL
        );
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
  printf("Subscribe to %s\n",filename.c_str());
  int calnum = ics::read(this,filename.c_str(),*db);
  if(calnum<0)
      return;
  Calendar* new_cal = db->load_calendar(calnum);
  assert(new_cal);
  if(_selected_occurrence)
      select(NULL);
  calendar_list->add_calendar(*new_cal);
  main_view->reload();
  queue_main_redraw();
  calendar_list->select( new_cal->position(), true );
}


void
Calendari::delete_selected_calendar(void)
{
  Calendar* cal = calendar_list->current();
  if(!cal)
      return;

  // First confirm that the use really wants to delete the whole calendar.
  GtkDialog* dialog =
    GTK_DIALOG(gtk_message_dialog_new(
        GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_NONE,
        "Delete calendar '%s' and all of its events?",cal->name().c_str()
      ));
  // Add "Keep" button.
  GtkButton* keep_btn = GTK_BUTTON(gtk_button_new_with_label("_Keep"));
  gtk_button_set_use_underline(keep_btn,true);
  gtk_button_set_image(keep_btn,
      gtk_image_new_from_stock("gtk-ok",GTK_ICON_SIZE_MENU));
  gtk_dialog_add_action_widget(dialog,GTK_WIDGET(keep_btn),GTK_RESPONSE_CANCEL);
  gtk_widget_show(GTK_WIDGET(keep_btn));
  // Add "Delete" button
  gtk_dialog_add_button(dialog,"gtk-delete",GTK_RESPONSE_ACCEPT);
  // Run the dialogue.
  int response = gtk_dialog_run(dialog);
  gtk_widget_destroy(GTK_WIDGET(dialog));
  if(GTK_RESPONSE_ACCEPT != response)
      return;

  // OK, let the blood flow...
  // Start by clearing the selection, if necessary.
  if(_clipboard_occurrence &&
     _clipboard_occurrence->event.calendar().calid == cal->calid)
  {
    _clipboard_occurrence = NULL;
  }
  if(_selected_occurrence &&
     _selected_occurrence->event.calendar().calid == cal->calid)
  {
    select(NULL);
  }
  // Remove the Calendar from the cal-list GUI.
  if( !calendar_list->remove_selected_calendar() )
      return; // Bail out if it's somehow not there.
  // Clear the database and destroy the Calendar/Event/Occurrence objects.
  db->erase_calendar(cal);
  main_view->reload();
  queue_main_redraw();
}


Occurrence*
Calendari::create_event(time_t dtstart, time_t dtend, Event* old)
{
  Calendar* calendar = calendar_list->find_writeable();
  if(!calendar)
      return NULL;
  assert(!calendar->readonly());
  // Format buf as <UUID>-cali@<hostname>
  const size_t uuid_strlen = 36;
  char buf[256];
  uuid_t uu;
  uuid_generate(uu);
  uuid_unparse(uu,buf);
  char* s = ::stpcpy(buf+uuid_strlen,"-cali@");
  ::gethostname(s, sizeof(buf) - (s-buf));
  // Make the new occurrence.
  Occurrence* occ = db->create_event(
      buf, //  uid,
      dtstart,
      dtend,
      (old? old->summary().c_str(): "New Event"), // summary,
      (old? old->all_day(): false), // all_day,
      calendar->calnum
    );
  moved(occ);
  select(occ);
  return occ;
}


void
Calendari::erase_selected(void)
{
  if(_selected_occurrence && !_selected_occurrence->event.readonly())
  {
    Occurrence* old_selected_occ = NULL;
    std::swap(old_selected_occ,_selected_occurrence);
    if(_clipboard_occurrence == old_selected_occ)
    {
      // forget clipboard
      _clipboard_occurrence = NULL;
      if(_clipboard_cut)
          queue_main_redraw();
    }
    main_view->erase(old_selected_occ);
    if(!_selected_occurrence) // main_view->erase() may have moved the selection.
        detail_view->select(NULL);
    db->erase(old_selected_occ);
  }
}


void
Calendari::cut_clipboard(void)
{
  if(_selected_occurrence && !_selected_occurrence->event.readonly())
  {
    bool ok =
      gtk_clipboard_set_with_data(
          clipboard,
          DragDrop::target_list_src, // const GtkTargetEntry *targets,
          DragDrop::target_list_src_len, // guint n_targets,
          (GtkClipboardGetFunc)clipboard_get, // GtkClipboardGetFunc,
          (GtkClipboardClearFunc)clipboard_clear, // GtkClipboardClearFunc,
          static_cast<gpointer>(this) // user_data
        );
    if(ok)
    {
      _clipboard_occurrence = _selected_occurrence;
      _clipboard_cut = true;
      select(NULL);
      queue_main_redraw();
    }
  }
}


void
Calendari::copy_clipboard(void)
{
  if(_selected_occurrence && !_selected_occurrence->event.readonly())
  {
    bool ok =
      gtk_clipboard_set_with_data(
          clipboard,
          DragDrop::target_list_src, // const GtkTargetEntry *targets,
          DragDrop::target_list_src_len, // guint n_targets,
          (GtkClipboardGetFunc)clipboard_get, // GtkClipboardGetFunc,
          (GtkClipboardClearFunc)clipboard_clear, // GtkClipboardClearFunc,
          static_cast<gpointer>(this) // user_data
        );
    if(ok)
    {
      _clipboard_occurrence = _selected_occurrence;
      _clipboard_cut = false;
    }
  }
}


void
Calendari::paste_clipboard(void)
{
  // ?? Support additional target types.
  if(_clipboard_occurrence)
  {
    GdkAtom target_type = gdk_atom_intern_static_string("OCCURRENCE");
    GtkSelectionData* data =
        gtk_clipboard_wait_for_contents(clipboard,target_type);
    if(data)
    {
      // Just check that we have the data
      if( *(int*)data->data )
      {
        if(_clipboard_cut)
        {
          // Move it.
          _clipboard_cut = false;
          main_view->move_here(_clipboard_occurrence);
        }
        else
        {
          // Copy it.
          main_view->copy_here(_clipboard_occurrence);
        }
        queue_main_redraw();
      }
      gtk_selection_data_free(data);
    }
  }
}


void
Calendari::clipboard_get(
    GtkClipboard*      cb,
    GtkSelectionData*  data,
    guint              info,
    gpointer           user_data
  )
{
  Calendari& app( ptr2app(user_data) );
  if(cb!=app.clipboard || !app._clipboard_occurrence)
      return;
  switch(static_cast<DragDrop::type>(info))
  {
  case DragDrop::DD_OCCURRENCE:
      {
        // Dummy data to pass back.
        static const int dd_occurrence_ok = 1;
        gtk_selection_data_set(
          data,
          data->target,
          8,                         // number of bits per 'unit'
          (guchar*)&dd_occurrence_ok,// pointer to data to be sent
          sizeof(dd_occurrence_ok)   // length of data in units
        );
      }
      break;
  case DragDrop::DD_STRING:
      {
        const std::string& summary( app._clipboard_occurrence->event.summary() );
        gtk_selection_data_set(
          data,
          data->target,
          8,                       // number of bits per 'unit'
          (guchar*)summary.c_str(),// pointer to data to be sent
          summary.size()           // length of data in units
        );
      }
      break;
  }
}


void
Calendari::clipboard_clear(GtkClipboard* cb, gpointer user_data)
{
  printf("%s\n",__PRETTY_FUNCTION__);
  Calendari& app( ptr2app(user_data) );
  if(cb==app.clipboard)
  {
    if(app._clipboard_cut)
    {
      // The cut occurrence has not been pasted anywhere, so replace it.
      app.queue_main_redraw();
    }
    app._clipboard_occurrence = NULL;
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
  std::auto_ptr<calendari::Calendari> app( new calendari::Calendari() );
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
          app->debug = true;
          break;
      case 'f':
          app->load(optarg);
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
  if(!app->db)
  {
    std::string dbname ="cali.sqlite";
    const char* home = ::getenv("HOME");
    if(home)
        dbname = home + ("/." + dbname);
    app->load(dbname.c_str());
  }

  // Import mode.
  if(import)
  {
    assert(app->db);
    calendari::ics::read(app.get(),import,*app->db);
    return 0;
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
  app->build(builder);

  // Destroy builder, since we don't need it anymore
  g_object_unref( G_OBJECT(builder) );

  // Show window. All other widgets are automatically shown by GtkBuilder
  gtk_widget_show( app->window );

  // Start main loop
  gtk_main();
  
  // Save settings before we quit.
  app->setting->save();

  return 0;
}
