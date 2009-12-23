#include "detailview.h"

#include "calendari.h"
#include "calendarlist.h"
#include "err.h"
#include "event.h"

#include <cassert>
#include <cstring>

#define FORMAT_DATE "%Y-%m-%d"
#define FORMAT_TIME " %H:%M"

namespace calendari {


void
DetailView::build(Calendari*, GtkBuilder* builder)
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
  gtk_entry_set_text(title_entry,occ->event.summary().c_str());

  // Fill in times
  this->moved(occ);

  GtkTreeIter iter;
  bool ok = gtk_tree_model_iter_nth_child(
      GTK_TREE_MODEL(cal.calendar_list->liststore_cal),
      &iter,
      NULL,
      occ->event.calendar().position()
    );
  if(!ok)
  {
    CALI_WARN(0,"Bad index in calendar list: %d",
        occ->event.calendar().position());
    clear();
    return;
  }
  gtk_combo_box_set_active_iter(calendar_combobox,&iter);

  const bool sensitive = !occ->event.calendar().readonly();
  gtk_widget_set_sensitive(GTK_WIDGET(title_entry),sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(start_entry),sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(end_entry),sensitive);
  gtk_widget_set_sensitive(GTK_WIDGET(calendar_combobox),sensitive);
}


void
DetailView::moved(Occurrence* occ)
{
  tm t;
  char buf[256];
  const char* date_format =
      ( occ->event.all_day()? FORMAT_DATE: FORMAT_DATE FORMAT_TIME );

  time_t dtstart = occ->dtstart();
  localtime_r(&dtstart,&t);
  strftime(buf,sizeof(buf),date_format,&t);
  gtk_entry_set_text(start_entry,buf);

  time_t dtend = occ->dtend();
  localtime_r(&dtend,&t);
  strftime(buf,sizeof(buf),date_format,&t);
  gtk_entry_set_text(end_entry,buf);
}


void
DetailView::entry_cb(GtkEntry* entry, calendari::Calendari* cal)
{
  Occurrence* selected = cal->selected();
  if(!selected)
      return;

  const char* newval = gtk_entry_get_text(entry);
  if(entry==title_entry)
  {
    if(selected->event.summary()!=newval && ::strlen(newval))
    {
      selected->event.set_summary( newval );
      gtk_widget_queue_draw(GTK_WIDGET(cal->main_drawingarea));
    }
  }
  // ?? Need much more work on this bit.
  // ?? It will get much more complicated when we start supporting
  // ?? recurring events.
  if(entry==start_entry)
  {
    tm new_tm;
    ::memset(&new_tm,0,sizeof(new_tm));
    char* ret = ::strptime(newval,FORMAT_DATE FORMAT_TIME,&new_tm);
    if(ret)
    {
      selected->event.set_all_day(false);
      new_tm.tm_isdst = -1;
      if(selected->set_start( ::mktime(&new_tm) ))
          cal->moved(selected);
      return;
    }
    ret = ::strptime(newval,FORMAT_DATE,&new_tm);
    if(ret)
    {
      selected->event.set_all_day(true);
      new_tm.tm_isdst = -1;
      new_tm.tm_hour = 12;
      if(selected->set_start( ::mktime(&new_tm) ))
          cal->moved(selected);
      return;
    }
  }
  if(entry==end_entry)
  {
    tm new_tm;
    ::memset(&new_tm,0,sizeof(new_tm));
    char* ret = ::strptime(newval,FORMAT_DATE FORMAT_TIME,&new_tm);
    if(ret && !selected->event.all_day())
    {
      new_tm.tm_isdst = -1;
      if(selected->set_end( ::mktime(&new_tm) ))
          cal->moved(selected);
      return;
    }
    ret = ::strptime(newval,FORMAT_DATE,&new_tm);
    if(ret && selected->event.all_day())
    {
      new_tm.tm_isdst = -1;
      new_tm.tm_hour = 12;
      if(selected->set_end( ::mktime(&new_tm) ))
          cal->moved(selected);
      return;
    }
  }
}


void
DetailView::combobox_cb(GtkComboBox* cb, calendari::Calendari* cal)
{
  Occurrence* selected = cal->selected();
  if(!selected)
      return;
  int cur_pos = selected->event.calendar().position();
  int new_pos = gtk_combo_box_get_active(cb);
  if(new_pos==cur_pos)
      return;
  GtkTreeIter iter;
  if( gtk_combo_box_get_active_iter(cb,&iter) )
  {
    Calendar* calendar;
    gtk_tree_model_get(
        GTK_TREE_MODEL( cal->calendar_list->liststore_cal ),
        &iter,
        0,&calendar,
        -1
      );
    selected->event.set_calendar( *calendar );
    cal->calendar_list->select(selected);
    gtk_widget_queue_draw(GTK_WIDGET(cal->main_drawingarea));
  }
}


} // end namespace calendari
