#include "util.h"

#include <cstring>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


namespace calendari {


const char* system_timezone(void)
{
  static std::string tzid = "";
  if(!tzid.empty())
      return tzid.c_str();

  // Look for /etc/timezone
  int fd = ::open("/etc/timezone",O_RDONLY);
  if(fd<0)
  {
    ::error(0,errno,"Failed to open /etc/timezone");
  }
  else
  {
    char buf[64];
    if(::read(fd,buf,sizeof(buf)))
    {
      buf[ sizeof(buf)-1 ] = '\0';
      char* pos = ::strchrnul(buf,'\n');
      if(pos>buf)
      {
        *pos = '\0';
        tzid = buf;
      }
    }
    ::close(fd);
  }
  // ?? Add in other ways to find timezone - /etc/localtime, Gnome Oop, etc.
  if(tzid.empty())
  {
    ::error(0,0,"Can't find system timezone. Using UTC.");
    tzid = "UTC";
  }
  return tzid.c_str();
}


GdkPixbuf* new_pixbuf_from_col(GdkColor& col, int width, int height)
{
  guint32 rgba = 0xFF |
    (0xFF00 & col.red)<<16 | (0xFF00 & col.green)<<8 | (0xFF00 & col.blue);
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,false,8,width,height);
  gdk_pixbuf_fill(pixbuf,rgba);
  return pixbuf;
}


} // end namespace calendari
