#line 1 "CON_IN.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

#define LAST_STAT_LINE 16

#ifdef __WIN32__

#include <windows.h>

uint lkbrd(int mode)
{
	uint c;

if(mode)
	return(kbhit());
c=getch();
if(!c)
	c=(getch()<<8);
return(c);
}

int  lclaes(void)
{
return(0);
}

#elif defined(__OS2__)

void fixkbdmode(void)
{
	KBDINFO 	kbd;

/* Disable Ctrl-C/Ctrl-P processing by OS/2 */
KbdGetStatus(&kbd,0);
if(kbd.fsMask&KEYBOARD_ASCII_MODE) {
    kbd.fsMask|=KEYBOARD_BINARY_MODE;
    kbd.fsMask&=~KEYBOARD_ASCII_MODE;
    KbdSetStatus(&kbd,0); }
}

uint lkbrd(int leave)
{
	KBDKEYINFO	key;

if(leave)
	KbdPeek(&key,0);
else
	KbdCharIn(&key,IO_NOWAIT,0);
if(!(key.fbStatus&KBDTRF_FINAL_CHAR_IN))
	return(0);
if(!(key.fbStatus&0x02) || key.chScan==0xE0)
	return(key.chChar);
return(key.chScan<<8);
}

#endif

/****************************************************************************/
/* Returns character if a key has been hit locally or remotely and responds */
/* to local ctrl/function keys. Does not print character. 					*/
/* Called from functions getkey, msgabort and main_sec						*/
/****************************************************************************/
char inkey(int mode)
{
	static inside;
	uchar str[512],*scrnbuf,x,y,c,atr,*gurubuf,ch=0,*helpbuf;
	int file,con=console;
	uint i,j;
	long l,length;

if(!(node_misc&NM_NO_LKBRD) && (lkbrd(1) || sys_status&SS_FINPUT)) {
	if(sys_status&SS_FINPUT) {
		if(lkbrd(1)==0xff00) {
			close(inputfile);
			sys_status^=SS_FINPUT;
			return(0); }
		if(read(inputfile,&c,1)!=1) {
            close(inputfile);
			sys_status^=SS_FINPUT;
			return(0); }
		if(c)	/* Regular character */
		   i=c;
		else {	/* Scan code */
			if(read(inputfile,&c,1)!=1) {
				close(inputfile);
				sys_status^=SS_FINPUT;
				return(0); }
			i=c<<8; } }
	else
		i=lkbrd(0);
	if(sys_status&SS_LCHAT && curatr!=color[clr_chatlocal])
		attr(color[clr_chatlocal]);
	if(i&0xff)
		ch=i&0xff;
	else {				 /* Alt or Function key hit */
		i>>=8;
		/*		 F1 - F10			Shift-F1 - Alt-F10			F11 - Alt-F12	*/
		if((i>=0x3b &&i<=0x44) || (i>=0x54 &&i<=0x71) || (i>=0x85 &&i<=0x8c)) {
			if(sys_status&SS_FINPUT)
				return(0);							/* MACROS */
			sprintf(str,"%s%sF%d.MAC",text_dir
				,i<0x45 || (i>=0x85 && i<=0x86) ? nulstr
				:i<0x5e || (i>=0x87 && i<=0x88) ? "SHFT-"
				:i<0x68 || (i>=0x89 && i<=0x8a) ? "CTRL-" : "ALT-"
				,i<0x45 ? i-0x3a : i<0x5e ? i-0x53 : i<0x68 ? i-0x5d
				:i<0x72 ? i-0x67 : i<0x87 ? i-0x7a : i<0x89 ? i-0x7c
				:i<0x8b ? i-0x7e : i-0x80);
			if((inputfile=nopen(str,O_RDONLY))!=-1)
				sys_status|=SS_FINPUT;
			return(0); }
		if(i>=0x78 && i<=0x81) {			/* Alt-# Quick Validation */
			if(!(sys_status&SS_USERON)		/* can't quick-validate if not */
				|| !useron.number
				|| !(sys_misc&SM_QVALKEYS)) { /* or not valid user number */
				beep(100,500);					 /* online yet */
				nosound();
				return(0); }
			if((node_misc&NM_SYSPW || SYSOP) && !chksyspass(1))
				return(0);
			beep(1000,100);
			beep(1500,100);
			nosound();
			if(i==0x81)
				i=0;
			else
				i-=0x77;
			useron.level=val_level[i];
			useron.flags1=val_flags1[i];
			useron.flags2=val_flags2[i];
			useron.flags3=val_flags3[i];
			useron.flags4=val_flags4[i];
			useron.exempt=val_exempt[i];
			useron.rest=val_rest[i];
			now=time(NULL);
			if(useron.expire<now && val_expire[i])
				useron.expire=now+((long)val_expire[i]*24L*60L*60L);
			else
				useron.expire+=((long)val_expire[i]*24L*60L*60L);
			putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,str,10));
			putuserrec(useron.number,U_FLAGS1,8,ultoa(useron.flags1,str,16));
			putuserrec(useron.number,U_FLAGS2,8,ultoa(useron.flags2,str,16));
			putuserrec(useron.number,U_FLAGS3,8,ultoa(useron.flags3,str,16));
			putuserrec(useron.number,U_FLAGS4,8,ultoa(useron.flags4,str,16));
			putuserrec(useron.number,U_EXEMPT,8
				,ultoa(useron.exempt,str,16));
			putuserrec(useron.number,U_REST,8
				,ultoa(useron.rest,str,16));
			putuserrec(useron.number,U_EXPIRE,8
				,ultoa(useron.expire,str,16));
			useron.cdt=adjustuserrec(useron.number,U_CDT,10,val_cdt[i]);
			statusline();
			return(0); }
		switch(i) {
			case 0x16:	  /* Alt-U	- Runs Uedit local only with user online */
				sprintf(str,"%.*s",lbuflen,lbuf);
				waitforsysop(1);
				if((scrnbuf=MALLOC((node_scrnlen*80L)*2L))==NULL) {
					errormsg(WHERE,ERR_ALLOC,nulstr,(node_scrnlen*80L)*2L);
					return(CR); }
				gettext(1,1,80,node_scrnlen-1,scrnbuf);
				x=lclwx();
				y=lclwy();
				console&=~(CON_R_ECHO|CON_R_INPUT);
				i=(sys_status&SS_TMPSYSOP);
				if(!SYSOP) sys_status|=SS_TMPSYSOP;
				useredit(0,1);
				if(!i) sys_status&=~SS_TMPSYSOP;
				lputc(FF);
				puttext(1,1,80,node_scrnlen-1,scrnbuf);
				FREE(scrnbuf);
				lclxy(x,y);
				statusline();
				if(online==ON_REMOTE)
					rioctl(IOFI);					/* flush input buffer */
				console=con;
				waitforsysop(0);
				strcpy(lbuf,str);
				lbuflen=strlen(lbuf);
				return(0);
			case 0x1f:	  /* Alt-S	- Toggles Spinning Cursor */
				useron.misc^=SPIN;
				return(0);
			case 0x12:	  /* Alt-E	 - Toggle Remote echo and input */
				if(online!=ON_REMOTE) return(0);
				if(console&CON_R_ECHO) {
					beep(500,50);
					beep(1000,50);
                    nosound();
					waitforsysop(1);
					console&=~(CON_R_ECHO|CON_R_INPUT);
					j=bstrlen(text[Wait]);
					for(i=0;i<j;i++)
						bputs("\b \b"); }
				else {
					beep(1000,50);
					beep(500,50);
                    nosound();
					j=bstrlen(text[Wait]);
					for(i=0;i<j;i++)
						outchar(SP);
					console|=(CON_R_ECHO|CON_R_INPUT);
					rioctl(IOFI);				/* flush input buffer */
					waitforsysop(0); }
				return(0);
			case 0x10:	  /* Alt-Q	 - Toggles remote input */
				if(online!=ON_REMOTE)
					return(0);
				console^=CON_R_INPUT;
				if(console&CON_R_INPUT) {
					beep(1000,50);
					beep(500,50);
                    nosound();
					rioctl(IOSM|PAUSE|ABORT); }
				else {
					beep(500,50);
					beep(1000,50);
					nosound();
					rioctl(IOCM|PAUSE|ABORT); }
				rioctl(IOFI);			/* flush input buffer */
				return(0);
			case 0x19:		/* Alt-P   - Turns off sysop page */
				nosound();
				sys_status&=~SS_SYSPAGE;
				return(0);
			case 0x23:		/* Alt-H   - Hangs up on user */
				hangup();
				return(0);
			case 0x1e:		/* Alt-A   - Alert Sysop when User's done */
				sys_status^=SS_SYSALERT;
				statusline();
				return(0);
			case 0x82:		/* Alt--   - Sub Time (5 min) */
				starttime-=300;
				return(0);
			case 0x83:		/* Alt-+   - Add time (5 min)*/
				starttime+=300;
				return(0);
			case 0x14:		/* Alt-T   - Temp SysOp */
				if(!(sys_status&SS_TMPSYSOP) && node_misc&NM_SYSPW
					&& !chksyspass(1))
					return(0);
				if(sys_misc&SM_L_SYSOP)
					sys_status^=SS_TMPSYSOP;
				statusline();
				return(0);
			case 0x2c:		/* Alt-Z	- Menu of local keys */
				if((scrnbuf=MALLOC((node_scrnlen*80L)*2L))==NULL) {
					errormsg(WHERE,ERR_ALLOC,nulstr,(node_scrnlen*80L)*2L);
					return(CR); }
				gettext(1,1,80,node_scrnlen,scrnbuf);
				sprintf(str,"%sSBBSHELP.DAB",exec_dir);
				if((file=nopen(str,O_RDONLY))==-1)
					lputs("\7Can't open SBBSHELP");
				else {
					length=filelength(file);
					if((helpbuf=MALLOC(length))==NULL)
						lputs("\7Can't allocate for SBBSHELP");
					else {
						read(file,helpbuf,length);
						close(file);
						puttext(13,1,66,24,helpbuf);
						FREE(helpbuf); } }
				while(!lkbrd(1)) mswait(1);
				if(lkbrd(1)==0x2c00)	/* suck up the alt-z */
					lkbrd(0);
				puttext(1,1,80,node_scrnlen,scrnbuf);
				FREE(scrnbuf);
                return(0);
			case 0x2e:		/* Alt-C	- Sysop Chat */
				if(sys_status&SS_LCHAT) {
					sys_status^=SS_LCHAT;
					return(CR); }
				else {
					sys_status|=SS_LCHAT;
					SAVELINE;
					localchat();
					return(0); }
			case 0x2d:	/* Alt-X exit after logoff */
				getnodedat(node_num,&thisnode,1);
				thisnode.misc^=NODE_DOWN;
				putnodedat(node_num,thisnode);
				statusline();
				return(0);
			case 0x47:	/* Home - Same as Ctrl-B */
				return(2);	/* ctrl-b beginning of line */
			case 0x8d:	/* Ctrl-Up Arrow - decrement statusline */
				if(statline) {
					statline--;
					statusline(); }
				return(0);
			case 0x77:	/* Ctrl-Home up - top of status line info */
				statline=1;
				statusline();
				return(0);
			case 0x4b:		/* Left Arrow - same as ctrl-] */
				return(0x1d);
			case 0x4d:		/* Right Arrow - same as ctrl-f */
				return(6);
			case 0x48:		/* Up arrow - same as ctrl-^ */
				return(0x1e);
			case 0x50:		/* Down arrow - same as Ctrl-J */
				return(LF);
			case 0x4f:	  /* End	  - same as Ctrl-E */
				return(5);	/* ctrl-e - end of line */
			case 0x91:	  /* Ctrl-Dn Arrow - increment statusline */
				if(statline<LAST_STAT_LINE) {
					statline++;
					statusline(); }
				return(0);
			case 0x75:	/* Ctrl-End - end of status line info */
				statline=LAST_STAT_LINE;
				statusline();
				return(0);
			case 0x52:	/* Insert */
				return(0x1f);	/* ctrl-minus - insert mode */
			case 0x53:	/* Delete */
				return(0x7f);	/* ctrl-bkspc - del cur char */
			case 0x26:	/* Alt-L  - Capture to cap_fname */
				if(lclaes())
					return(0);
				if(sys_status&SS_CAP) {
					sys_status^=SS_CAP;
					fclose(capfile);
#ifdef __MSDOS__
					freedosmem=farcoreleft();	/* fclose frees memory */
#endif
					statusline();
					return(0); }
				x=lclwx();
				y=lclwy();
				atr=lclatr(LIGHTGRAY<<4);
				STATUSLINE;
				lclxy(1,node_scrnlen);
				lputs("  Filename: ");
				lputc(CLREOL);
				console&=~(CON_R_INPUT|CON_R_ECHO);
				getstr(cap_fname,40,K_UPPER|K_EDIT);
				TEXTWINDOW;
				lclxy(x,y);
				lclatr(atr);
                console=con;
				if(cap_fname[0] &&
					(capfile=fopen(cap_fname,"a"))==NULL)
					bprintf("Couldn't open %s.\r\n",cap_fname);
				else if(cap_fname[0]) {
#ifdef __MSDOS__
					freedosmem=farcoreleft();	/* fnopen allocates memory */
#endif
					sys_status|=SS_CAP; }
				statusline();
				return(0);
			case 0x17:	/* Alt-I  - Interrupt node now */
				getnodedat(node_num,&thisnode,1);
				thisnode.misc^=NODE_INTR;
				putnodedat(node_num,thisnode);
				return(0);
			case 0x13:	/* Alt-R  - Rerun node when caller hangs up */
				getnodedat(node_num,&thisnode,1);
				thisnode.misc^=NODE_RRUN;
				putnodedat(node_num,thisnode);
				statusline();
				return(0);
			case 0x31:	/* Alt-N  - Lock this node */
				getnodedat(node_num,&thisnode,1);
				thisnode.misc^=NODE_LOCK;
				putnodedat(node_num,thisnode);
				statusline();
				return(0);
			case 0x22:	/* Alt-G  - Guru Chat */
				if(sys_status&SS_GURUCHAT) {
					sys_status&=~SS_GURUCHAT;
					return(CR); }
				if(!total_gurus)
					break;
				for(i=0;i<total_gurus;i++)
					if(chk_ar(guru[i]->ar,useron))
						break;
				if(i>=total_gurus)
					i=0;
				sprintf(str,"%s%s.DAT",ctrl_dir,guru[i]->code);
				if((file=nopen(str,O_RDONLY))==-1) {
					errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
					return(0); }
				if((gurubuf=MALLOC(filelength(file)+1L))==NULL) {
					close(file);
					errormsg(WHERE,ERR_ALLOC,str,filelength(file)+1L);
					return(0); }
				read(file,gurubuf,filelength(file));
				gurubuf[filelength(file)]=0;
				close(file);
				localguru(gurubuf,i);
				FREE(gurubuf);
				return(CR);
			case 0x20:	/* Alt-D - Shell to DOS */
				sprintf(str,"%.*s",lbuflen,lbuf);
				waitforsysop(1);
				if((scrnbuf=MALLOC((node_scrnlen*80L)*2L))==NULL) {
					errormsg(WHERE,ERR_ALLOC,nulstr,(node_scrnlen*80L)*2L);
					return(CR); }
				gettext(1,1,80,node_scrnlen,scrnbuf);
				lclatr(LIGHTGRAY);
				x=lclwx();
				y=lclwy();
				lclini(node_scrnlen);
				lputc(FF);
				if(!(node_misc&NM_SYSPW) || chksyspass(1))
					external(comspec,0);
				lclini(node_scrnlen-1);
				puttext(1,1,80,node_scrnlen,scrnbuf);
				FREE(scrnbuf);
				lclxy(x,y);
				if(online==ON_REMOTE)
					rioctl(IOFI);	/* flush input buffer */
				waitforsysop(0);
				strcpy(lbuf,str);
				lbuflen=strlen(lbuf);
				timeout=time(NULL);
				return(0);
			case 0x49:	/* PgUp - local ASCII upload */
				if(sys_status&SS_FINPUT || lclaes() || inside)
					return(0);
				inside=1;
				x=lclwx();
				y=lclwy();
				atr=lclatr(LIGHTGRAY<<4);
				STATUSLINE;
				lclxy(1,node_scrnlen);
				lputs("  Filename: ");
				lputc(CLREOL);
				console&=~(CON_R_INPUT|CON_R_ECHO);
				getstr(str,60,K_UPPER);
				if(str[0] && (inputfile=nopen(str,O_RDONLY))!=-1)
					sys_status|=SS_FINPUT;
				statusline();
				lclxy(x,y);
				lclatr(atr);
				console=con;
				inside=0;
				return(0);
			case 0xff:	/* ctrl-break - bail immediately */
				lputs("\r\nTerminate BBS (y/N) ? ");
				while((i=lkbrd(0))==0);
				if(toupper(i&0xff)=='Y') {  /* Ctrl-brk yes/no */
					lputs("Yes\r\n");
					bail(0); }
				lputs("No\r\n");
				sys_status|=SS_ABORT;
				return(0); }
		return(0); } }

if(!ch && console&CON_R_INPUT && rioctl(RXBC)) {
	if(sys_status&SS_LCHAT && curatr!=color[clr_chatremote])
		attr(color[clr_chatremote]);
	ch=incom();
	if(node_misc&NM_7BITONLY
		&& (!(sys_status&SS_USERON) || useron.misc&NO_EXASCII))
		ch&=0x7f; }
if(ch)
	timeout=time(NULL);
if(ch==3) {  /* Ctrl-C Abort */
	sys_status|=SS_ABORT;
	if(mode&K_SPIN) /* back space once if on spinning cursor */
		bputs("\b \b");
	return(0); }
if(ch==26 && action!=NODE_PCHT) {	 /* Ctrl-Z toggle raw input mode */
    if(mode&K_SPIN)
        bputs("\b ");
    SAVELINE;
	attr(LIGHTGRAY);
    CRLF;
    bputs(text[RawMsgInputModeIsNow]);
    if(console&CON_RAW_IN)
        bputs(text[OFF]);
    else
        bputs(text[ON]);
    console^=CON_RAW_IN;
    CRLF;
	CRLF;
    RESTORELINE;
    lncntr=0;
	if(action!=NODE_MAIN && action!=NODE_XFER)
		return(26);
	return(0); }
if(console&CON_RAW_IN)	{ /* ignore ctrl-key commands if in raw mode */
	if(!ch && (!(mode&K_GETSTR) || mode&K_LOWPRIO || node_misc&NM_LOWPRIO))
		mswait(0);
	return(ch);
}
if(ch<SP) { 				/* Control chars */
	if(ch==LF)				/* ignore LF's in not in raw mode */
		return(0);
	if(ch==15) {	/* Ctrl-O toggles pause temporarily */
		useron.misc^=UPAUSE;
		return(0); }
	if(ch==0x10) {	/* Ctrl-P Private node-node comm */
		if(!(sys_status&SS_USERON))
			return(0);			 /* keep from being recursive */
		if(mode&K_SPIN)
			bputs("\b ");
		if(sys_status&SS_SPLITP) {
			if((scrnbuf=MALLOC((24L*80L)*2L))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,(24L*80L)*2L);
				return(CR); }
			gettext(1,1,80,24,scrnbuf);
			x=lclwx();
			y=lclwy();
			CLS; }
		else {
			SAVELINE;
			attr(LIGHTGRAY);
			CRLF; }
		nodesync(); 	/* read any waiting messages */
		nodemsg();		/* send a message */
		SYNC;
		if(sys_status&SS_SPLITP) {
			lncntr=0;
			CLS;
			for(i=0;i<((24*80)-1)*2;i+=2) {
				if(scrnbuf[i+1]!=curatr)
					attr(scrnbuf[i+1]);
				outchar(scrnbuf[i]); }
			FREE(scrnbuf);
			GOTOXY(x,y); }
		else {
			CRLF;
			RESTORELINE; }
		lncntr=0;
		return(0); }

	if(ch==21) { /* Ctrl-U Users online */
		if(!(sys_status&SS_USERON))
			return(0);
		if(mode&K_SPIN)
			bputs("\b ");
		if(sys_status&SS_SPLITP) {
			if((scrnbuf=MALLOC((24L*80L)*2L))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,(24L*80L)*2L);
				return(CR); }
			gettext(1,1,80,24,scrnbuf);
			x=lclwx();
			y=lclwy();
			CLS; }
        else {
			SAVELINE;
			attr(LIGHTGRAY);
			CRLF; }
		whos_online(1); 	/* list users */
		ASYNC;
		if(sys_status&SS_SPLITP) {
			CRLF;
			nodesync();
			pause();
			CLS;
			for(i=0;i<((24*80)-1)*2;i+=2) {
				if(scrnbuf[i+1]!=curatr)
					attr(scrnbuf[i+1]);
				outchar(scrnbuf[i]); }
			FREE(scrnbuf);
			GOTOXY(x,y); }
		else {
			CRLF;
            RESTORELINE; }
		lncntr=0;
		return(0); }
	if(ch==20 && !(sys_status&SS_SPLITP)) { /* Ctrl-T Time information */
		if(!(sys_status&SS_USERON))
			return(0);
		if(mode&K_SPIN)
			bputs("\b ");
		SAVELINE;
		attr(LIGHTGRAY);
		now=time(NULL);
		bprintf(text[TiLogon],timestr(&logontime));
		bprintf(text[TiNow],timestr(&now));
		bprintf(text[TiTimeon]
			,sectostr(now-logontime,tmp));
		bprintf(text[TiTimeLeft]
			,sectostr(timeleft,tmp));
		SYNC;
		RESTORELINE;
		lncntr=0;
		return(0); }
	if(ch==11 && !(sys_status&SS_SPLITP)) {  /*  Ctrl-k Control key menu */
		if(!(sys_status&SS_USERON))
			return(0);
		if(mode&K_SPIN)
			bputs("\b ");
		SAVELINE;
		attr(LIGHTGRAY);
		lncntr=0;
		bputs(text[ControlKeyMenu]);
		ASYNC;
		RESTORELINE;
		lncntr=0;
		return(0); }

	if(ch==ESC && console&CON_R_INPUT) {
		for(i=0;i<20 && !rioctl(RXBC);i++)
			mswait(1);
		if(i==20)
			return(ESC);
		ch=incom();
		if(ch!='[') {
			ungetkey(ESC);
			ungetkey(ch);
			return(0); }
		i=j=0;
		autoterm|=ANSI; 			/* <ESC>[x means they have ANSI */
		if(!(useron.misc&ANSI) && useron.misc&AUTOTERM && sys_status&SS_USERON
			&& useron.number) {
			useron.misc|=ANSI;
			putuserrec(useron.number,U_MISC,8,ultoa(useron.misc,str,16)); }
		while(i<10 && j<30) {		/* up to 3 seconds */
			if(rioctl(RXBC)) {
				ch=incom();
				if(ch!=';' && !isdigit(ch) && ch!='R') {    /* other ANSI */
					switch(ch) {
						case 'A':
							return(0x1e);	/* ctrl-^ (up arrow) */
						case 'B':
							return(LF); 	/* ctrl-j (dn arrow) */
						case 'C':
							return(0x6);	/* ctrl-f (rt arrow) */
						case 'D':
							return(0x1d);	/* ctrl-] (lf arrow) */
						case 'H':
							return(0x2);	/* ctrl-b (beg line) */
						case 'K':
							return(0x5);	/* ctrl-e (end line) */
						}
					ungetkey(ESC);
					ungetkey('[');
					for(j=0;j<i;j++)
						ungetkey(str[j]);
					ungetkey(ch);
					return(0); }
				if(ch=='R') {       /* cursor position report */
					if(i && !(useron.rows)) {	/* auto-detect rows */
						str[i]=0;
						rows=atoi(str);
						if(rows<5 || rows>99) rows=24; }
					return(0); }
				str[i++]=ch; }
			else {
				mswait(100);
				j++; } }

		ungetkey(ESC);		/* should only get here if time-out */
		ungetkey('[');
		for(j=0;j<i;j++)
			ungetkey(str[j]);
		return(0); }

		}	/* end of control chars */

if(!ch && (!(mode&K_GETSTR) || mode&K_LOWPRIO || node_misc&NM_LOWPRIO))
	mswait(0);
return(ch);
}

