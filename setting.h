#ifndef CALENDARI__SETTINGS_H
#define CALENDARI__SETTINGS_H 1

#include <gtk/gtk.h>

namespace calendari {

class Calendari;


class Setting
{
public:
  static const int head_font_size = 12;
  static const int body_font_size = 11;

  Setting(Calendari&);

  int auto_refresh_minutes(void) const {return _auto_refresh_minutes;}
  void set_auto_refresh_minutes(int);

  int week_starts(void) const {return _week_starts;}
  void set_week_starts(int);

private:
  Setting(const Setting&);              ///< Not copyable
  Setting& operator = (const Setting&); ///< Not assignable

  // -- Non-setting data --

  Calendari& app;
  guint _timeout_source_tag; ///< ID of timeout source.

  // -- Settings --

  int _auto_refresh_minutes;
  int _week_starts;
};


} // end namespace

#endif // CALENDARI__SETTINGS_H
