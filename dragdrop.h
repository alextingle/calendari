#ifndef CALENDARI__DRAG_DROP_H
#define CALENDARI__DRAG_DROP_H 1

#include <gtk/gtk.h>

namespace calendari {


/** Utility class. */
class DragDrop
{
public:
  enum type
    {
      DD_OCCURRENCE,
      DD_STRING
    };

  static GtkTargetEntry  target_list_src[];
  static guint           target_list_src_len;

  static GtkTargetEntry  target_list_dest[];
  static guint           target_list_dest_len;

};


} // end namespace

#endif // CALENDARI__DRAG_DROP_H
