### Makefile
### Automates the build and everything else of the project.
###
### Author: Nathan Campos <nathan@innoveworkshop.com>

include variables.mk

# Sources and Objects
SOURCES += $(SRCDIR)/entrylog.c
OBJECTS := $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SOURCES))
TARGET  := $(BUILDDIR)/lib$(PROJECT).a

.PHONY: all compile compileall compiledb test example debug memcheck clean
all: compile

compile: $(BUILDDIR)/stamp $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/stamp:
	$(MKDIR) $(@D)
	$(TOUCH) $@

compiledb: clean
	bear --output .vscode/compile_commands.json -- make compileall

compileall: compile
	cd $(TESTDIR) && $(MAKE) compile

run: test

debug: CFLAGS += -g3 -DDEBUG
debug: clean compile
	cd $(TESTDIR) && $(MAKE) debug

memcheck: CFLAGS += -g3 -DDEBUG -DMEMCHECK
memcheck: clean compile
	cd $(TESTDIR) && $(MAKE) memcheck

test: compile
	cd $(TESTDIR) && $(MAKE) run

example: compile
	cd $(TESTDIR) && $(MAKE) example

clean:
	$(RM) -r $(BUILDDIR)
	cd $(TESTDIR) && $(MAKE) clean
