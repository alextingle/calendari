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
  
  void select(Occurrence* occ);
};


} // end namespace

#endif // CALENDARI__DETAIL_VIEW_H
