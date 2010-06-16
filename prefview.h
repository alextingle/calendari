#ifndef CALENDARI__PREF_VIEW_H
#define CALENDARI__PREF_VIEW_H 1

#include <gtk/gtk.h>

namespace calendari {

class Calendari;
class Occurrence;


/** Preferences dialogue box. */
struct PrefView
{
  Calendari& app;

  GtkDialog*     prefs;             ///< Preferences dialogue.
  GtkAdjustment* auto_refresh_adjust;
  GtkComboBox*   weekday_combobox;

  PrefView(Calendari& c): app(c) {}

  /** Populate members. */
  void build(GtkBuilder* builder);

  void adj_value_changed_cb(GtkAdjustment* adj);
  void combobox_cb(GtkComboBox* cb);
};


} // end namespace

#endif // CALENDARI__PREF_VIEW_H
