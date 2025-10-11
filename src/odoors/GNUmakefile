#  OpenDoors 6.23
#  (C} Copyright 1991 - 1997 by Brian Pirie. All Rights Reserved.
#
#  Oct-2001 door32.sys/socket modifications by Rob Swindell (www.synchro.net}
#
#
#         File: Win32.mak
#
#  Description: Makefile used to build the Win32 OpenDoors libraries from
#               the sources. Usage is described below.
#
#    Revisions: Date          Ver   Who  Change
#               ---------------------------------------------------------------
#               Aug 09, 2003  6.23  SH   *nix port
#
###############################################################################
#
# USAGE INFORMATION
#
###############################################################################
#
# Command Line:   make -fWin32.mak
#                     or
#                 nmake /f Win32.mak
#
###############################################################################
#
# CONFIGURATION
#
# Customize this section of the makefile to provide the relevant information
# for your compiler, assembler (if any} and build environment.
#
###############################################################################
# Compiler executable file name. Use:
#
#                  tcc - For Borland Turbo C and Turbo C++
#                  bcc - For Borland C++
#                   cl - For Microsoft compilers
#
CC	?=	gcc
#
#------------------------------------------------------------------------------
#
# Linker executable file name. Use:
#
#                tlink - For Borland compilers
#                 link - For Microsoft compilers
#
# Get OS name
gcc_machine := $(findstring mingw32,$(shell ${CC} -dumpmachine))
gcc_w64     := $(findstring w64,$(shell ${CC} -dumpmachine))
gcc_x86_64  := $(findstring x86_64,$(shell ${CC} -dumpmachine))
ifeq ($(gcc_machine),mingw32)
 CFLAGS +=      -DMSVCRT_VERSION=0x0800
 ifeq ($(gcc_x86_64),x86_64)
  OS            := Win64
  os            := Win64
  win           := 64
  CFLAGS        += -m64
  LDFLAGS       += -m64
  machine_uname := x64
 else
  OS            := Win32
  os            := Win32
  win           := 32
  CFLAGS        += -m32
  LDFLAGS       += -m32
  WINDRESFLAGS  += -Fpe-i386
  machine_uname := x86
 endif
 ifeq ($(gcc_w64),w64)
  CFLAGS += -DDISABLE_MKSTEMP_DEFINE
 endif
else
 OS      :=      $(shell uname)
 os	:=	$(shell echo $(OS) | tr '[A-Z]' '[a-z]' | tr ' ' '_')
endif
OBJDIR	:=	objs-$(OS)/
LIBDIR	:=	libs-$(OS)/
EXEDIR	:=	exe-$(OS)/

LD	?=	gcc

ifdef DEBUG
 CFLAGS	+=	-g -DOD_DEBUG
 BUILDTYPE	:=	debug
else
 BUILDTYPE	:=	release
endif
#
#------------------------------------------------------------------------------
#
# Compiler command-line flags.
#
CFLAGS	+=	-fPIC
LDFLAGS	+=	-fPIC
CFLAGS	+=	-O2 -I../xpdev
ifeq ($(OS),Darwin)
 CFLAGS		+=	-D__unix__
 LDFLAGS	:=	$(CFLAGS) -L$(LIBDIR) -dynamiclib -single_module
else
 LDFLAGS	:=	-L$(LIBDIR) -shared
endif
CFLAGS	+=	-Wall
ifeq ($(shell if [ -f /usr/include/inttypes.h ] ; then echo YES ; fi),YES)
 CFLAGS	+=	-DHAS_INTTYPES_H
endif

# /MTd /Zi - for debug
#
#------------------------------------------------------------------------------
#
# Link flags.
#
ifdef XPDEV_LIB
LDFLAGS	+=	-L$(XPDEV_LIB)
endif
#
#------------------------------------------------------------------------------
#
# Output directories. customize for your own preferences. Note that trailing
# backslash (\} characters are required.
#
SHFLAGS		+=	-shared
LIB_PREFIX	:=	lib
SHLIB_PREFIX	:=	lib
LIB_SUFFIX	:=	.6.2
EXTRA_LIBS	:=
ifeq ($(os),darwin)
 SHLIB		?=	.dylib
else
 ifdef win
  SHLIB		?=	.dll
  EXTRA_LIBS += -lwsock32 -lgdi32 -lcomctl32 -Wl,--subsystem,windows
  SHLIB_PREFIX	:=
  LIB_SUFFIX	:=
 else
  SHLIB		?=	.so
  SHFLAGS	+=	-Wl,-Bsymbolic
 endif
endif
STATICLIB	:=	.a
OBJFILE 	:=	.o
ifdef PROFILE
	CFLAGS	+=	-pg
	SHLIB	:=	_p${SHLIB}
	STATICLIB	:=	_p.a
endif
#
###############################################################################
#
# DEPENDENCIES
#
# You won't normally have to change anything after this point in this makefile.
#
###############################################################################
#
# Define primary target.
#
all: ${OBJDIR} ${LIBDIR} $(EXEDIR) ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB} \
    ${LIBDIR}${LIB_PREFIX}ODoors${STATICLIB} $(EXEDIR)ex_chat $(EXEDIR)ex_diag \
    $(EXEDIR)ex_hello $(EXEDIR)ex_music $(EXEDIR)ex_vote

ifdef XPDEV_LIB
all: $(EXEDIR)ex_ski
endif
#
#------------------------------------------------------------------------------
#
# Name of all headers.
#
HEADERS= ${HEADERDIR}ODCom.h\
         ${HEADERDIR}ODCore.h\
         ${HEADERDIR}ODGen.h\
         ${HEADERDIR}ODInEx.h\
         ${HEADERDIR}ODInQue.h\
         ${HEADERDIR}ODKrnl.h\
         ${HEADERDIR}ODPlat.h\
         ${HEADERDIR}ODRes.h\
         ${HEADERDIR}ODScrn.h\
         ${HEADERDIR}ODStat.h\
         ${HEADERDIR}ODSwap.h\
         ${HEADERDIR}ODTypes.h\
         ${HEADERDIR}ODUtil.h\
         ${HEADERDIR}OpenDoor.h
#
#------------------------------------------------------------------------------
#
# Build DLL from objects.
#
OBJECTS := ${OBJDIR}ODAuto${OBJFILE}\
         ${OBJDIR}ODBlock${OBJFILE}\
         ${OBJDIR}ODCFile${OBJFILE}\
         ${OBJDIR}ODCmdLn${OBJFILE}\
         ${OBJDIR}ODCom${OBJFILE}\
         ${OBJDIR}ODCore${OBJFILE}\
         ${OBJDIR}ODDrBox${OBJFILE}\
         ${OBJDIR}ODEdit${OBJFILE}\
         ${OBJDIR}ODEdStr${OBJFILE}\
         ${OBJDIR}ODEmu${OBJFILE}\
         ${OBJDIR}ODGetIn${OBJFILE}\
         ${OBJDIR}ODGraph${OBJFILE}\
         ${OBJDIR}ODInEx1${OBJFILE}\
         ${OBJDIR}ODInEx2${OBJFILE}\
         ${OBJDIR}ODInQue${OBJFILE}\
         ${OBJDIR}ODKrnl${OBJFILE}\
         ${OBJDIR}ODList${OBJFILE}\
         ${OBJDIR}ODLog${OBJFILE}\
         ${OBJDIR}ODMulti${OBJFILE}\
         ${OBJDIR}ODPlat${OBJFILE}\
         ${OBJDIR}ODPCB${OBJFILE}\
         ${OBJDIR}ODPopup${OBJFILE}\
         ${OBJDIR}ODPrntf${OBJFILE}\
         ${OBJDIR}ODRA${OBJFILE}\
         ${OBJDIR}ODScrn${OBJFILE}\
         ${OBJDIR}ODSpawn${OBJFILE}\
         ${OBJDIR}ODStand${OBJFILE}\
         ${OBJDIR}ODStat${OBJFILE}\
         ${OBJDIR}ODStr${OBJFILE}\
         ${OBJDIR}ODUtil${OBJFILE}\
         ${OBJDIR}ODWCat${OBJFILE}\
	 ${OBJDIR}ODWin${OBJFILE}
#         ${OBJDIR}ODoor.res
#         ${OBJDIR}odsys${OBJFILE}\	this file is missing

ifdef win
	OBJECTS += ${OBJDIR}ODFrame${OBJFILE}
endif

ODOORS_SHLIB	:= ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
ODOORS_LIB	:= ${LIBDIR}${LIB_PREFIX}ODoors${STATICLIB}

${OBJDIR}:
	mkdir ${OBJDIR}

${LIBDIR}:
	mkdir ${LIBDIR}

${EXEDIR}:
	mkdir ${EXEDIR}

$(OBJDIR)%$(OBJFILE) : %.c
	$(CC) $(CFLAGS) -o $@ -c $<

${ODOORS_SHLIB} : ${OBJECTS}
	$(CC) $(SHFLAGS) -o ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}${LIB_SUFFIX} ${OBJECTS} ${EXTRA_LIBS}
ifndef win
	ln -fs ${SHLIB_PREFIX}ODoors${SHLIB}${LIB_SUFFIX} ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
endif

${ODOORS_LIB} : ${OBJECTS}
	${AR} -r ${LIBDIR}${LIB_PREFIX}ODoors${STATICLIB} ${OBJECTS}
	${RANLIB} ${LIBDIR}${LIB_PREFIX}ODoors${STATICLIB}
	
${EXEDIR}ex_chat: ex_chat.c ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
	$(CC) $(LDFLAGS) $(CFLAGS) ex_chat.c -o $@ ${ODOORS_SHLIB}

${EXEDIR}ex_diag: ex_diag.c ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
	$(CC) $(LDFLAGS) $(CFLAGS) ex_diag.c -o $@ ${ODOORS_SHLIB}

${EXEDIR}ex_hello: ex_hello.c ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
	$(CC) $(LDFLAGS) $(CFLAGS) ex_hello.c -o $@ ${ODOORS_SHLIB}

${EXEDIR}ex_music: ex_music.c ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
	$(CC) $(LDFLAGS) $(CFLAGS) ex_music.c -o $@ ${ODOORS_SHLIB}

${EXEDIR}ex_ski: ex_ski.c ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
	$(CC) $(LDFLAGS) $(CFLAGS) $(LDFLAGS) ex_ski.c -o $@ ${ODOORS_SHLIB} -lxpdev

${EXEDIR}ex_vote: ex_vote.c ${LIBDIR}${SHLIB_PREFIX}ODoors${SHLIB}
	$(CC) $(LDFLAGS) $(CFLAGS) ex_vote.c ../xpdev/filewrap.c -o $@ ${ODOORS_SHLIB} -DMULTINODE_AWARE

#
#------------------------------------------------------------------------------
