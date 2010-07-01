#ifndef CALENDARI__CAL_DIALOG_H
#define CALENDARI__CAL_DIALOG_H 1

#include <cassert>
#include <gtk/gtk.h>

namespace calendari {

class Calendar;
class Calendari;
class CalendarList;


class CalDialog
{
protected:
  GtkDialog*       _dialog;
  GtkEntry*        _name_entry;
  GtkColorButton*  _colorbutton;

  Calendari&       _app;
  CalendarList&    _cl;
  GtkTreeIter      _it;
  Calendar*        _cal;

  bool setup(void); ///< Sets _it & _cal or returns FALSE.

public:
  CalDialog(calendari::Calendari* app, GtkBuilder* builder, const char* root);
  virtual ~CalDialog(void) {}

  virtual void run(void);

  void entry_cb(GtkEntry* entry);
  void color_set_cb(GtkColorButton* cb);
  virtual void file_set_cb(GtkFileChooserButton*) {assert(false);}
};


struct CalSubDialog: public CalDialog
{
  GtkFileChooserButton*  _filechooserbutton;

public:
  CalSubDialog(Calendari* app, GtkBuilder* builder);
  virtual void run(void);
  virtual void file_set_cb(GtkFileChooserButton* fc);
};


} // end namespace calendari

#endif // CALENDARI__CAL_DIALOG_H
