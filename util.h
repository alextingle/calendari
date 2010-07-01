#ifndef CALENDARI__UTIL_H
#define CALENDARI__UTIL_H 1

#include <gtk/gtk.h>
#include <time.h>

extern "C" { void g_free(void*); }

namespace calendari {


const char* system_timezone(void);

inline time_t normalise_local_tm(struct tm& v)
{
  v.tm_isdst = -1;
  time_t result = mktime(&v);
  localtime_r(&result,&v);
  return result;
}


inline const char* safestr(const char* s, const char* dflt="")
{
  if(s)
      return s;
  else
      return dflt;
}


inline const char* safestr(const unsigned char* s, const char* dflt="")
{
  if(s)
      return reinterpret_cast<const char*>( s );
  else
      return dflt;
}


/** A general scoped pointer type. */
template<typename T, void F(T*)>
class scoped
{
public:
  typedef T  value_type;
  typedef T* ptr_type;
  typedef T& ref_type;
  scoped(ptr_type ptr): _v(ptr)
    {}
  ~scoped(void)
    {
      if(_v)
          F(_v);
    }
  ref_type operator * (void) const
    { return *_v; }
  operator bool (void) const
    { return _v? true: false; }
  ptr_type operator -> (void) const
    { return _v; }
  ptr_type get(void) const
    { return _v; }
private:
  scoped(const scoped&);              ///< Not copyable
  scoped& operator = (const scoped&); ///< Not assignable

  ptr_type _v;
};


/** A scoped pointer type that wraps g_free(void*). */
template<typename T>
class g_scoped
{
public:
  typedef T  value_type;
  typedef T* ptr_type;
  typedef T& ref_type;
  g_scoped(ptr_type ptr): _v(ptr)
    {}
  ~g_scoped(void)
    {
      if(_v)
          g_free(static_cast<void*>(_v));
    }
  ref_type operator * (void) const
    { return *_v; }
  operator bool (void) const
    { return _v? true: false; }
  ptr_type operator -> (void) const
    { return _v; }
  ptr_type get(void) const
    { return _v; }
private:
  g_scoped(const g_scoped&);              ///< Not copyable
  g_scoped& operator = (const g_scoped&); ///< Not assignable

  ptr_type _v;
};


/** Construct a new pixbuf with a solid colour.
*   Needs to be freed by calling g_object_unref() */
GdkPixbuf* new_pixbuf_from_col(GdkColor& col, int width, int height);


} // end namespace calendari

#endif
