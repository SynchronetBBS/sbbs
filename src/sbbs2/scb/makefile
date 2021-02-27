
# Macros
CC	= bcc
LD	= tlink
SDK	= ..\sdk
MSWAIT	= ..\..\mswait\dos
INCLUDE = \bc31\include;$(SDK);..
LIB     = \bc31\lib
MODEL	= l
CFLAGS  = -d -C -m$(MODEL) -I$(INCLUDE) -w-pro
LDFLAGS = /n /c
OBJS	= xsdk.obj xsdkvars.obj $(MSWAIT)\mswait$(MODEL).obj ..\dos\rciol.obj
HEADERS = $(SDK)\xsdk.h $(SDK)\xsdkdefs.h $(SDK)\xsdkvars.c scb.h

# Implicit C Compile Rule
.c.obj:
    	@echo Compiling $*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $*.c

# Main EXE Link Rule
scb.exe: $(OBJS) scb.obj
    	@echo Linking $< ...
	$(LD) $(LDFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) scb.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib


# All .obj modules
scb.obj: $(HEADERS)

xsdk.obj: $(SDK)\xsdk.c $(HEADERS)
	@echo Compiling $(SDK)\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $(SDK)\$*.c

xsdkvars.obj: $(SDK)\xsdkvars.c $(SDK)\xsdkdefs.h
	@echo Compiling $(SDK)\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $(SDK)\$*.c
