#########################################################################
# Makefile for Synchronet BBS 						#
# For use with Borland c++ for DOS/Win32 or Borland C++ for OS/2 	#
# Tabstop=8		      						#
# usage: make           (under DOS/Windows to build DOS\SBBS.EXE)	#
#        make           (under OS/2 to build OS2\SBBS4OS2.EXE)		#
#        make -DW32	(under Windows 95/NT to build W32\SBBS4W32.EXE) #
#########################################################################

# Macros
BCDIR	= e:\bc31
CC	= $(BCDIR)\bin\bcc
LD	= $(BCDIR)\bin\tlink
INCLUDE = $(BCDIR)\include;smb;rio;..\spawno
LIB     = $(BCDIR)\lib
DEFINES = -DSBBS 

!if $d(__OS2__)
CC	= c:\bcos2\bin\bcc
LD	= c:\bcos2\bin\tlink
INCLUDE = c:\bcos2\include;smb;rio
LIB	= c:\bcos2\lib
OS	= OS2
CFLAGS	= -N -d -C -sm -I$(INCLUDE) $(DEFINES) -n$(OS) -w-pro
LFLAGS	= -m -s -c -Toe -ap -S:18000
#16k still caused stack overflows?
#-w-srf?
MAIN	= $(OS)\sbbs4os2.exe

!elif $d(W32)

OS	= W32 
CC      = \bc45\bin\bcc32
LD      = \bc45\bin\tlink32
INCLUDE = \bc45\include;smb;rio
LIB	= \bc45\lib
CFLAGS	= -N -d -C -H -tWC -tWM -I$(INCLUDE) $(DEFINES) -n$(OS) -w-pro
LFLAGS  = -m -n -c -Tpe -ax -S:18000
MAIN	= $(OS)\sbbs4w32.exe

!else
OS      = DOS
MODEL	= l
CFLAGS	= -N -d -C -Y -O1 -m$(MODEL) -I$(INCLUDE) $(DEFINES) -n$(OS) -w-pro
LFLAGS	= -l -m -n -c 
MAIN	= $(OS)\sbbs.exe
!endif


!if $d(__OS2__)
OBJS	= OS2\riolib.obj
!elif $d(W32)
OBJS	= W32\riolib.obj
!else
OBJS	= DOS\rciol.obj ..\mswait\DOS\mswait$(MODEL).obj
!endif
OBJS	= $(OBJS) $(OS)\etext.obj $(OS)\scfgvars.obj $(OS)\vars.obj \
	  $(OS)\xtrn.obj 

OVLOBJS = $(OS)\ars.obj\
	  $(OS)\chk_ar.obj\
	  $(OS)\comio.obj \
	  $(OS)\con_in.obj \
	  $(OS)\con_out.obj \
          $(OS)\con_mid.obj \
          $(OS)\con_hi.obj \
	  $(OS)\atcodes.obj \
	  $(OS)\data.obj \
	  $(OS)\data_ovl.obj\
	  $(OS)\getuser.obj\
	  $(OS)\userrec.obj\
	  $(OS)\getnode.obj\
	  $(OS)\putnode.obj\
	  $(OS)\exec.obj \
	  $(OS)\execmisc.obj \
	  $(OS)\execfunc.obj \
	  $(OS)\execmsg.obj \
	  $(OS)\execfile.obj \
	  $(OS)\file.obj $(OS)\file_ovl.obj \ 
	  $(OS)\chat.obj $(OS)\fido.obj \
	  $(OS)\initdata.obj \ 
	  $(OS)\listfile.obj \
	  $(OS)\viewfile.obj \
	  $(OS)\logio.obj \
	  $(OS)\login.obj \
	  $(OS)\logon.obj \
	  $(OS)\logout.obj \
	  $(OS)\newuser.obj \
	  $(OS)\mail.obj \  
 	  $(OS)\main.obj $(OS)\main_ovl.obj $(OS)\main_sec.obj \
	  $(OS)\main_wfc.obj \
          $(OS)\msg1.obj \
	  $(OS)\msg2.obj\
	  $(OS)\putmsg.obj\
	  $(OS)\postmsg.obj\
	  $(OS)\email.obj\
	  $(OS)\netmail.obj\
	  $(OS)\bulkmail.obj\
          $(OS)\misc.obj\
	  $(OS)\qwk.obj\
	  $(OS)\qwktomsg.obj\
	  $(OS)\msgtoqwk.obj\
	  $(OS)\pack_qwk.obj\
	  $(OS)\pack_rep.obj\
	  $(OS)\un_qwk.obj\
	  $(OS)\un_rep.obj\
	  $(OS)\readmsgs.obj\
	  $(OS)\scfglib1.obj\
	  $(OS)\scfglib2.obj\
	  $(OS)\str.obj $(OS)\text_sec.obj $(OS)\useredit.obj\
	  $(OS)\xfer_lo.obj $(OS)\xfer_mid.obj $(OS)\xfer_hi.obj\
	  $(OS)\xtrn_ovl.obj $(OS)\xfer_sec.obj $(OS)\lzh.obj\
	  $(OS)\smblib.obj

HEADERS = sbbs.h sbbsdefs.h gen_defs.h nodedefs.h text.h vars.c scfgvars.c \
	  smb\smblib.h smb\smbdefs.h

# Implicit C Compile Rule
{.}.c.obj:
	@echo Compiling (I) $< to $@ ...
	$(CC) $(CFLAGS) -c $<

# Main EXE Link Rule
$(MAIN): $(OBJS) $(OVLOBJS) $(OS)\ver.obj
    	@echo Linking $< ...
!if $d(__OS2__)
	$(LD) $(LFLAGS) @&&+
$(OBJS) $(LIB)\c02.obj $(OS)\ver.obj $(OVLOBJS)
+, $*, $*, $(LIB)\os2.lib $(LIB)\c2mt.lib
!elif $d(W32)
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0X32.OBJ $(OBJS) $(OS)\ver.obj $(OVLOBJS)
+, $*, $*, $(LIB)\import32.lib $(LIB)\cw32mt.lib
!else
	$(LD) $(LFLAGS) @&&+
$(LIB)\c0$(MODEL) $(OBJS) $(OS)\ver.obj /o $(OVLOBJS), \
$*, $*, ..\spawno\spawnl $(LIB)\overlay $(LIB)\c$(MODEL) \
	$(LIB)\math$(MODEL) $(LIB)\emu
+
!endif

$(OS)\smblib.obj: smb\smblib.c smb\smblib.h smb\smbdefs.h
	@echo Compiling SMBLIB ...
	$(CC) $(CFLAGS) -c -DSMB_GETMSGTXT smb\smblib.c

$(OS)\lzh.obj: smb\lzh.c smb\lzh.h 
	@echo Compiling LZH ...
	$(CC) $(CFLAGS) -c -DLZH_DYNAMIC_BUF smb\lzh.c

$(OS)\riolib.obj: rio\riolib.c rio\riolib.h rio\riodefs.h
	@echo Compiling RIOLIB ...
	$(CC) $(CFLAGS) -c rio\riolib.c

etext.h: etext.dat
	@echo Creating new encrypted text (ETEXT) Code and Header files...
	genetext
			
# All .obj modules
$(OS)\ars.obj:		$(HEADERS) ars_defs.h
$(OS)\chk_ar.obj:	$(HEADERS) ars_defs.h
$(OS)\atcodes.obj:	$(HEADERS) cmdshell.h
$(OS)\chat.obj:        	$(HEADERS)
$(OS)\comio.obj:       	$(HEADERS)
$(OS)\con_in.obj:      	$(HEADERS)
$(OS)\con_out.obj:     	$(HEADERS)
$(OS)\con_mid.obj:     	$(HEADERS)
$(OS)\con_hi.obj:      	$(HEADERS)
$(OS)\data.obj:        	$(HEADERS)
$(OS)\data_ovl.obj:    	$(HEADERS)
$(OS)\exec.obj:		$(HEADERS) cmdshell.h
$(OS)\execmisc.obj:	$(HEADERS) cmdshell.h
$(OS)\execfunc.obj:	$(HEADERS) cmdshell.h
$(OS)\execmsg.obj:	$(HEADERS) cmdshell.h
$(OS)\execfile.obj:	$(HEADERS) cmdshell.h
$(OS)\etext.obj:       	etext.h
$(OS)\fido.obj:        	$(HEADERS)
$(OS)\file.obj:        	$(HEADERS)
$(OS)\file_ovl.obj:    	$(HEADERS)
$(OS)\initdata.obj:    	$(HEADERS)
$(OS)\listfile.obj:	$(HEADERS)
$(OS)\viewfile.obj:	$(HEADERS)
$(OS)\logio.obj:       	$(HEADERS)
$(OS)\login.obj:	$(HEADERS)
$(OS)\logon.obj:    	$(HEADERS) cmdshell.h
$(OS)\logout.obj:	$(HEADERS)
$(OS)\main.obj:        	$(HEADERS) cmdshell.h
$(OS)\mail.obj:	       	$(HEADERS)
$(OS)\main_ovl.obj:    	$(HEADERS) etext.h
$(OS)\main_sec.obj:    	$(HEADERS)
$(OS)\main_wfc.obj:	$(HEADERS) qwk.h etext.h
$(OS)\misc.obj:        	$(HEADERS) ars_defs.h smb\crc32.h
$(OS)\msg1.obj:	       	$(HEADERS)
$(OS)\msg2.obj:     	$(HEADERS)
$(OS)\newuser.obj:	$(HEADERS)
$(OS)\putmsg.obj:	$(HEADERS)
$(OS)\email.obj:	$(HEADERS) cmdshell.h
$(OS)\postmsg.obj:     	$(HEADERS)
$(OS)\netmail.obj:     	$(HEADERS) qwk.h
$(OS)\bulkmail.obj:    	$(HEADERS)
$(OS)\qwk.obj:		$(HEADERS) qwk.h post.h etext.h
$(OS)\qwktomsg.obj:	$(HEADERS) qwk.h
$(OS)\msgtoqwk.obj:	$(HEADERS) qwk.h
$(OS)\pack_qwk.obj:	$(HEADERS) qwk.h post.h
$(OS)\pack_rep.obj:	$(HEADERS) qwk.h post.h
$(OS)\un_qwk.obj:	$(HEADERS) qwk.h
$(OS)\un_rep.obj:	$(HEADERS) qwk.h
$(OS)\readmsgs.obj:	$(HEADERS) post.h
$(OS)\scfglib1.obj:    	$(HEADERS) scfglib.h
$(OS)\scfglib2.obj:    	$(HEADERS) scfglib.h
$(OS)\scfgvars.obj:    	$(HEADERS)
$(OS)\str.obj:         	$(HEADERS)
$(OS)\getnode.obj:	$(HEADERS)
$(OS)\putnode.obj:	$(HEADERS)
$(OS)\text_sec.obj:	$(HEADERS)
$(OS)\getuser.obj:	$(HEADERS)
$(OS)\userrec.obj:	$(HEADERS)
$(OS)\useredit.obj:    	$(HEADERS)
$(OS)\vars.obj:        	$(HEADERS)
$(OS)\ver.obj:		$(OBJS) $(OVLOBJS) $(HEADERS)
$(OS)\xfer_lo.obj:     	$(HEADERS)
$(OS)\xfer_mid.obj:    	$(HEADERS)
$(OS)\xfer_hi.obj:     	$(HEADERS)
$(OS)\xfer_sec.obj:	$(HEADERS)
$(OS)\xtrn.obj:        	$(HEADERS) cmdshell.h
$(OS)\xtrn_ovl.obj:    	$(HEADERS)
