###############################
# Makefile for CHKSMB         #
# For use with Borland C++    #
# Tabstop=8		      #
###############################

# Macros

CC	= bcc
LD	= tlink
INCLUDE = \bc31\include
LIB	= \bc31\lib
MODEL	= l
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE)
LFLAGS	= -n -c
MAIN	= chksmb.exe
OBJS	= $(MODEL)\chksmb.obj $(MODEL)\smblib.obj $(MODEL)\smbvars.obj
HEADERS = smblib.h smbdefs.h crc32.h

!ifdef __OS2__
CC      = c:\bcos2\bin\bcc
LD      = c:\bcos2\bin\tlink
INCLUDE = c:\bcos2\include;smb
LIB     = c:\bcos2\lib
CFLAGS	= -d -C -I$(INCLUDE)
LFLAGS  = -c -w-srf
!endif

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
	$(CC) $(CFLAGS) -n$(MODEL) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS)
    	@echo Linking $< ...
!ifdef __OS2__
	$(LD) $(LFLAGS) @&&+
$(LIB)\c02.obj $(OBJS) $(LIB)\wildargs.obj
+, $*, $*, $(LIB)\os2.lib $(LI$(MODEL)\B)\c2.lib
!else
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0$(MODEL) $(OBJS) $(LIB)\wildargs.obj
+, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib
!endif

# All .obj modules
$(MODEL)\chksmb.obj:	 $(HEADERS)
$(MODEL)\smbvars.obj:	 $(HEADERS)
$(MODEL)\smblib.obj:	 $(HEADERS)
