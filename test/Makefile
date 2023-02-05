### Makefile
### Automates the build of the example program.
###
### Author: Nathan Campos <nathan@innoveworkshop.com>

include ../variables.mk

# Directories and Paths
LIBDIR         := ../$(SRCDIR)
PRJBUILDDIR    := ../$(BUILDDIR)
LIBENTRYLOGGER := $(PRJBUILDDIR)/lib$(PROJECT).a

# Sources and Objects
SOURCES  = main.c
OBJECTS := $(addprefix $(PRJBUILDDIR)/, $(patsubst %.c, %.o, $(SOURCES)))
TARGET  := $(PRJBUILDDIR)/$(PROJECT)_test

.PHONY: all compile run debug memcheck example clean
all: compile

compile: $(LIBENTRYLOGGER) $(TARGET)

$(TARGET): $(OBJECTS) $(LIBENTRYLOGGER)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(PRJBUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBENTRYLOGGER):
	cd .. && $(MAKE)

$(ELDEXAMPLE): compile
	$(TARGET) -c $(ELDEXAMPLE)

debug: CFLAGS += -g3 -DDEBUG
debug: clean compile $(ELDEXAMPLE)
	$(GDB) $(TARGET) $(ELDEXAMPLE)

memcheck: CFLAGS += -g3 -DDEBUG -DMEMCHECK
memcheck: clean compile
	valgrind --tool=memcheck --leak-check=yes --show-leak-kinds=all \
		--track-origins=yes --log-file=$(PRJBUILDDIR)/valgrind.log $(TARGET)
	cat $(PRJBUILDDIR)/valgrind.log

run: compile $(ELDEXAMPLE)
	$(TARGET) $(ELDEXAMPLE)

example: $(ELDEXAMPLE)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(TARGET)
	$(RM) $(PRJBUILDDIR)/valgrind.log
