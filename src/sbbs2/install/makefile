###############################################
# Makefile for Synchronet BBS Install Program #
# For use with Borland C++ for DOS or OS/2    #
###############################################

# Macros
CC	= bcc
LD	= tlink
!ifdef __OS2__
OS	= OS2
INCLUDE = c:\bcos2\include;..;..\..\uifc
LIB	= c:\bcos2\lib
CFLAGS  = -d -C -I$(INCLUDE) -w-pro
LFLAGS  = -c
!else
OS      = DOS
INCLUDE = \bc45\include;..;..\..\uifc
LIB	= \bc45\lib
MODEL	= l
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE) -w-pro
LFLAGS	= -n -c
OBJS	= ..\..\mswait\$(OS)\mswait.obj
!endif
MAIN	= $(OS)\install.exe
OBJS	= $(OBJS) $(OS)\uifc.obj $(OS)\install.obj
HEADERS = ..\..\uifc\uifc.h

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
        $(CC) $(CFLAGS) -n$(OS) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS) 
    	@echo Linking $< ...
!ifdef __OS2__
	$(LD) $(LFLAGS) @&&+
$(LIB)\c02.obj $(OBJS)
+, $*, $*, $(LIB)\os2.lib $(LIB)\c2.lib
!else
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0$(MODEL).obj $(OBJS)
+, $*, $*, $(LIB)\c$(MODEL).lib $(LIB)\math$(MODEL).lib\
	$(LIB)\emu.lib
!endif

# Local Menu Routines
$(OS)\uifc.obj: ..\..\uifc\uifc.c ..\..\uifc\uifc.h
	@echo Compiling ..\..\uifc\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\..\uifc\$&.c

install.obj: 	$(HEADERS)
