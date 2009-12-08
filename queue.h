#ifndef CALENDARI__QUEUE_H
#define CALENDARI__QUEUE_H 1

#include <string>
#include <list>

namespace calendari {

class Db;


/** Stores SQL for pending changes to the database. */
class Queue
{
public:
  static bool idle(void*);
  static Queue& inst(void)
    {
      static Queue q;
      return q;
    }

  /** Escape single-quote characters for Sqlite strings. */
  std::string quote(const std::string& s) const;

  void set_db(Db* db_);

  bool empty(void) const { return _changes.empty(); }
  void push(const std::string& sql);
  void pushf(const char* format, ...);
  void flush(void);

private:
  Queue(void) {}
  Queue(const Queue&);
  Queue& operator = (const Queue&);

  Db* _db;
  std::list<std::string> _changes;
};


} // end namespace

#endif // CALENDARI__QUEUE_H
