#ifndef CALENDARI__UTIL_H
#define CALENDARI__UTIL_H 1

#include <cassert>
#include <gtk/gtk.h>
#include <time.h>
#include <string>

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


/** Write a new UUID to 'dest' buffer (size 'n'). Return the pointer to the next
*   free byte, or NULL if there was an error. */
char* uuidp(char* dest, size_t n);


/** Get a new UUID string. */
std::string uuids(void);


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
  ptr_type release(void)
    {
      ptr_type v = _v;
      _v = NULL;
      return v;
    }
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


/** Find a value in a GtkTreeModel. */
template<class M, typename T>
bool tree_model_find(M* model, int column, const T& value, GtkTreeIter* iter)
{
  assert(model);
  assert(iter);
  GtkTreeModel* m = GTK_TREE_MODEL(model);
  bool ok = gtk_tree_model_get_iter_first(m,iter);
  while(ok)
  {
    T v;
    gtk_tree_model_get(m,iter,column,&v,-1);
    if(v == value)
        return true;
    ok = gtk_tree_model_iter_next(m,iter);
  }
  return false;
}


} // end namespace calendari

#endif
