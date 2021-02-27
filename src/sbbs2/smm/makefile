# Macros
CC	= \bc31\bin\bcc
LD	= \bc31\bin\tlink
SDK	= ..\sdk
MSWAIT	= ..\..\mswait\dos
INCLUDE = \bc31\include;$(SDK);..\smb
LIB     = \bc31\lib
MODEL	= l
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE) -w-prot
LFLAGS  = /n /c
OBJS	= xsdk.obj xsdkvars.obj $(MSWAIT)\mswait$(MODEL).obj ..\DOS\rciol.obj
HEADERS = $(SDK)\xsdk.h $(SDK)\xsdkdefs.h $(SDK)\xsdkvars.c smmdefs.h

# Implicit C Compile Rule
.c.obj:
    	@echo Compiling $*.c to $*.obj ...
	$(CC) $(CFLAGS) -c $*.c

# Main EXE Link Rule
smm.exe: $(OBJS) smm.obj
    	@echo Linking $< ...
	$(LD) $(LFLAGS) @&&!
$(LIB)\c0$(MODEL) $(OBJS) smm.obj
!, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib $(LIB)\emu.lib

# All .obj modules
smm.obj: $(HEADERS)

xsdk.obj: $(SDK)\xsdk.c $(HEADERS)
	@echo Compiling $(SDK)\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -c -w+pro $(SDK)\$*.c

xsdkvars.obj: $(SDK)\xsdkvars.c $(SDK)\xsdkdefs.h
	@echo Compiling $(SDK)\$*.c to $*.obj ...
	$(CC) $(CFLAGS) -c -w+pro $(SDK)\$*.c
