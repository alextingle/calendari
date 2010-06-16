#include "prefview.h"

#include "calendari.h"
#include "setting.h"

namespace calendari {


void
PrefView::build(GtkBuilder* builder)
{
  prefs = GTK_DIALOG(gtk_builder_get_object(builder,"cali_prefs_dialog"));
  auto_refresh_adjust =
    GTK_ADJUSTMENT(gtk_builder_get_object(builder,"cali_auto_refresh_adj"));
  gtk_adjustment_set_value(
      auto_refresh_adjust,
      app.setting->auto_refresh_minutes()
    );
  weekday_combobox =
      GTK_COMBO_BOX(gtk_builder_get_object(builder,"cali_weekday_combobox"));
  gtk_combo_box_set_active( weekday_combobox, app.setting->week_starts() );
}


void
PrefView::adj_value_changed_cb(GtkAdjustment* adj)
{
  if(adj == auto_refresh_adjust)
    app.setting->set_auto_refresh_minutes( gtk_adjustment_get_value(adj) );
}


void
PrefView::combobox_cb(GtkComboBox* cb)
{
  if(cb==weekday_combobox)
  {
    int cur_pos = app.setting->week_starts();
    int new_pos = gtk_combo_box_get_active(weekday_combobox);
    if(new_pos==cur_pos)
        return;
    app.setting->set_week_starts(new_pos);
    gtk_combo_box_set_active( weekday_combobox, app.setting->week_starts() );
  }
}


} // end namespace calendari
