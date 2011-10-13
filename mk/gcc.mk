## GCC specific modifications.

# Other flavours can replace this with their own.

ifeq "$(DEBUG)" "1"
  CXXFLAGS   += -g
else
  CPPDEFINES += NDEBUG
  CXXFLAGS   += -O2
endif

CXXFLAGS += -Wall -Wextra
