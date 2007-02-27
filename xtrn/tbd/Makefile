##################################################################
# Makefile for SBJ (Synchronet Blackjack External Program) 	 #
# For use with Borland C++ or C++ Builder (for Win32)  		 #
# Tabstop=8				 	  		 #
##################################################################

XSDK	= ..\sdk
!ifndef SRC_ROOT
	SRC_ROOT        =       ..\..\src
!endif
!include $(SRC_ROOT)\build\Common.bmake
CFLAGS  =	$(CFLAGS) -I$(XSDK) -DUSE_XPDEV $(XPDEV-MT_CFLAGS)
CFLAGS	= $(CFLAGS) -q -d -H -X- -w-csu -w-pch -w-ccc -w-rch -w-par -w-8004
.path.c = .;$(XSDK)

# Enable auto-dependency checking
.autodepend
.cacheautodepend	

# Main EXE Link Rule
$(TBD): $(OBJS)
    	@echo Linking $@
	@$(CC) $(CFLAGS) $(XPDEV-MT_LDFLAGS) -WM -e$@ $** $(XPDEV-MT_LIBS)
	@copy /y $(TBD) .

# Main EXE Link Rule
$(REROLL): $(REROLL_OBJS)
    	@echo Linking $@
	@$(CC) $(CFLAGS) $(XPDEV-MT_LDFLAGS) -e$@ $** $(XPDEV-MT_LIBS)
	@copy /y $(REROLL) .
