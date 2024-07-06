# Makefile by Adam, 2022

################################################################################
# What OS we're compiling on
################################################################################

ifeq ($(OS),Windows_NT)
    HOST_OS = Windows
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        HOST_OS = Linux
    else ifeq ($(UNAME_S),Darwin)
        HOST_OS = Darwin
    endif
endif

################################################################################
# Programs to use
################################################################################

ifeq ($(HOST_OS),Windows)
	CC = gcc
else ifeq ($(HOST_OS),Linux)
	CC = gcc
else ifeq ($(UNAME_S),Darwin)
	CC = gcc
endif

FIND:=find
ifeq ($(HOST_OS),Windows)
	FIND:=$(shell cygpath `where find | grep bin | grep -v " "`)
endif

################################################################################
# Source Files
################################################################################

# This is all the examples files
EXAMPLE_SOURCES = $(shell $(FIND) examples -maxdepth 1 -iname "*.[c]")
EXAMPLES = $(patsubst %.c, %, $(EXAMPLE_SOURCES) )

################################################################################
# Includes
################################################################################

INC_DIRS = .
# Prefix the directories for gcc
INC = $(patsubst %, -I%, $(INC_DIRS) )

################################################################################
# Compiler Flags
################################################################################

# These are flags for the compiler, all files
CFLAGS =

ifeq ($(HOST_OS),Darwin)
# Required for OpenGL and some other libraries
# TODO is this needed
CFLAGS += \
	-I/opt/homebrew/include
endif

################################################################################
# Defines
################################################################################

# Create a variable with the git hash and branch name
GIT_HASH  = \"$(shell git rev-parse --short=7 HEAD)\"

# Extra defines
DEFINES_LIST += \
	GIT_SHA1=${GIT_HASH}

DEFINES = $(patsubst %, -D%, $(DEFINES_LIST))

################################################################################
# Linker options
################################################################################

# This is a list of libraries to include. Order doesn't matter
ifeq ($(HOST_OS),Windows)
    LIBS = winmm
endif
ifeq ($(HOST_OS),Linux)
    LIBS = asound
endif
ifeq ($(HOST_OS),Darwin)
    LIBS = pulse
endif

# These are directories to look for library files in
LIB_DIRS =

# TODO is this necessary?
ifeq ($(HOST_OS),Darwin)
    LIB_DIRS = /opt/homebrew/lib
endif

# This combines the flags for the linker to find and use libraries
LIBRARY_FLAGS = $(patsubst %, -L%, $(LIB_DIRS)) $(patsubst %, -l%, $(LIBS))

# Incompatible flags for clang on MacOS
ifeq ($(HOST_OS),Darwin)
LIBRARY_FLAGS += \
	-framework Foundation \
	-framework CoreFoundation \
	-framework CoreMIDI
endif

################################################################################
# Targets for Building
################################################################################

# This list of targets do not build files which match their name
.PHONY: all clean print-%

# Build everything!
all: examples

examples: $(EXAMPLES)

# This compiles each example into an executable
./examples/%: ./examples/%.c
	$(CC) $(CFLAGS) $(INC) $(LIBRARY_FLAGS) $< -o $@

clean:
	-@rm -f $(OBJECTS) $(EXAMPLES)

# This cleans everything
fullclean: clean
	git clean -fX

# Print any value from this makefile
print-%  : ; @echo $* = $($*)
