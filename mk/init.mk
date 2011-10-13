ifndef _init_mk
_init_mk := 1

## -- Make settings --

# Grab the first target "all".
.PHONY: all
all: default

# Eliminate all default rules.
.SUFFIXES:

# Don't leave half-finished targets lying around.
.DELETE_ON_ERROR:

# Turn on secondary expansion
.SECONDEXPANSION:


## -- Make variables --

# Useful constants
EMPTY :=
COMMA := ,
SPACE := $(EMPTY) $(EMPTY)

# Default to compiling a debug version. Override with command-line: DEBUG=0
ifeq ($(origin DEBUG), undefined)
  DEBUG := 1
endif


## -- Directories --

# Root for all object files.
OBJDIR  := $(BUILDDIR:%=%/)OBJ$(FLAVOUR:%=-%)

# Binaries
BINDIR  := $(BUILDDIR:%=%/)bin$(FLAVOUR:%=-%)

# Libraries
LIBDIR  := $(BUILDDIR:%=%/)lib$(FLAVOUR:%=-%)

# Test results go here.
TESTDIR := $(OBJDIR)/test.dir


## -- Build variables --

# CPP include paths.
CPPINCLUDES :=

endif # _init_mk
