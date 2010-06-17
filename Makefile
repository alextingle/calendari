OBJDIR := obj

CPPFLAGS += $(TESTFLAGS)
CXXFLAGS += -g $$(pkg-config --cflags gtk+-2.0 gmodule-2.0) -Wall -Wextra
LDFLAGS += $$(pkg-config --libs gtk+-2.0 gmodule-2.0) -lsqlite3 -lical

CC_FILES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc,$(OBJDIR)/%.o,$(CC_FILES))

.PHONEY: calendari
calendari: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

.PHONEY: test_ics_read
test_ics_read: TESTFLAGS := -DCALENDARI__ICAL__READ__TEST
test_ics_read: obj/ics.o obj/db.o obj/event.o obj/queue.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

.PHONEY: test_ics_write
test_ics_write: TESTFLAGS := -DCALENDARI__ICAL__WRITE__TEST
test_ics_write: obj/ics.o obj/db.o obj/event.o obj/queue.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(OBJECTS): $(OBJDIR)/%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONEY: clean
clean:
	rm -f $(OBJDIR)/*.o

.PHONEY: really_clean
really_clean: clean
	rm -f calendari test_*

$(OBJDIR)/deps.mk: $(wildcard *.h) $(CC_FILES)
	g++ -MM $(CXXFLAGS) $(CPPFLAGS) $(CC_FILES) | \
	  sed 's%^[a-zA-Z].*:%$(OBJDIR)/\0%' > $@

ifneq "$(MAKECMDGOALS)" "clean"
  include $(OBJDIR)/deps.mk
endif
