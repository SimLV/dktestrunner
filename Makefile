#******************************************************************************
#  Testrunner - run keeperfx tests, collect events
#******************************************************************************
#   @file Makefile
#      A script used by GNU Make to recompile the project.
#  @par Purpose:
#      Allows to invoke "make all" or similar commands to compile all
#      source code files and link them into executable file.
#  @par Comment:
#      None.
#  @author   Sim
#  @date     16 jul 2020
#  @par  Copying and copyrights:
#      This program is free software; you can redistribute it and/or modify
#      it under the terms of the GNU General Public License as published by
#      the Free Software Foundation; either version 2 of the License, or
#      (at your option) any later version.
#
#******************************************************************************
ifneq (,$(findstring Windows,$(OS)))
  RES  = obj/main.res
  EXEEXT = .exe
  PKGFMT = zip
  PKGOS = win
  CROSS_LFLAGS = -lwsock32
  OSFLAGS =
else
  RES  = 
  EXEEXT =
  PKGFMT = tar.gz
  PKGOS = lin
  # uncomment to crosscompile
  # CROSS_COMPILE = i686-w64-mingw32-

#--enable-shared=no --enable-jit? --build=mingw64 ?
ifeq (, $(CROSS_COMPILE))
  CURSES ?= -DUSE_CURSES=1
  OSFLAGS = -DLINUX=1 $(CURSES)
endif

endif

ifneq (, $(CROSS_COMPILE))
  EXEEXT = .exe
  CROSS_LFLAGS = -lwsock32
  CROSS_HOST = win32
endif

CPP  = $(CROSS_COMPILE)g++
CC   = $(CROSS_COMPILE)gcc
WINDRES = $(CROSS_COMPILE)windres
DLLTOOL = $(CROSS_COMPILE)dlltool
RM = rm -f
MV = mv -f
CP = cp -f
MKDIR = mkdir -p
ECHO = @echo
TAR = tar
ZIP = zip

PCRE_NAME = pcre2-10.35
PCRE_URL = https://ftp.pcre.org/pub/pcre/pcre2-10.35.tar.bz2
PCRE_OBJS = \
lib/$(PCRE_NAME)/obj/pcre2_auto_possess.o \
lib/$(PCRE_NAME)/obj/pcre2_chartables.o \
lib/$(PCRE_NAME)/obj/pcre2_compile.o \
lib/$(PCRE_NAME)/obj/pcre2_config.o \
lib/$(PCRE_NAME)/obj/pcre2_context.o \
lib/$(PCRE_NAME)/obj/pcre2_convert.o \
lib/$(PCRE_NAME)/obj/pcre2_dfa_match.o \
lib/$(PCRE_NAME)/obj/pcre2_error.o \
lib/$(PCRE_NAME)/obj/pcre2_extuni.o \
lib/$(PCRE_NAME)/obj/pcre2_find_bracket.o \
lib/$(PCRE_NAME)/obj/pcre2_jit_compile.o \
lib/$(PCRE_NAME)/obj/pcre2_maketables.o \
lib/$(PCRE_NAME)/obj/pcre2_match.o \
lib/$(PCRE_NAME)/obj/pcre2_match_data.o \
lib/$(PCRE_NAME)/obj/pcre2_newline.o \
lib/$(PCRE_NAME)/obj/pcre2_ord2utf.o \
lib/$(PCRE_NAME)/obj/pcre2_pattern_info.o \
lib/$(PCRE_NAME)/obj/pcre2_script_run.o \
lib/$(PCRE_NAME)/obj/pcre2_serialize.o \
lib/$(PCRE_NAME)/obj/pcre2_string_utils.o \
lib/$(PCRE_NAME)/obj/pcre2_study.o \
lib/$(PCRE_NAME)/obj/pcre2_substitute.o \
lib/$(PCRE_NAME)/obj/pcre2_substring.o \
lib/$(PCRE_NAME)/obj/pcre2_tables.o \
lib/$(PCRE_NAME)/obj/pcre2_ucd.o \
lib/$(PCRE_NAME)/obj/pcre2_valid_utf.o \
lib/$(PCRE_NAME)/obj/pcre2_xclass.o

PCRE_FLAGS = -Ilib/$(PCRE_NAME)/src -DPCRE2_CODE_UNIT_WIDTH=8 -DHAVE_STDINT_H=1 -DHAVE_STDLIB_H=1 \
-DHAVE_STRINGS_H=1 -DHAVE_STRING_H=1 -DHEAP_LIMIT=20000000 -DNEWLINE_DEFAULT=2 \
-DPCRE2_STATIC=1 -DSUPPORT_PCRE2_8=1 -DSUPPORT_UNICODE=1 -DLINK_SIZE=2 \
-DMAX_NAME_SIZE=32 -DMAX_NAME_COUNT=1000 -DMATCH_LIMIT=100000 -DMATCH_LIMIT_DEPTH=MATCH_LIMIT \
-DPARENS_NEST_LIMIT=250

JIT_OBJS = \
lib/$(PCRE_NAME)/obj/sljitLir.o

ifeq (,$(CROSS_COMPILE))
JIT_FLAGS = -DSLJIT_CONFIG_X86_64=1
else
JIT_FLAGS = -DSLJIT_CONFIG_X86_32=1 -DSLJIT_ARGUMENT_CHECKS=0
endif

CUNIT_NAME = CUnit-2.1-3
CUNIT_URL = https://sourceforge.net/projects/cunit/files/CUnit/2.1-3/CUnit-2.1-3.tar.bz2/download
CUNIT_OBJS = \
lib/$(CUNIT_NAME)/obj/Basic.o \
lib/$(CUNIT_NAME)/obj/CUError.o \
lib/$(CUNIT_NAME)/obj/TestRun.o \
lib/$(CUNIT_NAME)/obj/Util.o \
lib/$(CUNIT_NAME)/obj/TestDB.o

CUNIT_FLAGS = -Ilib/$(CUNIT_NAME)/CUnit/Headers

RUNNERBIN = bin/testrunner$(EXEEXT)
LIBS =
OBJS = \
obj/main.o \
obj/compat.o \
obj/jit.o \
obj/regex.o

ALL_OBJS = \
$(OBJS) \
$(PCRE_OBJS) \
$(CUNIT_OBJS) \
$(JIT_OBJS) \
$(RES)

GENSRC = obj/ver_defs.h
LINKLIB = $(SOCKLIB)
INCS =
CXXINCS =
# flags to generate dependency files
DEPFLAGS = -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" 
#DEPFLAGS =
# code optimization flags
OPTFLAGS = -ggdb
#-O3
DBGFLAGS =
# linker flags
LINKFLAGS = $(CROSS_LFLAGS)
# compiler warning generation flags
WARNFLAGS = -Wall -Wno-sign-compare -Wno-unused-parameter
# disabled warnings: -Wextra -Wtype-limits
CXXFLAGS = $(CXXINCS) $(PCRE_FLAGS) -c $(WARNFLAGS) $(DEPFLAGS) $(OPTFLAGS) $(OSFLAGS)
CFLAGS = $(INCS) -c $(WARNFLAGS) $(DEPFLAGS) $(OPTFLAGS) $(OSFLAGS)
LDFLAGS = $(LINKLIB) $(OPTFLAGS) $(DBGFLAGS) $(LINKFLAGS)

# load program version
include version.mk
VER_STRING = $(VER_MAJOR).$(VER_MINOR).$(VER_RELEASE).$(VER_BUILD)

.PHONY: all all-before all-after clean clean-custom package pkg-before zip tar.gz

all: all-before $(RUNNERBIN) all-after

all-before:
	$(MKDIR) obj bin lib/pcre2-10.35/obj

clean: clean-custom
	-${RM} $(ALL_OBJS) $(GENSRC) $(LIBS)
	-${RM} pkg/*
	-$(ECHO) ' '

$(RUNNERBIN): $(ALL_OBJS) $(LIBS)
	-$(ECHO) 'Building target: $@'
	$(CPP) $(ALL_OBJS) -o "$@" $(LDFLAGS) -DQ=z
	-$(ECHO) 'Finished building target: $@'
	-$(ECHO) ' '

obj/%.o: src/%.cpp $(GENSRC)
	-$(ECHO) 'Building file: $<'
	$(CPP) -DA=z $(CXXFLAGS) -o"$@" "$<"
	-$(ECHO) 'Finished building: $<'
	-$(ECHO) ' '

# pcre

lib/$(PCRE_NAME).tar.bz2:
	-$(ECHO) 'Downloading package: $@'
	curl -L -o "$@.dl" "$(PCRE_URL)"
	tar -tf "$@.dl" >/dev/null
	$(MV) "$@.dl" "$@"

lib/$(PCRE_NAME)/src/pcre2.h.generic: lib/$(PCRE_NAME).tar.bz2
	-$(ECHO) 'Extracting package: $<'
	-cd "$(<D)"; tar -xf "$(<F)"
	-$(ECHO) 'Finished extracting: $<'
	-$(ECHO) ' '

lib/$(PCRE_NAME)/src/pcre2_chartables.c: lib/$(PCRE_NAME)/src/pcre2_chartables.c.dist
	$(CP) lib/$(PCRE_NAME)/src/pcre2_chartables.c.dist lib/$(PCRE_NAME)/src/pcre2_chartables.c

lib/$(PCRE_NAME)/src/pcre2.h: lib/$(PCRE_NAME)/src/pcre2.h.generic
	$(CP) $< $@

lib/$(PCRE_NAME)/obj/%.o: lib/pcre2-10.35/src/%.c lib/$(PCRE_NAME)/src/pcre2.h
	$(CC) $(CFLAGS) $(PCRE_FLAGS) -o"$@" "$<" -DQ=1

pcre2: lib/pcre2-10.35/src/pcre2.h

# JIT

lib/$(PCRE_NAME)/obj/%.o: lib/$(PCRE_NAME)/src/sljit/%.c
	$(CC) $(CFLAGS) $(JIT_FLAGS) -mshstk -o"$@" "$<"

# CUNIT

lib/$(CUNIT_NAME).tar.bz2:
	-$(ECHO) 'Downloading package: $@'
	curl -L -o "$@.dl" "$(CUNIT_URL)"
	tar -tf "$@.dl" >/dev/null
	$(MV) "$@.dl" "$@"

lib/$(CUNIT_NAME)/Sources/Framework/CUError.c: lib/$(CUNIT_NAME).tar.bz2
	-$(ECHO) 'Extracting package: $<'
	-cd "$(<D)"; tar -xf "$(<F)"
	$(MKDIR) lib/$(CUNIT_NAME)/obj
	$(CP) lib/$(CUNIT_NAME)/CUnit/Headers/CUnit.h.in lib/$(CUNIT_NAME)/CUnit/Headers/CUnit.h
	-$(ECHO) 'Finished extracting: $<'
	-$(ECHO) ' '

cunit: lib/$(CUNIT_NAME)/Sources/Framework/CUError.c

lib/$(CUNIT_NAME)/obj/%.o: lib/$(CUNIT_NAME)/CUnit/Sources/Framework/%.c
	-$(ECHO) 'Building file: $<'
	$(CC) $(CFLAGS) $(CUNIT_FLAGS) -o"$@" "$<"
	-$(ECHO) 'Finished building: $<'
	-$(ECHO) ' '

lib/$(CUNIT_NAME)/obj/Basic.o: lib/$(CUNIT_NAME)/CUnit/Sources/Basic/Basic.c
	-$(ECHO) 'Building file: $<'
	$(CC) $(CFLAGS) $(CUNIT_FLAGS) -o"$@" "$<"
	-$(ECHO) 'Finished building: $<'
	-$(ECHO) ' '

# obj

obj/%.o: src/%.c $(GENSRC) cunit pcre2
	-$(ECHO) 'Building file: $<'
	$(CC) $(CFLAGS) $(CUNIT_FLAGS) $(PCRE_FLAGS) $(JIT_FLAGS) -o"$@" "$<" -DQ=w
	-$(ECHO) 'Finished building: $<'
	-$(ECHO) ' '

obj/%.res: res/%.rc $(GENSRC)
	-$(ECHO) 'Building resource: $<'
	$(WINDRES) -i "$<" --input-format=rc -o "$@" -O coff 
	-$(ECHO) 'Finished building: $<'
	-$(ECHO) ' '

obj/ver_defs.h: version.mk Makefile
	$(ECHO) \#define VER_MAJOR   $(VER_MAJOR) > "$(@D)/tmp"
	$(ECHO) \#define VER_MINOR   $(VER_MINOR) >> "$(@D)/tmp"
	$(ECHO) \#define VER_RELEASE $(VER_RELEASE) >> "$(@D)/tmp"
	$(ECHO) \#define VER_BUILD   $(VER_BUILD) >> "$(@D)/tmp"
	$(ECHO) \#define VER_STRING  \"$(VER_STRING)\" >> "$(@D)/tmp"
	$(ECHO) \#define PACKAGE_SUFFIX  \"$(PACKAGE_SUFFIX)\" >> "$(@D)/tmp"
	$(MV) "$(@D)/tmp" "$@"

package: pkg-before $(PKGFMT)

pkg-before:
	-${RM} pkg/*
	$(MKDIR) pkg
	$(CP) bin/* pkg/
	$(CP) docs/*_readme.txt pkg/

pkg/%.tar.gz: pkg-before
	-$(ECHO) 'Creating package: $<'
	cd $(@D); \
	$(TAR) --owner=0 --group=0 --exclude=*.tar.gz --exclude=*.zip -zcf "$(@F)" .
	-$(ECHO) 'Finished creating: $<'
	-$(ECHO) ' '

tar.gz: pkg/testrunner-$(subst .,_,$(VER_STRING))-$(PACKAGE_SUFFIX)-$(PKGOS).tar.gz

pkg/%.zip: pkg-before
	-$(ECHO) 'Creating package: $<'
	cd $(@D); \
	$(ZIP) -x*.tar.gz -x*.zip -9 -r "$(@F)" .
	-$(ECHO) 'Finished creating: $<'
	-$(ECHO) ' '

zip: pkg/testrunner-$(subst .,_,$(VER_STRING))-$(PACKAGE_SUFFIX)-$(PKGOS).zip

#******************************************************************************
