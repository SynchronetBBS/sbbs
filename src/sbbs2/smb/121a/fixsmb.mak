###############################
# Makefile for FIXSMB	      #
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
MAIN	= fixsmb.exe
OBJS	= $(MODEL)\fixsmb.obj $(MODEL)\smblib.obj $(MODEL)\smbvars.obj
HEADERS = smblib.h smbdefs.h crc32.h

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
	$(CC) $(CFLAGS) -n$(MODEL) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS)
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0$(MODEL) $(OBJS) 
+ $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

# All .obj modules
$(MODEL)\fixsmb.obj:	$(HEADERS)
$(MODEL)\smbvars.obj:    $(HEADERS)
$(MODEL)\smblib.obj:     $(HEADERS)
