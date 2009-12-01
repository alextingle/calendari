#include "detailview.h"

#include "calendari.h"
#include "calendarlist.h"
#include "err.h"
#include "event.h"

namespace calendari {


void
DetailView::build(Calendari* cal, GtkBuilder* builder)
{
  title_entry = GTK_ENTRY(gtk_builder_get_object(builder,"detail_title_entry"));
  start_entry = GTK_ENTRY(gtk_builder_get_object(builder,"detail_start_entry"));
  end_entry   = GTK_ENTRY(gtk_builder_get_object(builder,"detail_end_entry"));
  calendar_combobox =
      GTK_COMBO_BOX(gtk_builder_get_object(builder,"detail_calendar_combobox"));
  textview = GTK_TEXT_VIEW(gtk_builder_get_object(builder,"detail_textview"));
}


void
DetailView::clear(void)
{
  gtk_entry_set_text(title_entry,"");
  gtk_entry_set_text(start_entry,"");
  gtk_entry_set_text(end_entry,"");
  gtk_widget_set_sensitive(GTK_WIDGET(title_entry),false);
  gtk_widget_set_sensitive(GTK_WIDGET(start_entry),false);
  gtk_widget_set_sensitive(GTK_WIDGET(end_entry),false);
  gtk_widget_set_sensitive(GTK_WIDGET(calendar_combobox),false);
}


void
DetailView::select(Occurrence* occ)
{
  if(!occ)
  {
    clear();
    return;
  }
  gtk_entry_set_text(title_entry,occ->event.summary.c_str());
  tm t;
  char buf[256];

  const char* date_format =( occ->event.all_day? "%x": "%X %x" );

  localtime_r(&occ->dtstart,&t);
  strftime(buf,sizeof(buf),date_format,&t);
  gtk_entry_set_text(start_entry,buf);

  localtime_r(&occ->dtend,&t);
  strftime(buf,sizeof(buf),date_format,&t);
  gtk_entry_set_text(end_entry,buf);

  GtkTreeIter iter;
  bool ok = gtk_tree_model_iter_nth_child(
      GTK_TREE_MODEL(cal.calendar_list->liststore_cal),
      &iter,
      NULL,
      occ->event.calendar.position
    );
  if(!ok)
  {
    util::warning(0,
        "Bad index in calendar list: %d",occ->event.calendar.position);
    clear();
    return;
  }
  gtk_combo_box_set_active_iter(calendar_combobox,&iter);

  gtk_widget_set_sensitive(GTK_WIDGET(title_entry),true);
  gtk_widget_set_sensitive(GTK_WIDGET(start_entry),true);
  gtk_widget_set_sensitive(GTK_WIDGET(end_entry),true);
  gtk_widget_set_sensitive(GTK_WIDGET(calendar_combobox),true);
}


} // end namespace calendari
