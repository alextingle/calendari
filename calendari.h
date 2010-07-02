#ifndef CALENDARI__CALENDARI_H
#define CALENDARI__CALENDARI_H 1

#include <gtk/gtk.h>

#define FORMAT_DATE "%Y-%m-%d"
#define FORMAT_TIME " %H:%M"

namespace calendari {

class Db;
class View;
class CalendarList;
class DetailView;
class Event;
class Occurrence;
class PrefView;
class Setting;


/** Main application data store. */
struct Calendari
{
  // Settings
  bool        debug;
  Db*         db;
  Setting*    setting;

  // Widgets
  GtkWidget*         window;            ///< Application window
  GtkWidget*         main_drawingarea;  ///< Canvas for the main view.
  GtkLabel*          main_label;        ///< Label for the main view.
  GtkStatusbar*      statusbar;         ///< Status bar.
  GtkVPaned*         sidebar_vpaned;
  GtkCheckMenuItem*  view_cals_menuitem;///< Menu: View>Calendars
  GtkDialog*         about;             ///< About box.
  GtkClipboard*      clipboard;         ///< 'CLIPBOARD' 

  bool main_drawingarea_redraw_queued;

  // Subordinate components.
  View*          main_view;
  CalendarList*  calendar_list;
  DetailView*    detail_view;
  PrefView*      pref_view;

  /** Load database. */
  void load(const char* dbname);

  /** Populate members. */
  void build(GtkBuilder* builder);

  /** Requests a redraw of main_drawingarea. */
  void queue_main_redraw(bool reload=false);

  /** Set the currently selected occurrence. */
  void select(Occurrence* occ);

  /** Get the selected occurrence, if any. */
  Occurrence* selected(void) const
    { return _selected_occurrence; }

  /** Get the cut occurrence, if any. */
  Occurrence* cut(void) const
    { return _clipboard_cut? _clipboard_occurrence: NULL; }

  /** An occurrence moved. */
  void moved(Occurrence* occ);

  /** Create a new calendar - triggered by UI. */
  void create_calendar(void);

  /** Import a new readonly calendar - triggered by UI. */
  void subscribe_calendar(void);

  void delete_selected_calendar(void);

  /** Create a new event - triggered by UI. Copy details from 'old', if set. */
  Occurrence* create_event(time_t dtstart, time_t dtend, Event* old=NULL);

  /** Erase the selected occurrence, if any. */
  void erase_selected(void);

  /** Cut the selected occurrence, if any, and place it in the clipboard. */
  void cut_clipboard(void);

  /** Copy the selected occurrence, if any, and place it in the clipboard. */
  void copy_clipboard(void);
  void paste_clipboard(void);

  // -- call-backs --

  /** GtkClipboardGetFunc */
  static void clipboard_get(
      GtkClipboard*      cb,
      GtkSelectionData*  selection_data,
      guint              info,
      gpointer
    );

  /** GtkClipboardClearFunc */
  static void clipboard_clear(GtkClipboard* cb, gpointer);

private:
  Occurrence* _selected_occurrence;
  Occurrence* _clipboard_occurrence; ///< Contents of the clipboard.
  bool        _clipboard_cut; ///< TRUE if _clipboard_occurrence has been cut.
};


} // end namespace

#endif // CALENDARI__CALENDARI_H
