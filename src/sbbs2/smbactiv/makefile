############################################
# Makefile for Synchronet SMBACTIV Utility #
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
INCLUDE = \watcom\h;\watcom\h\os2;..;..\smb;..\rio


!ifeq OS DOS
CFLAGS	= -I$(INCLUDE) -s -fh=$*.pch -bt=$(OS) -fo=$(OS)\ -ml
!else
CFLAGS	= -I$(INCLUDE) -s -fh=$*.pch -bt=$(OS) -fo=$(OS)\
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
!ifeq OS NT
SYSTEM	= NT
!endif

LFLAGS  = option stack=16k system $(SYSTEM)

MAIN	= $(OS)\smbactiv.exe
OBJS	= $(OS)\smbactiv.obj $(OS)\scfgvars.obj $(OS)\scfglib1.obj &
	  $(OS)\ars.obj $(OS)\scfglib2.obj $(OS)\smblib.obj
HEADERS = ..\sbbs.h ..\sbbsdefs.h ..\gen_defs.h ..\scfgvars.c &
	  ..\smb\smbdefs.h ..\smb\smblib.h

# Implicit C Compile Rule
.c.obj:
	@echo Compiling (I) $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

# Main EXE Link Rule
$(MAIN): $(OBJS) 
#	 @echo Linking $< ...
	$(LD) $(LFLAGS) file { $(OBJS) }

# Global Variables
$(OS)\scfgvars.obj: ..\scfgvars.c ..\sbbsdefs.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

# ARS
$(OS)\ars.obj: ..\ars.c ..\ars_defs.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

# Shared Functions
$(OS)\scfglib1.obj: ..\scfglib1.c ..\sbbs.h ..\sbbsdefs.h ..\scfgvars.c &
	..\scfglib.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) &
-DNO_TEXT_CFG &
-DNO_XTRN_CFG &
-DNO_CMDS_CFG &
-DNO_CHAT_CFG &
$[@

# Shared Functions
$(OS)\scfglib2.obj: ..\scfglib2.c ..\sbbs.h ..\sbbsdefs.h ..\scfgvars.c &
	..\scfglib.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) &
-DNO_TEXT_CFG &
-DNO_XTRN_CFG &
-DNO_CMDS_CFG &
-DNO_CHAT_CFG &
$[@

$(OS)\smblib.obj: ..\smb\smblib.c ..\smb\smblib.h ..\smb\smbdefs.h
	@echo Compiling $[@ to $^@ ...
	$(CC) $(CFLAGS) $[@

