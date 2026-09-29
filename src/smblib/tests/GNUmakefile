# smblib/tests/GNUmakefile

#########################################################################
# Makefile for Synchronet Message Base Library (SMBLIB) tests			#
# GNU make																#
# @format.tab-size 4													#
#																		#
# usage: make		(build the tests)									#
#        make test	(build and run them)								#
#########################################################################

SRC_ROOT = ../..
# Cross platform/compiler definitions
include $(SRC_ROOT)/build/Common.gmake	# defines clean and output directory rules

.DEFAULT_GOAL := all

CUTEST_SRC	= $(SRC_ROOT)$(DIRSEP)cutest

# Tests use assert() and CuTest, so never build them with -DNDEBUG
CFLAGS += -UNDEBUG $(SMBLIB_CFLAGS) -I$(CUTEST_SRC) $(ENCODE_CFLAGS) $(HASH_CFLAGS) $(XPDEV_CFLAGS)
LDFLAGS += $(SMBLIB_LDFLAGS) $(ENCODE_LDFLAGS) $(HASH_LDFLAGS) $(XPDEV_LDFLAGS)
TEST_LIBS = $(SMBLIB_LIBS) $(ENCODE_LIBS) $(HASH_LIBS) $(XPDEV_LIBS)
TEST_DEPS = $(SMBLIB) $(ENCODE_LIB) $(HASH_LIB) $(XPDEV_LIB)

TESTS = $(EXEODIR)$(DIRSEP)smbidxtest \
        $(EXEODIR)$(DIRSEP)smbfiletest

.PHONY: all test
all: $(TESTS)

test: $(TESTS)
	@for t in $(TESTS); do echo Running $$t; $$t || exit 1; done

$(EXEODIR)$(DIRSEP)smbidxtest: smbidxtest.c $(CUTEST_SRC)$(DIRSEP)CuTest.c $(TEST_DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) smbidxtest.c $(CUTEST_SRC)$(DIRSEP)CuTest.c -o $@ $(LDFLAGS) $(TEST_LIBS)

$(EXEODIR)$(DIRSEP)smbfiletest: smbfiletest.c $(CUTEST_SRC)$(DIRSEP)CuTest.c $(TEST_DEPS) | $(EXEODIR)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) smbfiletest.c $(CUTEST_SRC)$(DIRSEP)CuTest.c -o $@ $(LDFLAGS) $(TEST_LIBS)
