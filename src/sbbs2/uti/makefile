###############################
# Makefile for Synchronet UTI #
# For use with Turbo C++      #
# Tabstop=8		      #
###############################

# Macros
CC	= bcc
LD	= tlink
INCLUDE = \bc31\include;..;..\smb;..\rio
LIB     = \bc31\lib
MODEL	= l
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE) -w-pro
LFLAGS  = /n /c
OBJS	= scfgvars.obj scfglib1.obj uti.obj ars.obj
SMB	= smblib.obj 
LZH	= lzh.obj
HEADERS = ..\sbbs.h ..\sbbsdefs.h ..\scfgvars.c \
	  ..\smb\smblib.h ..\smb\smbdefs.h

# Implicit C Compile Rule
.c.obj:
        @echo Compiling $*.c to $*.obj ...
	$(CC) $(CFLAGS) -o$*.obj -c $*.c

all: utilist.exe utihigh.exe utiexprt.exe utilstrd.exe utiimprt.exe utiver.exe

utilist.exe: $(OBJS) utilist.obj
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) utilist.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

utihigh.exe: $(OBJS) $(SMB) utihigh.obj
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) $(SMB) utihigh.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

utiexprt.exe: $(OBJS) $(SMB) $(LZH) utiexprt.obj
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) $(SMB) $(LZH) utiexprt.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

utiimprt.exe: $(OBJS) $(SMB) $(LZH) utiimprt.obj
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) $(SMB) $(LZH) utiimprt.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

utilstrd.exe: $(OBJS) utilstrd.obj
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) utilstrd.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

utiver.exe: uti.h utiver.obj utiver.c
	bcc -w-pro utiver.c

# SMBLIB Functions
smblib.obj: ..\smb\smblib.c ..\smb\smbdefs.h ..\smb\smblib.h
	@echo Compiling ..\$*.c to $*.obj ...
        $(CC) $(CFLAGS) -c ..\smb\$*.c

# SMB LZH Functions
lzh.obj: ..\smb\lzh.c ..\smb\lzh.h 
	@echo Compiling ..\$*.c to $*.obj ...
        $(CC) $(CFLAGS) -c ..\smb\$*.c

# Global Variables
scfgvars.obj: ..\scfgvars.c ..\smb\smbdefs.h 
	@echo Compiling ..\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -c ..\$*.c

ars.obj: ..\ars.c ..\ars_defs.h
        @echo Compiling ..\$&.c to $< ...
	$(CC) $(CFLAGS) -c ..\ars.c


# Shared Functions
scfglib1.obj: ..\sbbs.h ..\sbbsdefs.h ..\scfglib1.c ..\scfgvars.c ..\scfglib.h
	@echo Compiling ..\$*.c to $*.obj ...
        $(CC) $(CFLAGS) -c ..\$*.c

utiimprt.obj:	uti.h
utiexprt.obj:	uti.h
utilstrd.obj:	uti.h
utihigh.obj:	uti.h
utilist.obj:	uti.h
