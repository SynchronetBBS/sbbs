# GNUmakefile

#########################################################################
# Makefile for Synchronet monitor for Unix								#
# For use with GNU make and GNU C Compiler or Borland Kylix C++			#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# gcc: gmake															#
# Borland (still in testing/debuging stage): gmake bcc=1				#
#																		#
# Optional build targets: dlls, utils, mono, all (default)				#
#########################################################################

# $Id$

#USE_CURSES	:=	1	#Use old curses UIFC implementation
USE_UIFC32	:=	1	# Use new uifc32 implementation

# Macros
ifndef RELEASE
 ifndef DEBUG
  RELEASE	:=	1
 endif
endif

ifdef DEBUG
 BUILD	=	debug
 CFLAGS	+=	-g
else
 BUILD	=	release
endif

ifdef bcc
 CC		=	bc++ -q
 CCPRE	:=	bcc
 CCPP	=	bc++ -q
 LD		=	ilink -q
 CFLAGS +=	-mm -md -D__unix__ -w-csu -w-pch -w-ccc -w-rch -w-par -w-aus
else
 CFLAGS	+=	-MMD -Wall
 CCPRE	:=	gcc
 ifdef BUILD_DEPENDS
  CC		=	../../build/mkdep -a
  CCPP	=	../../build/mkdep -a
  LD		=	echo
  COMPILE_MSG	:= Depending
 else
  CC		=	gcc
  CCPP	=	g++
  LD		=	ld
  COMPILE_MSG	:= Compiling
 endif
endif
OFILE	=	o

LIBFILE	=	.a
UIFC	=	../../uifc/
XPDEV	=	../../xpdev/
SBBS3	=   ../

ifndef os
 os		:=	$(shell uname)
endif
# this line wont work with solaris unless awk in path is actually gawk 
os      :=	$(shell echo $(os) | tr "[A-Z]" "[a-z]")
#os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')
# remove '/' from "os/2"
os      :=  $(shell echo $(os) | tr -d "/")

LIBODIR :=	$(CCPRE).$(os).lib.$(BUILD)
EXEODIR :=	$(CCPRE).$(os).exe.$(BUILD)

DELETE	=	rm -f

CFLAGS	+=	-I../../include/mozilla/js -I$(XPDEV) -I$(UIFC) -I$(SBBS3)
LFLAGS	+=	-lcurses

ifeq ($(os),netbsd)
 CFLAGS	+=	-D__unix__ -I/usr/pkg/include
endif

# So far, only QNX has sem_timedwait() 
ifeq ($(os),qnx)
 LFLAGS := -lm -lsocket
else
 CFLAGS +=      -DUSE_XP_SEMAPHORES
 USE_XP_SEMAPHORES      :=      1
endif

vpath %.c $(UIFC) $(XPDEV) $(SBBS3)
vpath %.cpp $(SBBS3)

OBJS	:=	$(LIBODIR)/filewrap.o \
			$(LIBODIR)/chat.o \
			$(LIBODIR)/sockwrap.o \
			$(LIBODIR)/dirwrap.o \
			$(LIBODIR)/genwrap.o \
			$(LIBODIR)/sbbs_ini.o \
			$(LIBODIR)/ini_file.o \
			$(LIBODIR)/scfglib1.o \
			$(LIBODIR)/scfglib2.o \
			$(LIBODIR)/nopen.o \
			$(LIBODIR)/userdat.o \
			$(LIBODIR)/ars.o \
			$(LIBODIR)/dat_rec.o \
			$(LIBODIR)/str_util.o \
			$(LIBODIR)/crc16.o \
			$(LIBODIR)/load_cfg.o \
			$(LIBODIR)/date_str.o \
			$(LIBODIR)/spyon.o \
			$(LIBODIR)/smblib.o \
			$(LIBODIR)/getstats.o \
			$(LIBODIR)/getmail.o \
			$(LIBODIR)/umonitor.o

ifdef USE_CURSES
 OBJS	+=	$(LIBODIR)/uifcc.o
endif

ifdef USE_UIFC32
 OBJS	+=	$(LIBODIR)/uifc32.o \
		$(LIBODIR)/ciowrap.o
endif

all: $(LIBODIR) $(BUILD_DEPENDS)$(EXEODIR)/umonitor

FORCE$(EXEODIR)/umonitor: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)

$(EXEODIR)/umonitor: $(EXEODIR) $(OBJS) $(BUILD_DEPENDS)
	@echo Linking $@
	@$(CC) $(LFLAGS) $(OBJS) -o $@

# Implicit C Compile Rule for utils
$(LIBODIR)/%.o : %.c $(LIBODIR) $(BUILD_DEPENDS)
   ifndef bcc
	@echo $(COMPILE_MSG) $<
   endif
	@$(CC) $(CFLAGS) -o $@ -c $<

$(LIBODIR):
	mkdir $(LIBODIR)

$(EXEODIR):
	mkdir $(EXEODIR)

clean:
	@$(DELETE) $(LIBODIR)$(SLASH)*
	@$(DELETE) $(EXEODIR)$(SLASH)*

depend:
	@$(DELETE) $(LIBODIR)/.depend
	@$(DELETE) $(EXEODIR)/.depend
	$(MAKE) BUILD_DEPENDS=FORCE

FORCE:

-include $(LIBODIR)/.depend
-include $(EXEODIR)/.depend
-include $(LIBODIR)/*.d
-include $(EXEODIR)/*.d
