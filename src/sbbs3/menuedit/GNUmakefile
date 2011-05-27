# Makefile

#########################################################################
# Makefile for Synchronet BBS Menu Editor (menuedit)
# For use with gcc and gmake for *nix      				#
# @format.tab-size 4													#
#########################################################################

# $Id$

# Macros
#DEBUG	=	1				# Comment out for release (non-debug) version

SRC_ROOT        =       ../..
include $(SRC_ROOT)/build/Common.gmake
CFLAGS	+=	$(CIOLIB-MT_CFLAGS) $(UIFC-MT_CFLAGS) $(XPDEV-MT_CFLAGS)
LDFLAGS	+=	$(CIOLIB-MT_LDFLAGS) $(UIFC-MT_LDFLAGS) $(XPDEV-MT_LDFLAGS)

# MenuEdit Build Rule
$(MENUEDIT): $(OBJS)
	@echo Linking $@
	${QUIET}$(CC) $(LDFLAGS) $(MT_LDFLAGS) $(OBJS) -o $@ $(UIFC-MT_LIBS) $(CIOLIB-MT_LIBS) $(XPDEV-MT_LIBS)

