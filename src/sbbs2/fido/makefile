############################################
# Makefile for Synchronet SBBSFIDO Utility #
# For use with Watcom C/C++ 		   #
############################################

!ifndef OS
OS	= DOS
!endif

!ifeq OS DOS
CC	= *wcc
!else
CC	= *wcc386
!endif

LD	= *wlink
INCLUDE = \watcom\h;..;..\smb
!ifeq OS DOS
CFLAGS	= -I$(INCLUDE) -fh=$*.pch -bt=$(OS) -fo=$(OS)\ -s -ml
!else
CFLAGS	= -I$(INCLUDE) -fh=$*.pch -bt=$(OS) -fo=$(OS)\ -s
!endif

!ifeq OS DOS
SYSTEM  = DOS
!endif
!ifeq OS OS2
SYSTEM  = OS2V2
!endif
!ifeq OS DOSX
SYSTEM  = DOS4G
!endif

LFLAGS  = option stack=16k system $(SYSTEM)

MAIN	= $(OS)\sbbsfido.exe
OBJS	= $(OS)\sbbsfido.obj $(OS)\scfgvars.obj $(OS)\scfglib1.obj &
	  $(OS)\smblib.obj $(OS)\smbvars.obj $(OS)\ars.obj $(OS)\lzh.obj
HEADERS = ..\sbbs.h ..\sbbsdefs.h ..\scfgvars.c &
          ..\smb\smbdefs.h ..\smb\smblib.h ..\smb\smbvars.c

# Implicit C Compile Rule
.c.obj:
	@echo Compiling (I) $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

# Main EXE Link Rule
$(MAIN): $(OBJS) 
	@echo Linking $[@ ...
        $(LD) $(LFLAGS) file { $(OBJS) }

# Global Variables
$(OS)\scfgvars.obj: ..\scfgvars.c ..\sbbsdefs.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

# Shared Functions
$(OS)\scfglib1.obj: ..\scfglib1.c ..\sbbs.h ..\sbbsdefs.h ..\scfgvars.c &
		    ..\scfglib.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

$(OS)\smblib.obj: ..\smb\smblib.c ..\smb\smblib.h ..\smb\smbdefs.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

$(OS)\smbvars.obj: ..\smb\smbvars.c ..\smb\smblib.h ..\smb\smbdefs.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

$(OS)\lzh.obj: ..\smb\lzh.c ..\smb\lzh.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

$(OS)\ars.obj: ..\ars.c ..\ars_defs.h 
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

