###############################
# Makefile for CHKSMB         #
# For use with Borland C++    #
# Tabstop=8		      #
###############################

# Macros

OS	= DOS
CC	= bcc
LD	= tlink
INCLUDE = \bc31\include;..
LIB	= \bc31\lib
MODEL	= l
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE) -w-pro
LFLAGS	= -n -c

!ifdef __OS2__
OS	= OS2
CC      = c:\bcos2\bin\bcc
LD      = c:\bcos2\bin\tlink
INCLUDE = c:\bcos2\include;..
LIB     = c:\bcos2\lib
CFLAGS	= -d -C -I$(INCLUDE) -w-pro
LFLAGS  = -c -w-srf
!endif

MAIN	= $(OS)\chksmb.exe
OBJS	= $(OS)\chksmb.obj $(OS)\smblib.obj
HEADERS = ..\smblib.h ..\smbdefs.h ..\crc32.h

# Implicit C Compile Rule
{.;..}.c.obj:
	@echo Compiling (I) $< to $@ ...
	$(CC) $(CFLAGS) -n$(OS) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS)
    	@echo Linking $< ...
!ifdef __OS2__
	$(LD) $(LFLAGS) @&&+
$(LIB)\c02.obj $(OBJS) $(LIB)\wildargs.obj
+, $*, $*, $(LIB)\os2.lib $(LIB)\c2.lib
!else
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0$(MODEL) $(OBJS) $(LIB)\wildargs.obj
+, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib
!endif

# All .obj modules
$(OS)\chksmb.obj:	 $(HEADERS)
$(OS)\smblib.obj:	 $(HEADERS)
