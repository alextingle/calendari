OBJDIR := obj

CPPFLAGS += $(TESTFLAGS)
CXXFLAGS += -g $(shell pkg-config --cflags gtk+-2.0 gmodule-2.0)
LDFLAGS += $(shell pkg-config --libs gtk+-2.0 gmodule-2.0) -lsqlite3 -lical

CC_FILES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc,$(OBJDIR)/%.o,$(CC_FILES))

.PHONEY: calendari
calendari: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

.PHONEY: test_ics
test_ics: TESTFLAGS := -DCALENDARI__ICAL__READ__TEST
test_ics: obj/ics.o obj/db.o obj/event.o obj/queue.o
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
