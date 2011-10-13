# Variables that may be set

# BUILDDIR    -  Root for all build files. Usually empty, which puts the build
#                into the current dir. May be set to another volume (/tmp for
#                example).
# CPPFLAGS
# CPPINCLUDES - list of CPP include paths, e.g. path/one /abs/path/two
# CPPDEFINES  - list of defines, e.g. VAR1 VAR2=VAL VAR3
# CXXFLAGS
# LDFLAGS
# LIBPATHS
# LIBS
# LINK_STATIC - 1: Executables are statically linked.
# DEBUG       - 0: Optimize


# These component makefiles are all in the same directory as this file.
include mk/init.mk
include mk/utilities.mk
include mk/templates.mk
include mk/functions.mk

_flavour_mk := $(wildcard $(FLAVOUR).mk)
ifneq "$(_flavour_mk)" ""
  include $(wildcard $(FLAVOUR).mk)
else
  include mk/gcc.mk
endif

include mk/rules.mk
