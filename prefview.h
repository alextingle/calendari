#ifndef CALENDARI__PREF_VIEW_H
#define CALENDARI__PREF_VIEW_H 1

#include <gtk/gtk.h>

namespace calendari {

class Calendari;
class Occurrence;


/** Preferences dialogue box. */
struct PrefView
{
  Calendari& cal;

  GtkDialog*     prefs;             ///< Preferences dialogue.
  GtkAdjustment* auto_refresh_adjust;

  PrefView(Calendari& c): cal(c) {}

  /** Populate members. */
  void build(GtkBuilder* builder);

  void adj_value_changed_cb(GtkAdjustment* adj);
};


} // end namespace

#endif // CALENDARI__PREF_VIEW_H
