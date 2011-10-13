## -- Files --

OFILES        := $(call cc2o,$(CCFILES))
DEPFILES      := $(call cc2dep,$(CCFILES) $(CCFILES.EXE) $(CCFILES.TEST))
OBJDIRS       := $(sort $(dir $(DEPFILES)))
EXEFILES      := $(call cc2exe,$(CCFILES.EXE))
EXEFILES.TEST := $(call cc2exe,$(CCFILES.TEST))


## -- Pattern rules --

# Rules to compile objects from C++ source.
$(OBJDIR)/%.$(OBJEXT):  %.cc | $$(@D)
	$(call CxxDepCompile,$<,$@,$(OBJDIR)/$*.dep)
$(OBJDIR)/%.$(OBJEXT):  $(OBJDIR)/%.cc | $$(@D)
	$(call CxxDepCompile,$<,$@,$(OBJDIR)/$*.dep)


## -- Targets --

.PHONY: default
default: exes


# Directories
$(OBJDIRS) $(BINDIR) $(LIBDIR) $(TESTDIR):
	$(call MakeDir,$@)


# Executables (including test cases).
.PHONY: exes
exes: $(EXEFILES) $(EXEFILES.TEST)

define CcExeRule
$(cc2exe): $(1:%.cc=$(OBJDIR)/%.$(OBJEXT)) $(OFILES) | $(BINDIR)
	$$(call CxxLinkExe,$$@,$$^,)
endef
$(foreach E,$(CCFILES.EXE) $(CCFILES.TEST),$(eval $(call CcExeRule,$(E))))


# Run the test cases and record in $(TESTDIR) if they pass.
.PHONY: test
test: $(call exe2test,$(EXEFILES.TEST))
define TestRule
$(call exe2test,$(1)): $(1) | $(TESTDIR)
	$$^
	$$(call Touch,$$@) # the test has passed
endef
$(foreach T,$(EXEFILES.TEST),$(eval $(call TestRule,$(T))))


# Clean
.PHONY: clean
clean:
	$(call RemoveAll,$(OBJDIR) $(BINDIR) $(LIBDIR))


## -- Bring in generated dependency files --

ifneq "$(MAKECMDGOALS)" "clean"
  -include $(DEPFILES)
endif
