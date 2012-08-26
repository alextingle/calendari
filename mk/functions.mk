## Late-bound variables used by the functions.
_cppflags = $(strip \
  $(CPPFLAGS) $(addprefix -I,$(CPPINCLUDES)) $(addprefix -D,$(CPPDEFINES)))
_cxxflags   = $(strip $(CXXFLAGS))
_ldflags    = $(strip $(LDFLAGS))
_ldlibflags = $(strip $(addprefix -L,$(LIBPATHS)) $(addprefix -l,$(LIBS)))


## -- Make functions --

#
# Strip off the directory part and extension from file names.
Base = $(notdir $(basename $(1)))


## -- Recipe functions --

#
# Make a new directory (and any missing parents).
# Use as $(call MkDir,PATH)
MakeDir = $(MKDIR) -p $(1)

#
# Touch a file, create it if it doesn't exist.
# Use as $(call Touch,PATHS)
Touch = $(TOUCH) $(1)

#
# Remove files & directories, ignoring errors / missing files.
# Use as $(call Delete,PATHS)
RemoveAll = $(RM) -rf $(1)

#
# Compile an object from C++ source. Generate a .dep as a side-effect.
# Use as $(call CxxCompile,SOURCE,OBJECT,DEPFILE)
CxxDepCompile = \
  $(CXX) -o $(2)  -c $(_cppflags) $(_cxxflags) -MMD -MF $(3) -MT $(2) -MP $(1)

#
# Link an executable from (C++) objects.
# Use as: $(call CxxLinkExe, EXE, OBJECTS, LIBS)
CxxLinkExe = $(CXX) -o $(1)  $(strip $(LINK_STATIC:1=-Wl,-static) \
 $(_cxxflags) \
 $(2) $(filter %.$(ARCHEXT),$(2) $(2)) \
 $(_ldflags) $(_ldlibflags) $(3) \
 $(LINK_STATIC:1=-Wl,-call_shared))


## -- File name transformations --

cc2exe   = $(addprefix $(BINDIR)/,$(call Exe,$(call Base,$(1))))
cc2o     = $(addprefix $(OBJDIR)/,$(1:.cc=.$(OBJEXT)))
cc2dep   = $(addprefix $(OBJDIR)/,$(1:.cc=.dep))
exe2test = $(patsubst %,$(TESTDIR)/%.passed,$(notdir $(1)))
