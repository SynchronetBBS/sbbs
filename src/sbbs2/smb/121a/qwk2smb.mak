###############################
# Makefile for QWK2SMB	      #
# For use with Borland C++    #
# Tabstop=8		      #
###############################

# Macros
CC	= bcc
LD	= tlink
INCLUDE = \bc31\include
LIB	= \bc31\lib
MODEL	= s
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE)
LFLAGS	= -n -c
MAIN	= qwk2smb.exe
OBJS	= qwk2smb.obj smblib.obj smbvars.obj
HEADERS = smblib.h smbdefs.h crc32.h

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
	$(CC) $(CFLAGS) -n$(OS) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS)
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0$(MODEL) $(OBJS) 
+ $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

# All .obj modules
qwk2smb.obj:	$(HEADERS)
smbvars.obj:    $(HEADERS)
smblib.obj:     $(HEADERS)
