# GNUmakefile

#########################################################################
# Makefile for cross-platform development "wrappers" test				#
# For use with GNU make and GNU C Compiler or Borland C++ (Kylix 3)								
#
# @format.tab-size 4, @format.use-tabs true								#
#																		#
# usage: gmake [os=target_os] [bcc=1]											
#
#########################################################################

# $Id$

# Macros
DEBUG	=	1		# Comment out for release (non-debug) version
ifdef bcc
CC	=	bc++
CFLAGS	=	-w -D__unix__
else
CC	=	gcc
CFLAGS	=	-Wall -O
endif
SLASH	=	/
OFILE	=	o

ifndef $(os)
os		=	$(shell uname)
$(warning OS not specified on command line, setting to '$(os)'.)
endif

ifdef bcc
ODIR	:=	bcc.$(os)
else
ODIR	:=	gcc.$(os)
endif

DELETE	=	rm -fv

ifeq ($(os),FreeBSD)	# FreeBSD
CFLAGS	+= -D_THREAD_SAFE
LFLAGS	:=	-pthread
else					# Linux / Other UNIX
ifdef bcc
LFLAGS	:=	libpthread.a
else
LFLAGS	:=	-lpthread
endif
endif

ifdef DEBUG
ifdef bcc
CFLAGS	+=	-v -y
else
CFLAGS	+=	-g
endif
CFLAGS	+=	-D_DEBUG
ODIR	:=	$(ODIR).debug
else # RELEASE
ODIR	:=	$(ODIR).release
endif

include objects.mk		# defines $(OBJS)
include targets.mk		# defines all and clean targets

# Implicit C Compile Rule
$(ODIR)/%.o : %.c
	@echo Compiling $<
	@$(CC) $(CFLAGS) -c $< -o$@

# Create output directories
$(ODIR):
	mkdir $(ODIR)

# Executable Build Rule
$(WRAPTEST): $(ODIR)/wraptest.o $(OBJS)
	@echo Linking $@
	@$(CC) $(LFLAGS) $^ -o$@

