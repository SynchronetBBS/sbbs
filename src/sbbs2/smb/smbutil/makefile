#########################################################################
# Makefile for SMBUTIL							#
# For use with Watcom C       						#
# Tabstop=8                                                             #
#									#
# To use this makefile, you must create the following sub-directories:	#
# DOS, OS2, and DOSX.							#
#									#
# *** IMPORTANT NOTE ***
# You must also copy the file \WATCOM\SRC\STARTUP\WILDARGV.C into the	#
# current directory.							#
#########################################################################

# Macros

!ifndef OS
OS	= DOS
!endif

!ifeq OS DOS
CC	= *wcc -ml
!else
CC	= *wcc386
!endif

LD	= *wlink
MAIN	= $(OS)\smbutil.exe
OBJS	= $(OS)\smbutil.obj $(OS)\smblib.obj $(OS)\lzh.obj $(OS)\wildargv.obj
HEADERS = smbutil.h smblib.h smbdefs.h crc32.h lzh.h

!ifeq OS DOS
CFLAGS	= -I=\watcom\h;.. -bt=$(OS) -fo=$(OS)\ -DSMB_GETMSGTXT
!else
CFLAGS	= -I=\watcom\h;.. -bt=$(OS) -fo=$(OS)\ -DSMB_GETMSGTXT -DLZH_DYNAMIC_BUF
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

LFLAGS	= system $(SYSTEM)


# Implicit C Compile Rule
.obj: $(OS)
.c.obj:
	@echo Compiling (I) $< to $@ ...
	$(CC) $(CFLAGS) $<

# Main EXE Link Rule
$(MAIN): $(OBJS)
    	@echo Linking $< ...
	$(LD) $(LFLAGS) file { $(OBJS) }

# All .obj modules
$(OS)\smbutil.obj::   $(HEADERS)

$(OS)\smblib.obj: ..\smblib.c ..\smblib.h ..\smbdefs.h
        @echo Compiling $[@ to $^@ ...
        $(CC) -DSMB_GETMSGTXT $(CFLAGS) $[@

$(OS)\lzh.obj: ..\lzh.c ..\lzh.h
        @echo Compiling $[@ to $^@ ...
        $(CC) $(CFLAGS) $[@
