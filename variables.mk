### variables.mk
### Common variables used throughout the project.
###
### Author: Nathan Campos <nathan@innoveworkshop.com>

# Project
PROJECT = entrylog

# Environment
PLATFORM := $(shell uname -s)

# Directories and Paths
SRCDIR     := src
TESTDIR    := test
BUILDDIR   := build
ELDEXAMPLE ?= example.eld

# Tools
CC    = gcc
AR    = ar
GDB   = gdb
RM    = rm -f
MKDIR = mkdir -p
TOUCH = touch

# Handle OS X-specific tools.
ifeq ($(PLATFORM), Darwin)
	CC  = clang
	GDB = lldb
endif

# Flags
CFLAGS  = -Wall -Wno-psabi --std=c89
LDFLAGS =
