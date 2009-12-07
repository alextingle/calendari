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
  static Queue& inst(void)
    {
      static Queue q;
      return q;
    }

  /** Escape single-quote characters for Sqlite strings. */
  std::string quote(const std::string& s) const;

  void push(const std::string& sql);
  void pushf(const char* format, ...);
  void flush(Db& db);

private:
  Queue(void) {}
  Queue(const Queue&);
  Queue& operator = (const Queue&);

  std::list<std::string> changes;
};


} // end namespace

#endif // CALENDARI__QUEUE_H
