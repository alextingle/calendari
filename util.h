#ifndef CALENDARI__UTIL_H
#define CALENDARI__UTIL_H 1

#include <time.h>

namespace calendari {


inline time_t normalise_local_tm(struct tm& v)
{
    time_t result = mktime(&v);
    localtime_r(&result,&v);
    return result;
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


} // end namespace calendari

#endif
