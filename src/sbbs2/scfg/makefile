##############################################
# Makefile for Synchronet BBS Config Program #
# For use with Borland C++ for DOS or OS/2   #
##############################################

# Macros

INCLUDE = \bc45\include;..;..\..\uifc;..\..\spawno;..\smb;..\rio
LIB	= \bc45\lib
MAIN	= $(OS)\scfg.exe

!if $d(DOS32)
OS	= DOS32
CC	= \bc45\bin\bcc32
LD	= \bc45\bin\tlink32
CFLAGS	= -w-pro -d -C -WX -I$(INCLUDE) -DSCFG
LFLAGS	= -Tpe -ax -n -c
MAIN	= $(OS)\scfg32.exe
!elif $d(__OS2__)
OS	= OS2
CC	= c:\bcos2\bin\bcc
LD	= c:\bcos2\bin\tlink
CFLAGS	= -w-pro -d -C -I$(INCLUDE) -DSCFG
LFLAGS	= -Toe -ap -c
INCLUDE = c:\bcos2\include;..;..\..\uifc;..\..\spawno;..\smb;..\rio
LIB	= c:\bcos2\lib
MAIN	= $(OS)\scfg4os2.exe
!else
OS      = DOS
CC	= \bc45\bin\bcc
LD	= \bc45\bin\tlink
CFLAGS	= -w-pro -d -C -Y -ml -I$(INCLUDE) -DSCFG
LFLAGS	= -n -c
OBJS	= ..\..\mswait\dos\mswait.obj
!endif

OBJS	= $(OBJS) $(OS)\scfgvars.obj
OVLOBJS = $(OS)\scfg.obj $(OS)\scfgnode.obj\
        $(OS)\scfgxfr1.obj $(OS)\scfgxfr2.obj\
	$(OS)\scfgnet.obj $(OS)\scfgxtrn.obj $(OS)\scfgsys.obj\
	$(OS)\scfgmsg.obj $(OS)\scfgsub.obj\
        $(OS)\scfglib1.obj $(OS)\scfglib2.obj $(OS)\scfgchat.obj\
        $(OS)\uifc.obj $(OS)\scfgmdm.obj $(OS)\smblib.obj\
        $(OS)\wrcnf1.obj $(OS)\wrcnf2.obj
HEADERS = scfg.h ..\..\uifc\uifc.h ..\sbbsdefs.h ..\gen_defs.h ..\scfgvars.c

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
        $(CC) $(CFLAGS) -n$(OS) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS) $(OVLOBJS)
    	@echo Linking $< ...
!ifdef __OS2__
	$(LD) $(LFLAGS) @&&+
$(LIB)\c02.obj $(OBJS) $(OVLOBJS)
+, $*, $*, $(LIB)\os2.lib $(LIB)\c2.lib
!elif $d(DOS32)
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0X32.obj $(OBJS) $(OVLOBJS)
+, $*, $*,$(LIB)\DPMI32 $(LIB)\CW32
!else
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0l.obj $(OBJS) /o $(OVLOBJS)
+, $*, $*, $(LIB)\overlay.lib $(LIB)\cl.lib $(LIB)\mathl.lib\
	$(LIB)\emu.lib
!endif
# Global Variables
$(OS)\scfgvars.obj: ..\scfgvars.c ..\sbbsdefs.h
	@echo Compiling ..\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\$&.c

# Shared Functions
$(OS)\scfglib1.obj: ..\sbbs.h ..\sbbsdefs.h ..\scfglib.h ..\scfglib1.c \
		    ..\scfgvars.c
	@echo Compiling ..\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\$&.c

# Shared Functions
$(OS)\scfglib2.obj: ..\sbbs.h ..\sbbsdefs.h ..\scfglib.h ..\scfglib2.c \
		    ..\scfgvars.c
	@echo Compiling ..\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\$&.c

# Local Menu Routines
$(OS)\uifc.obj: ..\..\uifc\uifc.c ..\..\uifc\uifc.h
	@echo Compiling ..\..\uifc\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\..\uifc\$&.c

# SMBLIB Functions 
$(OS)\smblib.obj: ..\smb\smblib.c ..\smb\smblib.h ..\smb\smbdefs.h
	@echo Compiling ..\smb\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\smb\$&.c

$(OS)\scfg.obj:       $(HEADERS)
$(OS)\scfgnode.obj:   $(HEADERS)
$(OS)\scfgchat.obj:   $(HEADERS)
$(OS)\scfgmsg.obj:    $(HEADERS)
$(OS)\scfgsub.obj:    $(HEADERS)
$(OS)\scfgnet.obj:    $(HEADERS)
$(OS)\scfgsys.obj:    $(HEADERS)
$(OS)\scfgxtrn.obj:   $(HEADERS)
$(OS)\scfgxfr1.obj:   $(HEADERS)
$(OS)\scfgxfr2.obj:   $(HEADERS)
$(OS)\scfgmdm.obj:    $(HEADERS)
$(OS)\wrcnf1.obj:     $(HEADERS)
$(OS)\wrcnf2.obj:     $(HEADERS)
