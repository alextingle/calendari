#include "dragdrop.h"

namespace calendari {


GtkTargetEntry DragDrop::target_list_src[] ={
    { (gchar*)"OCCURRENCE", GTK_TARGET_SAME_WIDGET, DD_OCCURRENCE },
    { (gchar*)"STRING",     GTK_TARGET_OTHER_APP,   DD_STRING     },
    { (gchar*)"text/plain", GTK_TARGET_OTHER_APP,   DD_STRING     }
  };
guint DragDrop::target_list_src_len =G_N_ELEMENTS(DragDrop::target_list_src);

GtkTargetEntry DragDrop::target_list_dest[] ={
    { (gchar*)"OCCURRENCE", GTK_TARGET_SAME_WIDGET, DD_OCCURRENCE }
  };
guint DragDrop::target_list_dest_len =G_N_ELEMENTS(DragDrop::target_list_dest);



} // end namespace
