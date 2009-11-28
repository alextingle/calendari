CXXFLAGS += -g $(shell pkg-config --cflags gtk+-2.0 gmodule-2.0)

LDFLAGS += $(shell pkg-config --libs gtk+-2.0 gmodule-2.0) -lsqlite3

CC_FILES := $(wildcard *.cc)
OBJECTS := $(CC_FILES:.cc=.o)

.PHONEY: calendari
calendari: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(OBJECTS): %.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $<

.PHONEY: clean
clean:
	rm -f calendari *.o
