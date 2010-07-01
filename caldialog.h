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
  CalDialog(calendari::Calendari* app, GtkBuilder* builder, const char* root);

public:
  virtual ~CalDialog(void) {}

  virtual void run(void);

  virtual void entry_cb(GtkEntry* entry);
  void color_set_cb(GtkColorButton* cb);
  virtual void file_set_cb(GtkFileChooserButton*) {assert(false);}
  virtual void activated(GtkToggleButton*) {assert(false);}
  virtual void button_clicked_cb(GtkButton*)  {assert(false);}
};


struct CalSubDialog: public CalDialog
{
  GtkFileChooserButton*  _filechooserbutton;

public:
  CalSubDialog(Calendari* app, GtkBuilder* builder);
  virtual void run(void);
  virtual void file_set_cb(GtkFileChooserButton* fc);
};


struct CalPubDialog: public CalDialog
{
  GtkToggleButton* _private_togglebutton;
  GtkToggleButton* _public_togglebutton;
  GtkEntry*        _path_entry;
  GtkButton*       _browse_button;

public:
  CalPubDialog(Calendari* app, GtkBuilder* builder);
  virtual void run(void);
  virtual void entry_cb(GtkEntry* entry);
  virtual void activated(GtkToggleButton*);
  virtual void button_clicked_cb(GtkButton*);
};


} // end namespace calendari

#endif // CALENDARI__CAL_DIALOG_H
