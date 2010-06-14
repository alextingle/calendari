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
      cal.setting->auto_refresh_minutes()
    );
}


void
PrefView::adj_value_changed_cb(GtkAdjustment* adj)
{
  if(adj == auto_refresh_adjust)
    cal.setting->set_auto_refresh_minutes( gtk_adjustment_get_value(adj) );
}


} // end namespace calendari
