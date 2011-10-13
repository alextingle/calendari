## -- File name extensions --

# Object files.
OBJEXT  := o

# Shared object files.
SOEXT   := so

# Archive files.
ARCHEXT := a

# Executable files (optional)
EXEEXT  :=

_soext   = $(SOEXT:%=.%)
_exeext  = $(EXEEXT:%=.%)

## -- File name templates --

#
# Template for library link line.
# Use as -l$(call Libname,NAME)
Libname = $(strip $(1))

#
# Template for shared library soname.
# Use as: $(call Soname,NAME)
Soname = $(patsubst %,lib%$(_soext),$(Libname))

#
# Template for shared library file name.
# Use as: -o $(call SharedLib,NAME)
SharedLib = $(Soname)

#
# Template for static library file name.
# Use as: -o $(call StaticLib,NAME)
StaticLib = lib$(Libname)$(ARCHEXT:%=.%)

#
# Template for executable file name.
# Use as: -o $(call Exe,NAME)
Exe = $(1:%=%$(_exeext))
