############################################
# Makefile for Synchronet SBBSECHO Utility #
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
INCLUDE = \watcom\h;\watcom\h\os2;..\smb;..\rio;..
!ifeq OS DOS
CFLAGS	= -s -I$(INCLUDE) -fh=$*.pch -bt=$(OS) -fo=$(OS)\ -ml -DLZH_DYNAMIC_BUF
!else
CFLAGS	= -s -I$(INCLUDE) -fh=$*.pch -bt=$(OS) -fo=$(OS)\ 
!endif

!ifeq OS NT
SYSTEM  = NT
!endif
!ifeq OS DOS
SYSTEM  = DOS
!endif
!ifeq OS OS2
SYSTEM  = OS2V2
!endif
!ifeq OS DOS4G
SYSTEM  = DOS4G
!endif

LFLAGS  = option stack=20k system $(SYSTEM)

MAIN	= $(OS)\sbbsecho.exe
OBJS	= $(OS)\sbbsecho.obj $(OS)\scfgvars.obj $(OS)\scfglib1.obj &
	  $(OS)\smblib.obj $(OS)\ars.obj $(OS)\lzh.obj &
	  $(OS)\read_cfg.obj
HEADERS = ..\sbbs.h ..\sbbsdefs.h ..\scfgvars.c &
	  ..\smb\smbdefs.h ..\smb\smblib.h

# Implicit C Compile Rule
.c.obj:
	@echo Compiling (I) $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

# Main EXE Link Rule
$(MAIN): $(OBJS) 
	@echo Linking $[@ ...
        $(LD) $(LFLAGS) file { $(OBJS) } option map

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
	$(CC) -DSMB_GETMSGTXT $(CFLAGS) $[@

$(OS)\lzh.obj: ..\smb\lzh.c ..\smb\lzh.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

$(OS)\ars.obj: ..\ars.c ..\ars_defs.h 
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

$(OS)\sbbsecho.obj: sbbsecho.c sbbsecho.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@
