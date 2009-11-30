#include "detailview.h"

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
DetailView::select(Occurrence* occ)
{
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
}


} // end namespace calendari
