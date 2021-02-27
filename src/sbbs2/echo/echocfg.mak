
# Macros
CC	= bcc
LD	= tlink
UIFC	= ..\..\uifc
MSWAIT	= ..\..\mswait
INCLUDE = \bc45\include;$(UIFC);..;..\smb
LIB	= \bc45\lib
MODEL	= l
CFLAGS  = -d -C -m$(MODEL) -I$(INCLUDE) -w-pro
LDFLAGS = /n /c
OBJS	= $(MSWAIT)\dos\mswait$(MODEL).obj uifc.obj read_cfg.obj
HEADERS = $(UIFC)\uifc.h sbbsecho.h

# Implicit C Compile Rule
.c.obj:
    	@echo Compiling $*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $*.c

# Main EXE Link Rule
echocfg.exe: $(OBJS) echocfg.obj
    	@echo Linking $< ...
	$(LD) $(LDFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) echocfg.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib


# All .obj modules
echocfg.obj: $(HEADERS)

uifc.obj: $(UIFC)\uifc.h $(UIFC)\uifc.c
	@echo Compiling $(UIFC)\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $(UIFC)\$*.c

