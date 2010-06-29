#include "err.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <error.h>
#include <gtk/gtk.h>

namespace calendari {
namespace util {


void error(const Here& here,int r,int e,const char* format,...)
{
  char buf[512];
  va_list va_args;
  va_start(va_args,format);
  int len = ::vsnprintf(buf,sizeof(buf),format,va_args);
  va_end(va_args);
  if(e)
      snprintf(buf+len,sizeof(buf)-len,": %s",::strerror(e));
  buf[ sizeof(buf)-1 ] = '\0';
  fprintf(stderr,"%s at %s:%d\n",buf,here.first,here.second);
  // Pop up error dialogue
  GtkWidget* dialog =
    gtk_message_dialog_new(
        NULL, // window
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_ERROR,
        GTK_BUTTONS_CLOSE,
        "%s",buf
      );
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
#ifndef NDEBUG
  abort();
#endif
  if(r)
      ::exit(r);
}


void warning(const Here& here,int e,const char* format,...)
{
  char buf[512];
  va_list va_args;
  va_start(va_args,format);
  int len = ::vsnprintf(buf,sizeof(buf),format,va_args);
  va_end(va_args);
  if(e)
      snprintf(buf+len,sizeof(buf)-len,": %s",::strerror(e));
  buf[ sizeof(buf)-1 ] = '\0';
  fprintf(stderr,"%s at %s:%d\n",buf,here.first,here.second);
  // Pop up error dialogue
  GtkWidget* dialog =
    gtk_message_dialog_new(
        NULL, // window
        GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_WARNING,
        GTK_BUTTONS_CLOSE,
        "%s",buf
      );
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}


} } // end namespace calendari::util
