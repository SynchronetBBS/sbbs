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
NEED_THREADS	:=	1

XPDEV	:=	./
include $(XPDEV)Common.gmake

# Executable Build Rule
$(WRAPTEST): $(LIBODIR)/wraptest.o $(OBJS)
	@echo Linking $@
	$(QUIET)$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $^

lib: $(EXEODIR)/libxpdev.so $(EXEODIR)/libxpdev.a

$(EXEODIR)/libxpdev.so: $(OBJS)
	@echo Linking $@
	$(QUIET)gcc -shared $(OBJS) -o $(EXEODIR)/libxpdev.so

$(EXEODIR)/libxpdev.a: $(OBJS)
	@echo Linking $@
	$(QUIET)ar -r $(EXEODIR)/libxpdev.a $(OBJS)
	$(QUIET)ranlib $(EXEODIR)/libxpdev.a
