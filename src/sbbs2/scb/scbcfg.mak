
# Macros
CC	= bcc
LD	= tlink
SDK	= ..\sdk
UIFC	= ..\..\uifc
MSWAIT	= ..\..\mswait
INCLUDE = \bc45\include;$(UIFC)
LIB	= \bc45\lib
MODEL	= l
CFLAGS  = -d -C -w-pro -m$(MODEL) -I$(INCLUDE)
LDFLAGS = /n /c
OBJS	= $(MSWAIT)\dos\mswait$(MODEL).obj uifc.obj
HEADERS = $(UIFC)\uifc.h scb.h

# Implicit C Compile Rule
.c.obj:
    	@echo Compiling $*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $*.c

# Main EXE Link Rule
scbcfg.exe: $(OBJS) scbcfg.obj
    	@echo Linking $< ...
	$(LD) $(LDFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) scbcfg.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib


# All .obj modules
scbcfg.obj: $(HEADERS)

uifc.obj: $(UIFC)\uifc.h $(UIFC)\uifc.c
	@echo Compiling $(UIFC)\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -DSCFG -c $(UIFC)\$*.c

