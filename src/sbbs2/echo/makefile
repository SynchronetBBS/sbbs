############################################
# Makefile for Synchronet SBBSecho Utility #
# For use with Borland C++ for DOS or OS/2 #
############################################

# Macros

!ifdef __OS2__
OS	= OS2
CC	= c:\bcos2\bin\bcc
LD	= c:\bcos2\bin\tlink
INCLUDE = c:\bcos2\include;..;..\smb;..\rio
LIB	= c:\bcos2\lib
CFLAGS  = -d -C -I$(INCLUDE) -w-prot
LFLAGS  = -c

!elif $d(DOSX)
OS	= DOSX
CC	= \bc4\bin\bcc32
LD    	= \bc4\bin\tlink32
INCLUDE = \bc4\include;..;..\smb;..\rio
LIB	= \bc4\lib
CFLAGS	= -w-pro -d -C -WX -I$(INCLUDE) -w-prot
LFLAGS  = -Tpe -ax -n -c -L$(LIB)

!else
OS	= DOS
CC	= \bc45\bin\bcc
LD	= \bc45\bin\tlink
INCLUDE = \bc45\include;..\smb;..\rio;..
LIB	= \bc45\lib
MODEL	= l
CFLAGS	= -d -C -m$(MODEL) -I$(INCLUDE) -DLZH_DYNAMIC_BUF -DSMB_GETMSGTXT -w-pro

LFLAGS	= -c -L$(LIB)
!endif
MAIN	= $(OS)\sbbsecho.exe
OBJS	= $(OS)\sbbsecho.obj $(OS)\scfgvars.obj $(OS)\scfglib1.obj \
	  $(OS)\smblib.obj $(OS)\ars.obj $(OS)\lzh.obj \
	  $(OS)\read_cfg.obj
HEADERS = ..\sbbsdefs.h ..\scfgvars.c ..\smb\smbdefs.h ..\smb\smblib.h 

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
        $(CC) $(CFLAGS) -n$(OS) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS) $(HEADERS)
    	@echo Linking $< ...
!ifdef __OS2__
	$(LD) $(LFLAGS) @&&+
c02.obj $(OBJS)
+, $*, $*, os2.lib c2.lib
!elif $d(DOSX)
	$(LD) $(LFLAGS) @&&+
c0X32.obj $(OBJS)
+, $*, $*, DPMI32 CW32
!else
	$(LD) $(LFLAGS) @&&+
c0$(MODEL) $(OBJS)
+, $*, $*, ..\..\spawno\spawnl.lib mathl emu cl
!endif

# Global Variables
$(OS)\scfgvars.obj: ..\scfgvars.c ..\sbbsdefs.h
	@echo Compiling ..\$&.c to $< ...
	$(CC) $(CFLAGS) -c -n$(OS) ..\$&.c

# Shared Functions
$(OS)\scfglib1.obj: ..\sbbsdefs.h ..\scfglib1.c ..\scfgvars.c
        @echo Compiling ..\$&.c to $< ...
        $(CC) $(CFLAGS) -c -n$(OS) @&&!
-DNO_FILE_CFG
-DNO_XTRN_CFG
-DNO_CMDS_CFG
-DNO_CHAT_CFG
! ..\$&.c

$(OS)\smblib.obj: ..\smb\smblib.c ..\smb\smblib.h ..\smb\smbdefs.h
	@echo Compiling SMBLIB ...
	$(CC) -DSMB_GETMSGTXT $(CFLAGS) -n$(OS) -c ..\smb\smblib.c

$(OS)\lzh.obj: ..\smb\lzh.c ..\smb\lzh.h
        @echo Compiling LZH ...
	$(CC) $(CFLAGS) -n$(OS) -c ..\smb\lzh.c

$(OS)\ars.obj: ..\ars.c ..\ars_defs.h 
        @echo Compiling ..\$&.c to $< ...
	$(CC) $(CFLAGS) -n$(OS) -c ..\ars.c

$(OS)\sbbsecho.obj: sbbsecho.c sbbsecho.h
	@echo Compiling $&.c to $< ...
	$(CC) $(CFLAGS) -n$(OS) -c sbbsecho.c
