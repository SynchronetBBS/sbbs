#########################################################################
# Makefile for Synchronet BBS 											#
# For use with GNU make and GNU C Compiler								#
# @format.tab-size 4													#
#########################################################################

# $id$

# Macros
DEBUG	=	1		# Comment out for release (non-debug) version
CC		=	gcc
LD		=	dllwrap
CFLAGS	=	-mno-cygwin
LFLAGS  =	--target=i386-mingw32 -mno-cygwin

#ifdef DEBUG
#CFLAGS	=	$(CFLAGS) -g -O0 -D_DEBUG 
#LFLAGS	=	$(LFLAGS) 
#ODIR	=	$(ODIR).debug
#else
#ODIR	=	$(ODIR).release
#endif

SBBS	=	sbbs.dll

FTPSRVR	=	ftpsrvr.dll

MAILSRVR=	mailsrvr.dll

OBJS	=	ansiterm.o\
			answer.o\
			ars.o\
			atcodes.o\
			bat_xfer.o\
			bulkmail.o\
			chat.o\
			chk_ar.o\
			con_hi.o\
			con_out.o\
			data.o\
			data_ovl.o\
			date_str.o\
			download.o\
			email.o\
			exec.o\
			execfile.o\
			execfunc.o\
			execmisc.o\
			execmsg.o\
			fido.o\
			file.o\
			filedat.o\
			getkey.o\
			getmsg.o\
			getnode.o\
			getstr.o\
			inkey.o\
			listfile.o\
			load_cfg.o\
			logfile.o\
			login.o\
			logon.o\
			logout.o\
			lzh.o\
			mail.o\
			main.o\
			misc.o\
			msgtoqwk.o\
			netmail.o\
			newuser.o\
			pack_qwk.o\
			pack_rep.o\
			postmsg.o\
			prntfile.o\
			putmsg.o\
			putnode.o\
			qwk.o\
			qwktomsg.o\
			readmail.o\
			readmsgs.o\
			ringbuf.o\
			scandirs.o\
			scansubs.o\
			scfglib1.o\
			scfglib2.o\
			smblib.o\
			sortdir.o\
			str.o\
			telgate.o\
			telnet.o\
			text_sec.o\
			tmp_xfer.o\
			un_qwk.o\
			un_rep.o\
			upload.o\
			userdat.o\
			useredit.o\
			viewfile.o\
			wrappers.o\
			writemsg.o\
			xtrn.o\
			xtrn_sec.o 

LIBS	=	/usr/lib/libwsock32.a /usr/lib/libwinmm.a

HEADERS =	sbbs.h sbbsdefs.h sbbswrap.h sbbsinet.h scfgdefs.h gen_defs.h \
			nodedefs.h smblib.h smbdefs.h text.h

SBBSDEFS=	-DSBBS -DSBBS_EXPORTS -DSMB_GETMSGTXT -DSMBDLL -DLZHDLL \
			-DWRAPPER_DLL

# Implicit C Compile Rule for SBBS.DLL
.c.o:
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $<

# Implicit C++ Compile Rule for SBBS.DLL
.cpp.o:
	$(CC) $(CFLAGS) -c $(SBBSDEFS) $<

ALL: $(SBBS) $(FTPSRVR) $(MAILSRVR)

# SBBS DLL Link Rule
$(SBBS): $(OBJS) ver.o
		$(LD) $(LFLAGS) -o $(SBBS) ver.o $(OBJS) $(LIBS) --output-lib sbbs.a

# Mail Server DLL Link Rule
$(MAILSRVR): mailsrvr.c mxlookup.c
		$(CC) $(CFLAGS) -c -DMAILSRVR_EXPORTS mailsrvr.c mxlookup.c 
		$(LD) $(LFLAGS) -o $(MAILSRVR) mailsrvr.o mxlookup.o $(LIBS) sbbs.a --output-lib mailsrvr.a

# FTP Server DLL Link Rule
$(FTPSRVR): ftpsrvr.c
		$(CC) $(CFLAGS) -c -DFTPSRVR_EXPORTS ftpsrvr.c
		$(LD) $(LFLAGS) -o $(FTPSRVR) ftpsrvr.o $(LIBS) sbbs.a --output-lib ftpsrvr.a

# Dependencies
ansiterm.o:		$(HEADERS)
answer.o:		$(HEADERS)
ars.o:			$(HEADERS) ars_defs.h
bat_xfer.o:    	$(HEADERS)
bulkmail.o:    	$(HEADERS)
chk_ar.o:		$(HEADERS) ars_defs.h
atcodes.o:		$(HEADERS) cmdshell.h
chat.o:        	$(HEADERS)
comio.o:       	$(HEADERS)
con_hi.o:      	$(HEADERS)
con_out.o:     	$(HEADERS)
data.o:        	$(HEADERS)
data_ovl.o:    	$(HEADERS)
date_str.o:    	$(HEADERS)
download.o:    	$(HEADERS)
email.o:		$(HEADERS) cmdshell.h
exec.o:			$(HEADERS) cmdshell.h
execfile.o:		$(HEADERS) cmdshell.h
execfunc.o:		$(HEADERS) cmdshell.h
execmisc.o:		$(HEADERS) cmdshell.h
execmsg.o:		$(HEADERS) cmdshell.h
fido.o:        	$(HEADERS)
file.o:        	$(HEADERS)
filedat.o:    	$(HEADERS)
getkey.o:    	$(HEADERS)
getmsg.o:    	$(HEADERS)
getnode.o:		$(HEADERS)
getstr.o:    	$(HEADERS)
inkey.o:    	$(HEADERS)
listfile.o:		$(HEADERS)
load_cfg.o:		$(HEADERS)
logfile.o:		$(HEADERS)
login.o:		$(HEADERS)
logon.o:    	$(HEADERS) cmdshell.h
logout.o:		$(HEADERS)
lzh.o:			$(HEADERS)
mail.o:	       	$(HEADERS)
main.o:        	$(HEADERS) cmdshell.h
misc.o:        	$(HEADERS) ars_defs.h crc32.h
msgtoqwk.o:		$(HEADERS) qwk.h
netmail.o:     	$(HEADERS) qwk.h
newuser.o:		$(HEADERS)
pack_qwk.o:		$(HEADERS) qwk.h post.h
pack_rep.o:		$(HEADERS) qwk.h post.h
postmsg.o:     	$(HEADERS)
prntfile.o:     $(HEADERS)
putmsg.o:		$(HEADERS)
putnode.o:		$(HEADERS)
qwk.o:			$(HEADERS) qwk.h post.h
qwktomsg.o:		$(HEADERS) qwk.h
readmail.o:     $(HEADERS)
readmsgs.o:		$(HEADERS) post.h
ringbuf.o:		$(HEADERS)
scandirs.o:    	$(HEADERS)
scansubs.o:    	$(HEADERS)
scfglib1.o:    	$(HEADERS) scfglib.h
scfglib2.o:    	$(HEADERS) scfglib.h
smblib.o:    	$(HEADERS)
sortdir.o:    	$(HEADERS)
str.o:         	$(HEADERS)
telgate.o:		$(HEADERS)
telnet.o:		$(HEADERS)
text_sec.o:		$(HEADERS)
tmp_xfer.o:		$(HEADERS)
un_qwk.o:		$(HEADERS) qwk.h
un_rep.o:		$(HEADERS) qwk.h
upload.o:		$(HEADERS)
userdat.o:		$(HEADERS)
useredit.o:    	$(HEADERS)
getuser.o:		$(HEADERS)
ver.o:			$(HEADERS) $(OBJS)
viewfile.o:		$(HEADERS)
wrappers.o:    	$(HEADERS)
writemsg.o:    	$(HEADERS)
xtrn.o:        	$(HEADERS) cmdshell.h
xtrn_sec.o:    	$(HEADERS)
