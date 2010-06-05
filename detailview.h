#ifndef CALENDARI__DETAIL_VIEW_H
#define CALENDARI__DETAIL_VIEW_H 1

#include <gtk/gtk.h>

namespace calendari {

class Calendari;
class Occurrence;


struct DetailView
{
  Calendari& cal;

  GtkEntry*     title_entry;
  GtkEntry*     start_entry;
  GtkEntry*     end_entry;
  GtkComboBox*  calendar_combobox;
  GtkTextView*  textview;

  DetailView(Calendari& c): cal(c) {}

  /** Populate members. */
  void build(Calendari* cal, GtkBuilder* builder);

  void clear(void);
  
  void select(Occurrence* occ);
  void moved(Occurrence* occ);

  void entry_cb(GtkEntry* entry, calendari::Calendari* cal);
  void combobox_cb(GtkComboBox* cb, calendari::Calendari* cal);
  void textview_cb(GtkTextView* tv, calendari::Calendari* cal);
};


} // end namespace

#endif // CALENDARI__DETAIL_VIEW_H
