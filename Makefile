CCFILES := \
  caldialog.cc \
  calendarlist.cc \
  callback.cc \
  db.cc \
  detailview.cc \
  dragdrop.cc \
  err.cc \
  event.cc \
  ics.cc \
  monthview.cc \
  prefview.cc \
  queue.cc \
  reader.cc \
  recur.cc \
  setting.cc \
  sql.cc \
  util.cc \

CCFILES.EXE := calendari.cc

CXXFLAGS += $$(pkg-config --cflags gtk+-2.0 gmodule-2.0)
LDFLAGS += $$(pkg-config --libs gtk+-2.0 gmodule-2.0)

LIBS += sqlite3 ical uuid

include mk/main.mk
