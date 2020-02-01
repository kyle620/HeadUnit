###############################################################################
# Substitutions for common used utilities and programs, exported to be valid
# for all sub-make files.
###############################################################################


CROSS_COMPILE ?=
CC            ?= $(CROSS_COMPILE)gcc
CXX           ?= $(CROSS_COMPILE)g++
STRIP         ?= $(CROSS_COMPILE)strip
LD            ?= $(CROSS_COMPILE)gcc
OBJCOPY       ?= $(CROSS_COMPILE)objcopy
AR            ?= $(CROSS_COMPILE)ar
SIZE          ?= $(CROSS_COMPILE)size -A

RM    := rm -f
RMDIR := rm -rf
MKDIR := mkdir -p

# Some usefull variables for string manipulation.
COMMA:= ,
EMPTY:=
SPACE:= $(EMPTY) $(EMPTY)
COLON:=:
QUOTE:=\"

# line feed definition used in error messages
define n


endef
