/* xsdk.c */

/* Synchronet External Program Software Development Kit	*/

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/***************************** Revision History *****************************\

			Initial version for use with Synchronet v1a r6
	1.0á	
			Added bgotoxy() macro
			Added mnehigh and mnelow vars for control of the mnemonic colors
			Added sys_nodes and node_num variables to xtrn_sdk.c
			Added MAX_NODES to xtrn_sdk.h
			Added printfile() function to xtrn_sdk.c
			Added rputs() (Raw put string)
			Added getstr() (Get string)
			Added redrwstr() (Redraw string)
			Added stripattr() (String attributes)
			Added sys_op var and the reading from the xtrn.dat
			Removed user_min and the reading from the xtrn.dat
			Changed read_xtrn_dat() to initdata()
			Added ctrl-break handler to xtrn_sdk
			Changed xtrn.dat format (again), adding system operator,
				guru name, user ml, tl, birthdate and sex.
			Added username() function
			Renamed xtrn_sdk.* to xsdk.* and xtrnvars.c to xsdkvars.c
				and xtrndefs.h to xsdkdefs.h
			Added fexist() function
	1.0
			Ctrl-p is now switched into ctrl-^ by SBBS
			Fixed relative data_dir bug in username()
	1.01
			Added flength() function and lowered disk activity
			Lowered MAX_CARDS from 20 to 10 and made the re-shuffling happen
				less often.
	1.02
			Fixed bug in attr() for monochrome users
	1.03
			Made warning and timeout times variables (sec_warn and sec_timeout)
			Added SYSOP macro
			Made it so sysop won't get timeout
			Added user's phone number to XTRN.DAT
			Added modem and com port information to XTRN.DAT
			Added ahtoul function
			Changed getstr commands Ctrl-L to Ctrl-R and Ctrl-P to Ctrl-\
	1.04
			Added intercommunication between the external programs and users
			on the BBS or other external programs written with XSDK.
			Added rprintf() function
			Made many changes to getstr() function
	2.00
			Added DESQview awareness
			Removed difftime() function calls completely
			Added ungetkey() function
			Added memory address of last modem status register for com routines
				so we can track DCD incase user hangs up.
			Added checkline function that does the checking of DCD.
			Added new bug-free fdelay() routine to replace TC's delay() that
				crashes multi-taskers such as DESQview and Windows
	2.01
			Added external program name access for user listings.
			Added last node to send message to remembering and defaulting.
			Added MALLOC and FREE macros for memory model non-specific memory
				allocation.
	2.02
			Added INTRSBBS.DAT support for Synchronet shrinking to run programs
				written with XSDK (new with v1b rev 01).
			Added user's main flags, transfer flags, exemptions, and
				restrictions to XTRN.DAT
			Added support for the NODE_PAGE action (paging for private chat)
				when listing nodes (printnodedat()).
			Added user expiration date support to XTRN.DAT
	2.03
			Fixed bug with com_base variable being messed up.
			New messaging system supported (for v1b r2 and higher)
				(putnmsg and getnmsg functions).
	2.10
			Added support for file retrieving node status display.
			NOPEN collision notice only appears after 25 retries.
	2.11
			Changed getnmsg function to not use IXB files.
			Changed getsmsg function to not re-open for truncate.
			Added user address, location, and zip/postal code suppport.
			Added support for local arrow keys, home, end, ins, and del.
			Remote keys ^] (back one space) and ^BkSpc (del) supported now.
			Added support for high-bit Ctrl-A codes (for cursor positioning)
			Removed file locking check - slowed down initialization sometimes.
			Change user_ml to user_level, removed user_tl, changed user_mf to
				user_flags1, changed user_tf to user_flags2, and added
				user_flags3 and user_flags4.
			cls() now updates lncntr like it should have.
			Added ctrl-break handler so that users can abort printfile()
			If a ctrl-c is received by inkey, the aborted flag is set.
			Removed fdelay from XSDK and replaced with mswait for better
				multitasker performance
	2.20
			New mswait that support OS2/Windows, DOS Idle, and non-DV modes.
			XTRN.DAT passes mode of mswait configured in node setup.
	2.21
			Added user's real name/company name (user_realname) to XTRN.DAT
	2.22
			Added usernumber() function to get user number from a user name.
	2.23
			DTE rate (com_rate) now a ulong (instead of uint) to allow 115.2K
	2.24
			New K_AUTODEL mode for getstr() function, to be used in conjunction
				with the K_EDIT mode. Makes overwriting existing strings very
				easy for users.
			Supports intelligent timeslice APIs while waiting for a key with
				getkey() and with getstr() if K_LOWPRIO mode is used.
			Hitting Ctrl-C sets the 'aborted' variable to 1.
			Time zone and daylight savings automatically initialized to 0.
			Modem strings up to 63 chars in XTRN.DAT now supported.
			Fixed 10 character zip code bug in XSDKVARS.C.
			Node directories (node_dir) up to 127 chars now supported.
			nopen() can now open DENYNONE if passed access O_DENYNONE.
	2.30
			Added support for the following Ctrl-A codes: ;,.<>[]A
			Changed definitions of TAB, SP, etc. to hex
	2.31
			C restriction disallows users to use Ctrl-P.
			T exemption disables "Time's up" message.
			Added center() function for outputting centered lines of text.
			Added auto-pause to cls() and outchar() functions when clearing
				the screen and lncntr is greater than 1
			Changed bstrlen() to not count control characters (CR,LF,TAB,etc)
			XSDK is now Watcom C++ compatible (although SBJ.C and SBL.C aren't)
			XSDK.H is now *.CPP compatible
			Added support for Ctrl-AQ (reset pause) and Ctrl-AZ (premature EOF)
	2.32
			Change bstrlen(char *str) to bstrlen(uchar *str)
			Fixed bug in getstr() when word-wrapping a line that contains
				ctrl-a codes and the input string did not begin at column 1.
			Added user_dce variable (initialized by initdata from XTRN.DAT)
			Fixed printnodedat() to not show Waiting for call (M)
			Fixed typo in C restriction Ctrl-P message.
			Moved call of checkline() in getkey() to immediately abort when
				user hangs-up, even when keys are in the input buffer.
			Added setmode() call to initdata() to set stderr to binary mode
			Changed putchar() to write() in outchar() to elminate LF to CRLF
				expansion
	2.33
			Improved cls() routine for auto-pause feature.
			Added get_term() automatic RIP and WIP terminal detection function.
	2.34
			Added exec_dir, text_dir, and temp_dir variables to XTRN.DAT
				format and initdata() function.
			Added _fullpath() calls to initdata() to fix dir paths.
			Added sys_id (QWK ID) to XTRN.DAT format and initdat() func.
			Added node_misc to XTRN.DAT format and initdata() function.
			If NM_LOWPRIO (low priority string input) is toggled on in
				node_misc, then time-slices are always given up during input.
			XSDK is now Symantec C++ compatible
	2.40
			node_misc was being read as a decimal number (it's stored in
				the XTRN.DAT as hex), thus causing time-slice APIs to not
				function correctly.
	2.41
			Ctrl-T is now intercepted by inkey() and displays the time the
				program was launched, the current time, time used, and time
				left (similar to SBBS).
			Users are now warned of their last 5 minutes available (like SBBS).
	2.42
			Added Microsoft/Borland 32-bit compiler compatibility
			Added socket (Telnet) compatibility
			Changed Ctrl-Minus (Insert key) to Ctrl-V
			Changed Ctrl-V (Center text key) to Ctrl-L
			Added support for telnet nodes (connection=0xffff)
	3.00
			Fixed problem with clear screen (form feed) in node messages.
			checkline() now exits when the remote user disconnects on 32-bit 
				programs. Use atexit() to add cleanup code.
	3.01
			Eliminated warnings in ctrl_a() when compiled with VC++ 6.0.
			Added Linux/GCC support (xsdkwrap.c, xsdkwrap.h, and xsdkinet.h)
	3.10	
			Added COMPILER_DESC and PLATFORM_DESC macros to xsdkwrap.h
			Added support for no local console (XSDK_MODE_NOCONSOLE)
				- This is now the default mode when building 32-bit programs
			Eliminated use of ungetch() in favor of ungetkey() - more secure
	3.11	

\****************************************************************************/

#include "xsdk.h"

#ifdef _WINSOCKAPI_
WSADATA WSAData;		// WinSock data
#endif

char *xsdk_ver="3.11";
ulong xsdk_mode=XSDK_MODE_NOCONSOLE;

/****************************************************************************/
/* This allows users to abort the listing of text by using Ctrl-C           */
/****************************************************************************/
int cbreakh(void)	/* ctrl-break handler */
{
	aborted=1;
	return(1);		/* 1 to continue, 0 to abort */
}

/****************************************************************************/
/* Performs printf() using bbs bputs function								*/
/****************************************************************************/
int bprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int chcount;

	va_start(argptr,fmt);
	chcount=vsprintf(sbuf,fmt,argptr);
	va_end(argptr);
	bputs(sbuf);
	return(chcount);
}

/****************************************************************************/
/* Performs printf() using bbs rputs function								*/
/****************************************************************************/
int rprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int chcount;

	va_start(argptr,fmt);
	chcount=vsprintf(sbuf,fmt,argptr);
	va_end(argptr);
	rputs(sbuf);
	return(chcount);
}

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable) 	*/
/****************************************************************************/
void bputs(char *str)
{
	ulong l=0;

	while(str[l] && !aborted) {
		if(str[l]==1) {				/* ctrl-a */
			ctrl_a(str[++l]);		/* skip the ctrl-a */
			if(str[l]=='Z')         /* Ctrl-AZ marks premature end of file */
				break;
			l++; }					/* skip the attribute code */
		else
			outchar(str[l++]); }
}

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable) 	*/
/* Does not process ctrl-a codes (raw output)								*/
/* Max length of str is 64 kbytes											*/
/****************************************************************************/
void rputs(char *str)
{
	ulong l=0;

	while(str[l])
		outchar(str[l++]);
}

/****************************************************************************/
/* Returns the number of characters in 'str' not counting ctrl-ax codes		*/
/* or the null terminator													*/
/****************************************************************************/
int bstrlen(uchar *str)
{
	int i=0;

	while(*str) {
		if(*str<SP) {	/* ctrl char */
			if(*str==1) /* ctrl-A */
				str++;
			else if(*str!=CR && *str!=LF && *str!=FF)
				i++; }
		else
			i++;
		if(!(*str))
			break;
		str++; }
	return(i);
}

/****************************************************************************/
/* Outputs the string 'str' centered for an 80 column display               */
/* Automatically appends "\r\n" to output                                   */
/****************************************************************************/
void center(char *str)
{
	 int i,j;

	j=bstrlen(str);
	for(i=0;i<(80-j)/2;i++)
		outchar(SP);
	bputs(str);
}

#ifndef __16BIT__

char	outbuf[5000];
ulong	outbufbot=0;
ulong	outbuftop=0;
sem_t	output_sem;

void output_thread(void* arg)
{
	int		i,len;
	char	str[256];

	sem_init(&output_sem,0,0);

	while(client_socket!=INVALID_SOCKET) {
		if(outbufbot==outbuftop) {
			sem_init(&output_sem,0,0);
			sem_wait(&output_sem);
			continue; 
		}

		if(outbuftop>outbufbot)
			len=outbuftop-outbufbot;
		else
			len=sizeof(outbuf)-outbufbot;
		i=send(client_socket,outbuf+outbufbot,len,0);
		if(i!=len) {
			sprintf(str,"!XSDK Error %d (%d) sending on socket %d\n"
				,i,ERROR_VALUE,client_socket);
#ifdef _WIN32
			OutputDebugString(str);
#else
			fprintf(stderr,"%s",str);
#endif
		}
		outbufbot+=len;
		if(outbufbot>=sizeof(outbuf))
			outbufbot=0;
	}
}
#endif

/****************************************************************************/
/* Outputs one character to the screen. Handles, pause, saving and			*/
/* restoring lines, etc.													*/
/****************************************************************************/
void outchar(char ch)
{

#ifndef __16BIT__
	if(client_socket!=INVALID_SOCKET) {
		ulong	top=outbuftop+1;

		if(top==sizeof(outbuf))
			top=0;
		if(top!=outbufbot) {
			outbuf[outbuftop++]=ch;
			if(outbuftop==sizeof(outbuf))
				outbuftop=0;
			sem_post(&output_sem);
		}
	}
#endif

	if(con_fp!=NULL)
		write(fileno(con_fp),&ch,1);

	if(ch==LF) {
		lncntr++;
		lbuflen=0;
		tos=0; 
	}
	else if(ch==FF) {
		if(lncntr>1) {
			lncntr=0;
			CRLF;
			bpause(); 
		}
		lncntr=0;
		lbuflen=0;
		tos=1; 
	}
	else if(ch==BS) {
		if(lbuflen)
			lbuflen--; 
	}
	else {
		if(!lbuflen)
			latr=curatr;
		if(lbuflen>=LINE_BUFSIZE) lbuflen=0;
		lbuf[lbuflen++]=ch; 
	}
	if(lncntr==user_rows-1) {
		lncntr=0;
		bpause(); 
	}
}

/****************************************************************************/
/* Prints PAUSE message and waits for a key stoke							*/
/****************************************************************************/
void bpause(void)
{
	char	ch;
	uchar	tempattrs=curatr,*msg="\1_\1r\1h[Hit a key] ";
	int		i,j;

	lncntr=0;
	bputs(msg);
	j=bstrlen(msg);
	ch=getkey(K_UPPER);
	for(i=0;i<j;i++)
		bputs("\b \b");
	attr(tempattrs);
	if(ch=='N' || ch=='Q')
		aborted=1;
}

/****************************************************************************/
/* Prompts user for Y or N (yes or no) and CR is interpreted as a Y			*/
/* Returns 1 for Y or 0 for N												*/
/* Called from quite a few places											*/
/****************************************************************************/
char yesno(char *str)
{
	char ch;

	bprintf("\1_\1b\1h%s (Y/n) ? \1w",str);
	while(1) {
		ch=getkey(K_UPPER);
		if(ch=='Y' || ch==CR) {
			bputs("Yes\r\n");
			return(1); }
		if(ch=='N' || aborted) {
			bputs("No\r\n");
			return(0); 
		} 
	}
}

/****************************************************************************/
/* Prompts user for N or Y (no or yes) and CR is interpreted as a N			*/
/* Returns 1 for N or 0 for Y												*/
/* Called from quite a few places											*/
/****************************************************************************/
char noyes(char *str)
{
	char ch;

	bprintf("\1_\1b\1h%s (y/N) ? \1w",str);
	while(1) {
		ch=getkey(K_UPPER);
		if(ch=='N' || ch==CR || aborted) {
			bputs("No\r\n");
			return(1); }
		if(ch=='Y') {
			bputs("Yes\r\n");
			return(0); 
		} 
	}
}

/****************************************************************************/
/* Outputs a string highlighting characters preceeded by a tilde with the	*/
/* color specified in mnehigh and the rest of the line is in color mnelow.	*/
/* If the user doesn't have ANSI, it puts the character following the tilde */
/* in parenthesis.															*/
/****************************************************************************/
void mnemonics(char *str)
{
	long l;

	attr(mnelow);
	l=0L;
	while(str[l]) {
		if(str[l]=='~' && str[l+1]) {
			if(!(user_misc&ANSI))
				outchar('(');
			l++;
			attr(mnehigh);
			outchar(str[l]);
			l++;
			if(!(user_misc&ANSI))
				outchar(')');
			attr(mnelow); 
		}
		else
			outchar(str[l++]); 
	}
	attr(LIGHTGRAY);
}

int keyhit()
{
#ifndef __16BIT__
	int cnt=0;
	if(ioctlsocket(client_socket,FIONREAD,&cnt))
    	return(0);
    return(cnt);
#else
	return(kbhit());
#endif
}

/****************************************************************************/
/* If a key has been pressed, the ASCII code is returned. If not, 0 is		*/
/* returned. Ctrl-P and Ctrl-U are intercepted here.						*/
/****************************************************************************/
char inkey(long mode)
{
	static in_ctrl_p;
	uchar ch=0,hour,min,sec;
	long tleft;
	int i=0;
	time_t now;

#ifndef __16BIT__
	char	str[256];
	ulong	cnt=0;

	if(client_socket!=INVALID_SOCKET) {
		i=ioctlsocket(client_socket,FIONREAD,&cnt);
		if(i) {
			sprintf(str,"!XSDK Error %d (%d) checking readcnt on socket %d\n"
				,i,ERROR_VALUE,client_socket);
#ifdef _WIN32
			OutputDebugString(str);
#else
			fprintf(stderr,"%s",str);
#endif
		}
	}

	if(i==0 && cnt) 
		recv(client_socket,&ch,1,0);
	else
#endif

	if(keybufbot!=keybuftop) {
		ch=keybuf[keybufbot++];
		if(keybufbot==KEY_BUFSIZE)
			keybufbot=0; }
	else if(!(xsdk_mode&XSDK_MODE_NOCONSOLE) && kbhit()) {
		i=getch();
#ifdef __unix__
		if(i==LF) i=CR;	/* Enter key returns Ctrl-J on Unix! (ohmygod) */
#endif
		if(i==0 || i==0xE0) {			/* Local Alt or Function key hit */
			i=getch();
			switch(i) {
				case 0x47:	/* Home - Same as Ctrl-B */
					return(2);	/* ctrl-b beginning of line */
				case 0x4b:		/* Left Arrow - same as ctrl-] */
					return(0x1d);
				case 0x4d:		/* Right Arrow - same as ctrl-f */
					return(6);
				case 0x48:		/* Up arrow - same as ctrl-^ */
					return(0x1e);
				case 0x50:		/* Down arrow - same as CR */
					return(CR);
				case 0x4f:	  /* End	  - same as Ctrl-E */
					return(5);  /* ctrl-e - end of line */
				case 0x52:	/* Insert */
					return(0x1f);	/* ctrl-minus - insert mode */
				case 0x53:	/* Delete */
					return(0x7f);   /* ctrl-bkspc - del cur char */
				}
			return(0); } 
		ch=i;
	}

	if(ch==0x10 || ch==0x1e) {	/* Ctrl-P or Ctrl-^ */
		if(in_ctrl_p || !ctrl_dir[0])	/* keep from being recursive */
			return(0);
		in_ctrl_p=1;
		SAVELINE;
		CRLF;
		nodemsg();
		CRLF;
		RESTORELINE;
		lncntr=0;
		in_ctrl_p=0;
		return(0); }

	if(ch==20) { /* Ctrl-T Time left online */
		SAVELINE;
		attr(LIGHTGRAY);
		now=time(NULL);
		checktimeleft();
		CRLF;
		bprintf("\r\nStart     : %.24s",ctime(&starttime));
		bprintf("\r\nNow       : %.24s",ctime(&now));
		i=now-starttime;
		hour=(i/60)/60;
		min=(i/60)-(hour*60);
		sec=i-((min+(hour*60))*60);
		bprintf("\r\nTime Used : %02u:%02u:%02u",hour,min,sec);
		tleft=timeleft-(now-starttime);
		hour=(tleft/60)/60;
		min=(tleft/60)-(hour*60);
		sec=tleft-((min+(hour*60))*60);
		bprintf("\r\nTime Left : %02u:%02u:%02u\r\n\r\n",hour,min,sec);
		RESTORELINE;
		lncntr=0;
		return(0); }

	if(ch==21) { /* Ctrl-U Users online */
		if(!ctrl_dir[0])
			return(0);
		SAVELINE;
		CRLF;
		whos_online(1);
		CRLF;
		RESTORELINE;
		lncntr=0;
		return(0); }

#ifndef __16BIT__
	if(ch==LF) 
		ch=0;		/* Ignore LF of Telnet CR/LF sequence */
#endif

	if(ch==3)
		aborted=1;
	else if(aborted)
		ch=3;

	if(!ch && (!(mode&K_GETSTR) || mode&K_LOWPRIO|| node_misc&NM_LOWPRIO))
		mswait(1);
	return(ch);
}

/****************************************************************************/
/* Waits for remote or local user to hit a key. Inactivity timer is checked */
/* and hangs up if inactive for 4 minutes. Returns key hit, or uppercase of */
/* key hit if mode&K_UPPER or key out of KEY BUFFER. Does not print key.	*/
/****************************************************************************/
char getkey(long mode)
{
	char	ch,warn=0;
	long	tleft;
	time_t	timeout,now;

	aborted=lncntr=0;
	timeout=time(NULL);
	do {
		checkline();
		ch=inkey(mode);
		now=time(NULL);
		if(ch) {
			if(mode&K_NUMBER && isprint(ch) && !isdigit(ch))
				continue;
			if(mode&K_ALPHA && isprint(ch) && !isalpha(ch))
				continue;
			if(ch==LF) continue;
			if(mode&K_UPPER)
				return(toupper(ch));
			return(ch); 
		}
		checktimeleft();

		tleft=timeleft-(now-starttime);
		if((tleft/60)<(5-timeleft_warn)) {	/* Running out of time warning */
			timeleft_warn=5-(tleft/60);
			SAVELINE;
			bprintf("\1n\1h\r\n\7\r\nYou only have \1r\1i%u\1n\1h minute%s "
				"left.\r\n\r\n"
				,((ushort)tleft/60)+1,(tleft/60) ? "s" : "");
			RESTORELINE; 
		}

		if(now-timeout>=(time_t)sec_warn && !warn)		/* Inactivity warning */
			for(warn=0;warn<5;warn++)
				outchar(7);
	} while(now-timeout<(time_t)sec_timeout);

	bputs("\r\nInactive too long.\r\n");
	exit(0);
	return(0);	/* never gets here, but makes compiler happy */
}

/****************************************************************************/
/* If remote user, checks DCD to see if user has hung up or not.			*/
/****************************************************************************/
void checkline(void)
{
#ifdef __16BIT__
	if(com_port && !((*msr)&DCD)) exit(0);
#else
	char	str[256];
	char	ch;
	int		i;
	fd_set	socket_set;
	struct timeval timeout;

	if(client_socket!=INVALID_SOCKET) {
		FD_ZERO(&socket_set);
		FD_SET(client_socket,&socket_set);
		timeout.tv_sec=0;
		timeout.tv_usec=100;

		if((i=select(client_socket+1,&socket_set,NULL,NULL,&timeout))>0) {
			if((i=recv(client_socket,&ch,1,MSG_PEEK))!=1) {
				sprintf(str,"!XSDK Error %d (%d) checking state of socket %d\n"
					,i,ERROR_VALUE,client_socket);
	#ifdef _WIN32
				OutputDebugString(str);
	#else
				fprintf(stderr,"%s",str);
				fflush(stderr);
	#endif
				exit(0);
			}
		}
	}
#endif
}

/****************************************************************************/
/* Waits for remote or local user to hit a key that is contained inside str.*/
/* 'str' should contain uppercase characters only. When a valid key is hit, */
/* it is echoed (upper case) and is the return value.						*/
/* If max is non-zero and a number is hit that is not in str, it will be	*/
/* returned with the high bit set. If the return of this function has the	*/
/* high bit set (&0x8000), just flip the bit (^0x8000) to get the number.	*/
/****************************************************************************/
int getkeys(char *instr,int max)
{
	char	str[256];
	uchar	ch,n=0;
	int		i=0;

	sprintf(str,"%.*s",sizeof(str)-1,instr);
	strupr(str);
	while(!aborted) {
		ch=getkey(K_UPPER);
		if(max && ch>0x7f)	/* extended ascii chars are digits to isdigit() */
			continue;
		if(ch && !n && (strchr(str,ch))) { 	/* return character if in string */
			outchar(ch);
			attr(LIGHTGRAY);
			CRLF;
			return(ch); 
		}
		if(ch==CR && max) {             /* return 0 if no number */
			attr(LIGHTGRAY);
			CRLF;
			if(n)
				return(i|0x8000);		/* return number plus high bit */
			return(0); 
		}
		if(ch==BS && n) {
			bputs("\b \b");
			i/=10;
			n--; 
		}
		else if(max && isdigit(ch) && (i*10)+(ch&0xf)<=max && (ch!='0' || n)) {
			i*=10;
			n++;
			i+=ch&0xf;
			outchar(ch);
			if(i*10>max) {
				attr(LIGHTGRAY);
				CRLF;
				return(i|0x8000); 
			} 
		}	 
	}
	return(0);
}

/****************************************************************************/
/* Hot keyed number input routine.											*/
/****************************************************************************/
int getnum(int max)
{
	uchar ch,n=0;
	int i=0;

	while(1) {
		ch=getkey(K_UPPER);
		if(ch>0x7f)
			continue;
		if(ch=='Q') {
			outchar('Q');
			CRLF;
			break;
		}
		else if(ch==3) {		/* ctrl-c */
			CRLF;
			break;
		}
		else if(ch==CR) {
			CRLF;
			return(i);
		}
		else if(ch==BS && n) {
			bputs("\b \b");
			i/=10;
			n--;
		}
		else if(isdigit(ch) && (i*10)+(ch&0xf)<=max && (ch!='0' || n)) {
			i*=10;
			n++;
			i+=ch&0xf;
			outchar(ch);
			if(i*10>max) {
				CRLF;
				return(i);
			}
		}
	}
	return(-1);
}

/****************************************************************************/
/* Waits for remote or local user to input a CR terminated string. 'length' */
/* is the maximum number of characters that getstr will allow the user to 	*/
/* input into the string. 'mode' specifies upper case characters are echoed */
/* or wordwrap or if in message input (^A sequences allowed). ^W backspaces */
/* a word, ^X backspaces a line, ^Gs, BSs, TABs are processed, LFs ignored. */
/* ^N non-destructive BS, ^V center line. Valid keys are echoed.			*/
/****************************************************************************/
int getstr(char *strout, size_t maxlen, long mode)
{
	size_t i,l,x,z;	/* i=current position, l=length, j=printed chars */
					/* x&z=misc */
	uchar ch,str1[256],str2[256],ins=0,atr;

	if(mode&K_LINE && user_misc&ANSI) {
		attr(LIGHTGRAY|HIGH|(BLUE<<4));  /* white on blue */
		for(i=0;i<maxlen;i++)
			outchar(SP);
		bprintf("\x1b[%dD",maxlen); 
	}
	i=l=0;	/* i=total number of chars, j=number of printable chars */
	if(wordwrap[0]) {
		strcpy(str1,wordwrap);
		wordwrap[0]=0; 
	}
	else str1[0]=0;
	if(mode&K_EDIT)
		strcat(str1,strout);
	if(strlen(str1)>maxlen)
		str1[maxlen]=0;
	atr=curatr;
	if(mode&K_AUTODEL && str1[0])
		attr(BLUE|(LIGHTGRAY<<4));
	rputs(str1);
	if(mode&K_EDIT && !(mode&(K_LINE|K_AUTODEL)) && user_misc&ANSI)
		bputs("\x1b[K");  /* destroy to eol */
	i=l=strlen(str1);

	if(mode&K_AUTODEL && str1[0]) {
		ch=getkey(mode);
		attr(atr);
		if(isprint(ch) || ch==0x7f) {
			for(i=0;i<l;i++)
				bputs("\b \b");
			i=l=0; 
		}
		else {
			for(i=0;i<l;i++)
				outchar(BS);
			rputs(str1);
			i=l; 
		}
		if(ch!=SP && ch!=TAB)
			ungetkey(ch); 
	}

	while((ch=getkey(mode|K_GETSTR))!=CR && !aborted) {
		switch(ch) {
			case 1:	/* Ctrl-A for ANSI */
				if(!(mode&K_MSG) || i>maxlen-3)
					break;
				if(ins) {
					if(l<maxlen)
						l++;
					for(x=l;x>i;x--)
						str1[x]=str1[x-1];
					rprintf("%.*s",l-i,str1+i);
					rprintf("\x1b[%dD",l-i);
					if(i==maxlen-1)
						ins=0; 
				}
				outchar(str1[i++]=1);
				break;
			case 2:	/* Ctrl-B Beginning of Line */
				if(user_misc&ANSI && i) {
					bprintf("\x1b[%dD",i);
					i=0; 
				}
				break;
			case 4:	/* Ctrl-D Delete word right */
        		if(i<l) {
					x=i;
					while(x<l && str1[x]!=SP) {
						outchar(SP);
						x++; 
					}
					while(x<l && str1[x]==SP) {
						outchar(SP);
						x++; 
					}
					bprintf("\x1b[%dD",x-i);   /* move cursor back */
					z=i;
					while(z<l-(x-i))  {             /* move chars in string */
						outchar(str1[z]=str1[z+(x-i)]);
						z++; 
					}
					while(z<l) {					/* write over extra chars */
						outchar(SP);
						z++; 
					}
					bprintf("\x1b[%dD",z-i);
					l-=x-i; 						/* l=new length */
				}
				break;
			case 5:	/* Ctrl-E End of line */
				if(user_misc&ANSI && i<l) {
					bprintf("\x1b[%dC",l-i);  /* move cursor right one */
					i=l; 
				}
				break;
			case 6:	/* Ctrl-F move cursor forewards */
				if(i<l && (user_misc&ANSI)) {
					bputs("\x1b[C");   /* move cursor right one */
					i++; 
				}
				break;
			case 7:
				if(!(mode&K_MSG))
					break;
				 if(ins) {
					if(l<maxlen)
						l++;
					for(x=l;x>i;x--)
						str1[x]=str1[x-1];
					if(i==maxlen-1)
						ins=0; 
				 }
				 if(i<maxlen) {
					str1[i++]=7;
					outchar(7); 
				 }
				 break;
			case 14:	/* Ctrl-N Next word */
				if(i<l && (user_misc&ANSI)) {
					x=i;
					while(str1[i]!=SP && i<l)
						i++;
					while(str1[i]==SP && i<l)
						i++;
					bprintf("\x1b[%dC",i-x); 
				}
				break;
			case 0x1c:	  /* Ctrl-\ Previous word */
				if(i && (user_misc&ANSI)) {
					x=i;
					while(str1[i-1]==SP && i)
						i--;
					while(str1[i-1]!=SP && i)
						i--;
					bprintf("\x1b[%dD",x-i); 
				}
				break;
			case 18:	/* Ctrl-R Redraw Line */
				redrwstr(str1,i,l,0);
				break;
			case TAB:
				if(!(i%TABSIZE)) {
            		if(ins) {
						if(l<maxlen)
							l++;
						for(x=l;x>i;x--)
							str1[x]=str1[x-1];
						if(i==maxlen-1)
							ins=0; 
					}
					str1[i++]=SP;
					outchar(SP); 
				}
				while(i<maxlen && i%TABSIZE) {
            		if(ins) {
						if(l<maxlen)
							l++;
						for(x=l;x>i;x--)
							str1[x]=str1[x-1];
						if(i==maxlen-1)
							ins=0; 
					}
					str1[i++]=SP;
					outchar(SP); 
				}
				if(ins)
					redrwstr(str1,i,l,0);
				break;
			case BS:
				if(!i)
					break;
				i--;
				l--;
				if(i!=l) {				/* Deleting char in middle of line */
					outchar(BS);
					z=i;
					while(z<l)	{		/* move the characters in the line */
						outchar(str1[z]=str1[z+1]);
						z++; 
					}
					outchar(SP);		/* write over the last char */
					bprintf("\x1b[%dD",(l-i)+1); 
				}
				else
					bputs("\b \b");
				break;
			case 12:	/* Ctrl-L 	Center line (used to be Ctrl-V) */
				str1[l]=0;
				l=bstrlen(str1);
				for(x=0;x<(maxlen-l)/2;x++)
					str2[x]=SP;
				str2[x]=0;
				strcat(str2,str1);
				strcpy(strout,str2);
				l=strlen(strout);
				if(mode&K_MSG)
					redrwstr(strout,i,l,K_MSG);
				else {
					while(i--)
						bputs("\b");
					bputs(strout);
					if(mode&K_LINE)
						attr(LIGHTGRAY); 
				}
				CRLF;
				return(l);
			case 23:	/* Ctrl-W   Delete word left */
				if(i<l) {
					x=i;							/* x=original offset */
					while(i && str1[i-1]==SP) {
						outchar(BS);
						i--; 
					}
					while(i && str1[i-1]!=SP) {
						outchar(BS);
						i--; 
					}
					z=i;                            /* i=z=new offset */
					while(z<l-(x-i))  {             /* move chars in string */
						outchar(str1[z]=str1[z+(x-i)]);
						z++; 
					}
					while(z<l) {					/* write over extra chars */
						outchar(SP);
						z++; 
					}
					bprintf("\x1b[%dD",z-i);        /* back to new x corridnant */
					l-=x-i; 						/* l=new length */
				}
				else {
            		while(i && str1[i-1]==SP) {
						i--;
						l--;
						bputs("\b \b"); 
					}
					while(i && str1[i-1]!=SP) {
						i--;
						l--;
						bputs("\b \b"); 
					} 
				}
				break;
			case 24:	/* Ctrl-X   Delete entire line */
				while(i<l) {
					outchar(SP);
					i++; 
				}
				while(l) {
					l--;
					bputs("\b \b"); 
				}
				i=0;
				break;
			case 25:	/* Ctrl-Y	Delete to end of line */
				if(user_misc&ANSI) {
					bputs("\x1b[s\x1b[K\x1b[u");
					l=i; 
				}
				break;
			case 22:	/* Ctrl-V		Toggles Insert/Overwrite */
				if(!(user_misc&ANSI))
					break;
				if(ins) {
					ins=0;
					redrwstr(str1,i,l,0); 
				}
				else if(i<l) {
					ins=1;
					bprintf("\x1b[s\x1b[%dC",80-i);         /* save pos  */
					z=curatr;								/* and got to EOL */
					attr(z|BLINK|HIGH);
					outchar('°');
					attr(z);
					bputs("\x1b[u");   /* restore pos */
				}
				break;
			case 0x1d:	/* Ctrl-]  Reverse Cursor Movement */
				if(i && (user_misc&ANSI)) {
					bputs("\x1b[D");   /* move cursor left one */
					i--; 
				}
				break;
			case 0x7f:	/* Ctrl-BkSpc (DEL) Delete current char */
				if(i==l) {	/* Backspace if end of line */
					if(i) {
						i--;
						l--;
						bputs("\b \b");
					}
					break;
				}
				l--;
				z=i;
				while(z<l)	{		/* move the characters in the line */
					outchar(str1[z]=str1[z+1]);
					z++; 
				}
				outchar(SP);		/* write over the last char */
				bprintf("\x1b[%dD",(l-i)+1);
				break;
			case ESC:
				if(!(user_misc&ANSI))
					break;
				if((ch=getkey(0x8000))!='[') {
					ungetkey(ch);
					break; 
				}
				if((ch=getkey(0x8000))=='C') {
					if(i<l) {
						bputs("\x1b[C");   /* move cursor right one */
						i++; 
					} 
				}
				else if(ch=='D') {
					if(i) {
						bputs("\x1b[D");   /* move cursor left one */
						i--; 
					} 
				}
				else {
					while(isdigit(ch) || ch==';' || isalpha(ch)) {
						if(isalpha(ch)) {
							ch=getkey(0);
							break; 
						}
						ch=getkey(0); 
					}
					ungetkey(ch); 
				}
				break;
			default:
				if(mode&K_WRAP && i==maxlen && ch>=SP && !ins) {
					str1[i]=0;
					if(ch==SP) {	/* don't wrap a space as last char */
						strcpy(strout,str1);
						if(stripattr(strout))
							redrwstr(strout,i,l,K_MSG);
						CRLF;
						return(i); 
					}
					x=i-1;
					z=1;
					wordwrap[0]=ch;
					while(str1[x]!=SP && x)
						wordwrap[z++]=str1[x--];
					if(x<(maxlen/2)) {
						wordwrap[1]=0;	/* only wrap one character */
						strcpy(strout,str1);
						if(stripattr(strout))
							redrwstr(strout,i,l,K_MSG);
						CRLF;
						return(i); 
					}
					wordwrap[z]=0;
					while(z--) {
						i--;
						bputs("\b \b"); 
					}
					strrev(wordwrap);
					str1[x]=0;
					strcpy(strout,str1);
					if(stripattr(strout))
						redrwstr(strout,i,x,mode);
					CRLF;
					return(x); 
				}
				if(i<maxlen && ch>=SP) {
					if(mode&K_UPRLWR)
						if(!i || (i && (str1[i-1]==SP || str1[i-1]=='-'
							|| str1[i-1]=='.' || str1[i-1]=='_')))
							ch=toupper(ch);
						else
							ch=tolower(ch);
					if(ins) {
						if(l<maxlen)	/* l<maxlen */
							l++;
						for(x=l;x>i;x--)
							str1[x]=str1[x-1];
						rprintf("%.*s",l-i,str1+i);
						rprintf("\x1b[%dD",l-i);
						if(i==maxlen-1) {
							bputs("  \b\b");
							ins=0; 
						} 
					}
					str1[i++]=ch;
					outchar(ch); 
				} 
			} /* switch */
		if(i>l)
			l=i;
		if(mode&K_CHAT && !l)
			return(0); 
	}
	if(i>l)
		l=i;
	str1[l]=0;
	if(!aborted) {
		strcpy(strout,str1);
		if(stripattr(strout) || ins)
			redrwstr(strout,i,l,K_MSG); 
	}
	else
		l=0;
	if(mode&K_LINE) attr(LIGHTGRAY);
	if(!(mode&K_NOCRLF)) {
		outchar(CR);
		if(!(mode&K_MSG && aborted))
			outchar(LF); 
	}
	return(l);
}

/****************************************************************************/
/* Redraws str using i as current cursor position and l as length           */
/****************************************************************************/
void redrwstr(char *strin, int i, int l, long mode)
{
	char str[256],c;

	sprintf(str,"%-*.*s",l,l,strin);
	c=i;
	while(c--)
		outchar(BS);
	if(mode&K_MSG)
		bputs(str);
	else
		rputs(str);
	if(user_misc&ANSI) {
		bputs("\x1b[K");
		if(i<l)
			bprintf("\x1b[%dD",l-i); 
	}
	else {
		while(c<79)	{ /* clear to end of line */
			outchar(SP);
			c++; 
		}
		while(c>l) { /* back space to end of string */
			outchar(BS);
			c--; 
		} 
	}
}

/****************************************************************************/
/* Strips invalid Ctrl-Ax sequences from str                                */
/* Returns number of ^A's in line                                           */
/****************************************************************************/
char stripattr(char *strin)
{
	uchar str[81];
	uchar a,c,d,e;

	e=strlen(strin);
	for(a=c=d=0;c<e;c++) {
		if(strin[c]==1) {
			a++;
			switch(toupper(strin[c+1])) {
				case '-':	/* clear 		*/
				case '_':	/* clear		*/
				case 'B':	/* blue 	fg 	*/
				case 'C':	/* cyan 	fg 	*/
				case 'G':	/* green	fg 	*/
				case 'H':	/* high 	fg 	*/
				case 'I':	/* blink 	   	*/
				case 'K':	/* black 	fg 	*/
				case 'L':	/* cls         	*/
				case 'M':	/* magenta  fg 	*/
				case 'N':	/* normal      	*/
				case 'P':	/* pause       	*/
				case 'Q':   /* pause reset  */
				case 'R':	/* red      fg 	*/
				case 'W':	/* white    fg 	*/
				case 'Y':	/* yellow   fg 	*/
				case '0':	/* black 	bg 	*/
				case '1':	/* red   	bg 	*/
				case '2':	/* green 	bg 	*/
				case '3':   /* brown	bg 	*/
				case '4':	/* blue  	bg 	*/
				case '5':   /* magenta 	bg 	*/
				case '6':	/* cyan    	bg 	*/
				case '7':	/* white   	bg 	*/
					break;
				default:
					c++;
					continue; 
			} 
		}
		str[d++]=strin[c]; 
	}
	str[d]=0;
	strcpy(strin,str);
	return(a);
}

/***************************************************************************/
/* Changes local and remote text attributes accounting for monochrome      */
/***************************************************************************/
void attr(int atr)
{

	if(!(user_misc&ANSI) || aborted)
		return;
	if(!(user_misc&COLOR)) {  /* eliminate colors if user doesn't have them */
		if(atr&LIGHTGRAY)		/* if any bits set, set all */
			atr|=LIGHTGRAY;
		if(atr&(LIGHTGRAY<<4))
			atr|=(LIGHTGRAY<<4);
		if(atr&LIGHTGRAY && atr&(LIGHTGRAY<<4))
			atr&=~LIGHTGRAY; }	/* if background is solid, forground is black */
	if(curatr==atr) /* attribute hasn't changed. don't send codes */
		return;

	if((!(atr&HIGH) && curatr&HIGH)	|| (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		bprintf("\x1b[0m");
		curatr=LIGHTGRAY; }

	if(atr==LIGHTGRAY) {				 /* no attributes */
		curatr=atr;
		return; }

	if(atr&BLINK) {						/* special attributes */
		if(!(curatr&BLINK))
			bprintf("\x1b[5m"); }
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			bprintf("\x1b[1m"); }

	if((atr&0x7)==BLACK) {				/* foreground colors */
		if((curatr&0x7)!=BLACK)
			bprintf("\x1b[30m"); }
	else if((atr&0x7)==RED) {
		if((curatr&0x7)!=RED)
			bprintf("\x1b[31m"); }
	else if((atr&0x7)==GREEN) {
		if((curatr&0x7)!=GREEN)
			bprintf("\x1b[32m"); }
	else if((atr&0x7)==BROWN) {
		if((curatr&0x7)!=BROWN)
			bprintf("\x1b[33m"); }
	else if((atr&0x7)==BLUE) {
		if((curatr&0x7)!=BLUE)
			bprintf("\x1b[34m"); }
	else if((atr&0x7)==MAGENTA) {
		if((curatr&0x7)!=MAGENTA)
			bprintf("\x1b[35m"); }
	else if((atr&0x7)==CYAN) {
		if((curatr&0x7)!=CYAN)
			bprintf("\x1b[36m"); }
	else if((atr&0x7)==LIGHTGRAY) {
		if((curatr&0x7)!=LIGHTGRAY)
			bprintf("\x1b[37m"); }

	if((atr&0x70)==(BLACK<<4)) {		/* background colors */
		if((curatr&0x70)!=(BLACK<<4))
			bprintf("\x1b[40m"); }
	else if((atr&0x70)==(RED<<4)) {
		if((curatr&0x70)!=(RED<<4))
			bprintf("\x1b[41m"); }
	else if((atr&0x70)==(GREEN<<4)) {
		if((curatr&0x70)!=(GREEN<<4))
			bprintf("\x1b[42m"); }
	else if((atr&0x70)==(BROWN<<4)) {
		if((curatr&0x70)!=(BROWN<<4))
			bprintf("\x1b[43m"); }
	else if((atr&0x70)==(BLUE<<4)) {
		if((curatr&0x70)!=(BLUE<<4))
			bprintf("\x1b[44m"); }
	else if((atr&0x70)==(MAGENTA<<4)) {
		if((curatr&0x70)!=(MAGENTA<<4))
			bprintf("\x1b[45m"); }
	else if((atr&0x70)==(CYAN<<4)) {
		if((curatr&0x70)!=(CYAN<<4))
			bprintf("\x1b[46m"); }
	else if((atr&0x70)==(LIGHTGRAY<<4)) {
		if((curatr&0x70)!=(LIGHTGRAY<<4))
			bprintf("\x1b[47m"); }

	curatr=atr;
}

/****************************************************************************/
/* Peform clear screen														*/
/****************************************************************************/
void cls(void)
{
	if(lncntr>1 && !tos) {
		lncntr=0;
		CRLF;
		bpause();
		while(lncntr && !aborted)
			bpause(); }

	if(user_misc&ANSI)
		bputs("\x1b[2J\x1b[H");	/* clear screen, home cursor */
	else 
		outchar(FF);
	tos=1;
	lncntr=0;
}

/****************************************************************************/
/* performs the correct attribute modifications for the Ctrl-A code			*/
/****************************************************************************/
void ctrl_a(char x)
{
	char atr=curatr;
	int i;

	if((uchar)x>=0x7f) {
		if(user_misc&ANSI)
			bprintf("\x1b[%uC",(uchar)x-0x7f);
		else
			for(i=0;i<(uchar)x-0x7f;i++)
				outchar(SP);
		return; }

	switch(toupper(x)) {
		case '-':								/* turn off all attributes if */
			if(atr&(HIGH|BLINK|(LIGHTGRAY<<4)))	/* high intensity, blink or */
				attr(LIGHTGRAY);				/* background bits are set */
			break;
		case '_':								/* turn off all attributes if */
			if(atr&(BLINK|(LIGHTGRAY<<4)))		/* blink or background is set */
				attr(LIGHTGRAY);
			break;
		case ',':   /* Delay 1/10 sec */
			mswait(100);
			break;
		case ';':   /* Delay 1/2 sec */
			mswait(500);
			break;
		case '.':   /* Delay 2 secs */
			mswait(2000);
			break;
		case 'P':	/* Pause */
			bpause();
			break;
		case 'Q':   /* Pause reset */
			lncntr=0;
			break;
		case 'L':	/* CLS (form feed) */
			cls();
			break;
		case '>':   /* CLREOL */
			if(user_misc&ANSI)
				bputs("\x1b[K");
#if 0
			else {
				i=j=wherey();
				while(i++<80)
					outchar(SP);
				while(j++<80)
					outchar(BS); }
#endif
			break;
		case '<':   /* Non-destructive backspace */
			outchar(BS);
			break;
		case '[':   /* Carriage return */
			outchar(CR);
			break;
		case ']':   /* Line feed */
			outchar(LF);
			break;
		case 'A':   /* Ctrl-A */
			outchar(1);
			break;
		case 'H': 	/* High intensity */
			atr|=HIGH;
			attr(atr);
			break;
		case 'I':	/* Blink */
			atr|=BLINK;
			attr(atr);
			break;
		case 'N': 	/* Normal */
			attr(LIGHTGRAY);
			break;
		case 'R':
			atr=(atr&0xf8)|RED;
			attr(atr);
			break;
		case 'S':
			nodesync();
			break;
		case 'G':
			atr=(atr&0xf8)|GREEN;
			attr(atr);
			break;
		case 'B':
			atr=(atr&0xf8)|BLUE;
			attr(atr);
			break;
		case 'W':	/* White */
			atr=(atr&0xf8)|LIGHTGRAY;
			attr(atr);
			break;
		case 'C':
			atr=(atr&0xf8)|CYAN;
			attr(atr);
			break;
		case 'M':
			atr=(atr&0xf8)|MAGENTA;
			attr(atr);
			break;
		case 'Y':
			atr=(atr&0xf8)|BROWN;
			attr(atr);
			break;
		case 'K':	/* Black */
			atr=(atr&0xf8)|BLACK;
			attr(atr);
			break;
		case '0':	/* Black Background */
			atr=(atr&0x8f)|(uchar)(BLACK<<4);
			attr(atr);
			break;
		case '1':	/* Red Background */
			atr=(atr&0x8f)|(uchar)(RED<<4);
			attr(atr);
			break;
		case '2':	/* Green Background */
			atr=(atr&0x8f)|(uchar)(GREEN<<4);
			attr(atr);
			break;
		case '3':	/* Yellow Background */
			atr=(atr&0x8f)|(uchar)(BROWN<<4);
			attr(atr);
			break;
		case '4':	/* Blue Background */
			atr=(atr&0x8f)|(uchar)(BLUE<<4);
			attr(atr);
			break;
		case '5':	/* Magenta Background */
			atr=(atr&0x8f)|(uchar)(MAGENTA<<4);
			attr(atr);
			break;
		case '6':	/* Cyan Background */
			atr=(atr&0x8f)|(uchar)(CYAN<<4);
			attr(atr);
			break;
		case '7':	/* White Background */
			atr=(atr&0x8f)|(uchar)(LIGHTGRAY<<4);
			attr(atr);
			break; }
}

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char count=0;
	int file,share;

	if(access&SH_DENYNO) share=SH_DENYNO;
	else if(access==O_RDONLY) share=SH_DENYWR;
	else share=SH_DENYRW;
	while(((file=sopen(str,O_BINARY|access,share))==-1)
		&& errno==EACCES && count++<LOOP_NOPEN)
		if(count>10)
			mswait(50);
	if(count>(LOOP_NOPEN/2) && count<=LOOP_NOPEN)
		bprintf("\r\nNOPEN COLLISION - File: %s Count: %d\r\n"
			,str,count);
	if(file==-1 && errno==EACCES)
		bputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
	return(file);
}

/****************************************************************************/
/* Reads data from XTRN.DAT in the node directory and fills the appropriate */
/* global variables.														*/
/* Initializes starttime variable with current time.						*/
/****************************************************************************/
void initdata(void)
{
	char	str[256],tmp[256];
	char*	p;
	int		i;
	FILE*	stream;

#ifdef _WINSOCKAPI_
    WSAStartup(MAKEWORD(1,1), &WSAData);
	client_socket=INVALID_SOCKET;
#endif

#ifdef __WATCOMC__
	putenv("TZ=UCT0");
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
#endif

#ifdef __SC__
    setvbuf(stdout,NULL,_IONBF,0);
	con_fp=stdout;
#else
	con_fp=stderr;
#endif

#ifdef __16BIT__

#if defined(__TURBOC__) || defined(__SC__)	/* Borland or Symantec */
	ctrlbrk(cbreakh);
#endif

	if(setmode(fileno(con_fp),O_BINARY)==-1) {	 /* eliminate LF expansion */
		printf("Can't set console output to BINARY\n");
		exit(1); }
#endif

	/* Sets node_dir to node directory environment variable defined by synchronet. */
	if(node_dir[0]==0 && (p=getenv("SBBSNODE"))!=NULL)
		sprintf(node_dir,"%.*s",sizeof(node_dir)-1,p); 

	sprintf(str,"%sXTRN.DAT",node_dir);
	if((stream=fopen(str,"rt"))==NULL) {
		printf("Can't open %s\r\n",str);
		exit(1); }
	fgets(str,81,stream);			/* username */
	sprintf(user_name,"%.25s",str);
	truncsp(user_name);
	fgets(str,81,stream);			/* system name */
	sprintf(sys_name,"%.40s",str);
	truncsp(sys_name);
	fgets(str,81,stream);			/* system operator */
	sprintf(sys_op,"%.40s",str);
	truncsp(sys_op);
	fgets(str,81,stream);			/* system guru */
	sprintf(sys_guru,"%.40s",str);
	truncsp(sys_guru);

	fgets(str,81,stream);			/* ctrl dir */
	str[50]=0;
	if(str[0]=='.')
		sprintf(ctrl_dir,"%s%s",node_dir,str);
	else
		strcpy(ctrl_dir,str);
	truncsp(ctrl_dir);
	if(_fullpath(str,ctrl_dir,50))
		strcpy(ctrl_dir,str);
	backslash(ctrl_dir);

	fgets(str,81,stream);			/* data dir */
	if(str[0]=='.')
		sprintf(data_dir,"%s%s",node_dir,str);
	else
		sprintf(data_dir,"%.40s",str);
	truncsp(data_dir);
	if(_fullpath(str,data_dir,50))
		strcpy(data_dir,str);
	backslash(data_dir);

	fgets(str,81,stream);			/* total nodes */
	sys_nodes=atoi(str);
	fgets(str,81,stream);			/* current node */
	node_num=atoi(str);
	fgets(str,81,stream);			/* time left */
	timeleft=atoi(str);
	fgets(str,81,stream);			/* ANSI? (Yes, Mono, or No) */
	user_misc=0;
	if(str[0]=='Y')
		user_misc|=(ANSI|COLOR);
	else if(str[0]=='M')
		user_misc|=ANSI;
	fgets(str,81,stream);			/* screen lines */
	user_rows=atoi(str);
	fgets(str,81,stream);			/* credits */
	user_cdt=atol(str);
	fgets(str,81,stream);			/* level */
	user_level=atoi(str);
	fgets(str,81,stream);			/* was transfer level, left for compat. */
	fgets(str,81,stream);			/* birthdate */
	truncsp(str);
	sprintf(user_birth,"%.8s",str);
	fgets(str,81,stream);			/* sex */
	user_sex=str[0];
	fgets(str,81,stream);			/* user number */
	user_number=atoi(str);
	fgets(str,81,stream);			/* user phone number */
	sprintf(user_phone,"%.12s",str);
	truncsp(user_phone);
	fgets(str,81,stream);			/* com port (0 if local or no modem) */
	com_port=atoi(str);
	fgets(str,81,stream);			/* com (UART) irq */
	com_irq=atoi(str);
	fgets(str,81,stream);			/* com (UART) base address in hex */
	truncsp(str);
	com_base=(uint)ahtoul(str);
	fgets(str,81,stream);			/* com rate */
	com_rate=(ulong)atol(str);
	fgets(str,81,stream);			/* hardware flow control (Y/N) */
	if(toupper(str[0])=='Y')
		mdm_misc|=MDM_FLOWCTRL;
	fgets(str,81,stream);			/* locked DTE rate (Y/N) */
	if(toupper(str[0])=='Y')
		mdm_misc|=MDM_STAYHIGH;
	fgets(str,81,stream);			/* modem initialization string */
	sprintf(mdm_init,"%.63s",str);
	truncsp(mdm_init);
	fgets(str,81,stream);			/* modem special init string */
	sprintf(mdm_spec,"%.63s",str);
	truncsp(mdm_spec);
	fgets(str,81,stream);			/* modem terminal mode string */
	sprintf(mdm_term,"%.63s",str);
	truncsp(mdm_term);
	fgets(str,81,stream);			/* modem dial string */
	sprintf(mdm_dial,"%.63s",str);
	truncsp(mdm_dial);
	fgets(str,81,stream);			/* modem off-hook string */
	sprintf(mdm_offh,"%.63s",str);
	truncsp(mdm_offh);
	fgets(str,81,stream);			/* modem answer string */
	sprintf(mdm_answ,"%.63s",str);
	truncsp(mdm_answ);
	fgets(str,81,stream);			/* memory address of modem status register */
	msr=(uint FAR16 *)atol(str);
	if(!fgets(str,81,stream))		/* total number of external programs */
		total_xtrns=0;
	else
		total_xtrns=atoi(str);
	if(total_xtrns && (xtrn=(char **)MALLOC(sizeof(char *)*total_xtrns))==NULL) {
		printf("Allocation error 1: %u\r\n",sizeof(char *)*total_xtrns);
		exit(1); }
	for(i=0;i<(int)total_xtrns;i++) {
		fgets(str,81,stream);
		truncsp(str);
		if((xtrn[i]=(char *)MALLOC(strlen(str)+1))==NULL) {
			printf("Allocation error 2 (%u): %u\r\n",i,strlen(str)+1);
			exit(1); }
		strcpy(xtrn[i],str); }
	fgets(str,81,stream);			/* user's main flags */
	sprintf(user_flags1,"%.26s",str);
	fgets(str,81,stream);			/* user's xfer flags */
	sprintf(user_flags2,"%.26s",str);
	fgets(str,81,stream);			/* user's exemptions */
	sprintf(user_exempt,"%.26s",str);
	fgets(str,81,stream);			/* user's restrictions */
	sprintf(user_rest,"%.26s",str);
	fgets(str,81,stream);			/* user's expiration date */
	truncsp(str);
	user_expire=ahtoul(str);
	str[0]=0;
	fgets(str,81,stream);			/* user's address */
	sprintf(user_address,"%.30s",str);
	truncsp(user_address);
	fgets(str,81,stream);			/* user's location (city, state) */
	sprintf(user_location,"%.30s",str);
	truncsp(user_location);
	fgets(str,81,stream);			/* user's zip/postal code */
	sprintf(user_zipcode,"%.10s",str);
	truncsp(user_zipcode);
	str[0]=0;
	fgets(str,81,stream);
	sprintf(user_flags3,"%.26s",str);
	fgets(str,81,stream);
	sprintf(user_flags4,"%.26s",str);
	if(fgets(str,81,stream))		/* Time-slice API type */
#ifdef __16BIT__ 
		mswtyp=ahtoul(str);
#else
		;
#endif
	str[0]=0;
	fgets(str,81,stream);
	truncsp(str);
	sprintf(user_realname,"%.25s",str);
	str[0]=0;
	fgets(str,81,stream);
	user_dce=atol(str);

	str[0]=0;
	fgets(str,81,stream);			/* exec dir */
	if(!str[0])
		sprintf(exec_dir,"%s../exec/",ctrl_dir);
	else {
		if(str[0]=='.')
			sprintf(exec_dir,"%s%s",node_dir,str);
		else
			sprintf(exec_dir,"%.50s",str); }
	truncsp(exec_dir);
	if(_fullpath(str,exec_dir,50))
		strcpy(exec_dir,str);
	backslash(exec_dir);

	str[0]=0;
	fgets(str,81,stream);			/* text dir */
	if(!str[0])
		sprintf(text_dir,"%s../text/",ctrl_dir);
	else {
		if(str[0]=='.')
			sprintf(text_dir,"%s%s",node_dir,str);
		else
			sprintf(text_dir,"%.50s",str); }
	truncsp(text_dir);
	if(_fullpath(str,text_dir,50))
		strcpy(text_dir,str);
	backslash(text_dir);

	str[0]=0;
	fgets(str,81,stream);			/* temp dir */
	if(!str[0])
		sprintf(temp_dir,"%stemp/",node_dir);
	else {
		if(str[0]!=BACKSLASH && str[1]!=':')
			sprintf(temp_dir,"%s%s",node_dir,str);
		else
			sprintf(temp_dir,"%.50s",str); }
	truncsp(temp_dir);
	if(_fullpath(str,temp_dir,50))
		strcpy(temp_dir,str);
	backslash(temp_dir);

	str[0]=0;
	fgets(str,81,stream);
	sprintf(sys_id,"%.8s",str);

	str[0]=0;
	fgets(str,81,stream);
	truncsp(str);
	if(str[0])
		node_misc=(uint)ahtoul(str);
	else
		node_misc=NM_LOWPRIO;

#ifndef __16BIT__
	str[0]=0;
	fgets(str,81,stream);
	client_socket=atoi(str);

	if(client_socket!=INVALID_SOCKET) {

#ifdef _WIN32
		output_sem=CreateEvent(
						 NULL	// pointer to security attributes
						,TRUE	// flag for manual-reset event
						,FALSE	// flag for initial state
						,NULL	// pointer to event-object name
						);
		if(output_sem==NULL) {
			printf("\r\n\7Error %d creating output_event\r\n",GetLastError());
			exit(1);
		}
#else
		sem_init(&output_sem,0,0);
#endif
		_beginthread(output_thread,0,NULL);
	}
#endif

	fclose(stream);

	sprintf(str,"%sINTRSBBS.DAT",node_dir);     /* Shrank to run! */
	if(fexist(str)) {
		if((stream=fopen(str,"rt"))==NULL) {
			printf("Can't open %s\n",str);
			exit(1); 
		}
		fgets(tmp,81,stream);					/* so get MSR address from file */
		msr=(uint FAR16 *)atol(tmp);
		fclose(stream);
		remove(str); 
	}

	starttime=time(NULL);			/* initialize start time stamp */
	wordwrap[0]=0;					/* set wordwrap to null */
	mnehigh=LIGHTGRAY|HIGH; 		/* mnemonics highlight color */
	mnelow=GREEN;					/* mnemonics normal text color */
	sec_warn=180;					/* seconds till inactivity warning */
	sec_timeout=300;				/* seconds till inactivity timeout */
	tos=lncntr=0;					/* init topofscreen and linecounter to 0 */
	lastnodemsg=0;					/* Last node to send message to */
	aborted=0;                      /* Ctrl-C hit flag */
	sysop_level=90; 				/* Minimum level to be considered sysop */
	timeleft_warn=0;				/* Running out of time warning */

	sprintf(str,"%s%s",ctrl_dir,"node.dab");
	if((nodefile=sopen(str,O_BINARY|O_RDWR,SH_DENYNO))==-1) {
		printf("\r\n\7Error opening %s\r\n",str);
		exit(1); 
	}

	sprintf(str,"%suser/name.dat",data_dir);
	if((i=nopen(str,O_RDONLY))==-1) {
		printf("\r\n\7Error opening %s\r\n",str);
		exit(1); 
	}
	memset(str,0,30);
	read(i,str,26);
	close(i);
	if(str[25]==CR) 	/* Version 1b */
		name_len=25;
	else				/* Version 1a */
		name_len=30;

#ifdef __unix__
	_termios_setup();
#endif

	if(xsdk_mode&XSDK_MODE_NOCONSOLE) {
		con_fp=NULL;
#ifdef _WIN32
		FreeConsole();
#endif
	}

	attr(LIGHTGRAY);				/* initialize color and curatr to plain */
}

/****************************************************************************/
/* Automatic RIP & WIP terminal detection function. Sets RIP and WIP bits	*/
/* in user_misc variable. Must be called AFTER initdat(), not before.		*/
/****************************************************************************/
void get_term(void)
{
	char str[128],ch;
	int i;

	bputs("\r\x1b[!_\x1b[0t_\r        \r");
	mswait(500);
	for(i=0;i<120;i++) {
		ch=inkey(0);
		if(!ch)
			break;
		mswait(1);
		str[i]=ch; }
	str[i]=0;
	if(strstr(str,"RIPSCRIP"))
		user_misc|=RIP;
	if(strstr(str,"DC-TERM"))
		user_misc|=WIP;
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(uchar *str)
{
	char c;

	str[strcspn(str,"\t")]=0;
	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
}

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

	i=strlen(str);
	if(i && str[i-1]!='\\' && str[i-1]!='/') {
		str[i]=BACKSLASH; 
		str[i+1]=0; 
	}
}


/****************************************************************************/
/* Checks the amount of time inside the external program against the amount */
/* of time the user had left online before running the external program and */
/* prints a message and exits the program if time has run out.				*/
/****************************************************************************/
void checktimeleft(void)
{
	if(!SYSOP && !strchr(user_exempt,'T') && time(NULL)-starttime>timeleft) {
		bputs("\1_\n\1r\1hTime's up.\n");
		exit(0);  }
}

/****************************************************************************/
/* Prints a file remotely and locally, interpreting ^A sequences.			*/
/* 'str' is the path of the file to print                                   */
/****************************************************************************/
void printfile(char *str)
{
	char *buf;
	int file;
	ulong length;

	if(!tos)
		CRLF;
	if((file=nopen(str,O_RDONLY))==-1) {
		bprintf("File not Found: %s\r\n",str);
		return; }
	length=filelength(file);
	if((buf=MALLOC(length+1L))==NULL) {
		close(file);
		bprintf("\7\r\nPRINTFILE: Error allocating %lu bytes of memory for %s.\r\n"
			,length+1L,str);
		return; }
	buf[read(file,buf,length)]=0;
	close(file);
	bputs(buf);
	aborted=0;
	FREE(buf);
}

/****************************************************************************/
/* Returns a char pointer to the name of the user that corresponds to		*/
/* usernumber. Takes value directly from database.							*/
/****************************************************************************/
char *username(uint usernumber)
{
	static	char name[26];
	char	str[128];
	int 	i,file;

	strcpy(name,"UNKNOWN USER");
	if(!data_dir[0])
		return(name);
	if(!usernumber) {
		bputs("\7username: called with zero usernumber\r\n");
		return(name); }
	sprintf(str,"%suser/name.dat",data_dir);
	if((file=nopen(str,O_RDONLY))==-1) {
		bprintf("\7username: couldn't open %s\r\n",str);
		return(name); }
	if(filelength(file)<(long)(usernumber-1)*((long)name_len+2L)) {
		close(file);
		return(name); }
	lseek(file,(long)(usernumber-1)*((long)name_len+2L),SEEK_SET);
	read(file,name,25);
	close(file);
	for(i=0;i<25;i++)
		if(name[i]==3)
			break;
	name[i]=0;
	if(!name[0])
		strcpy(name,"DELETED USER");
	return(name);
}

/****************************************************************************/
/* Returns the number of the user 'username' from the NAME.DAT file.        */
/* If the username is not found, the function returns 0.					*/
/****************************************************************************/
uint usernumber(char *username)
{
	char str[128];
	int i,file;
	FILE *stream;

	if(!data_dir[0])
		return(0);
	sprintf(str,"%suser/name.dat",data_dir);
	if((file=nopen(str,O_RDONLY))==-1 || (stream=fdopen(file,"rb"))==NULL) {
		if(file!=-1)
			close(file);
		bprintf("\7usernumber: couldn't open %s\r\n",str);
		return(0); }
	for(i=1;!feof(stream);i++) {
		if(!fread(str,27,1,stream))
			break;
		str[25]=0;
		truncsp(str);	/* chop of trailing EOTs and spaces */
		if(!stricmp(str,username)) {
			fclose(stream);
			return(i); } }
	fclose(stream);
	return(0);
}


/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas. Maximum value of l is 4 gigabytes.								*/
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
	char str[81];
	char i,j,k;

	sprintf(str,"%lu",l);
	i=strlen(str)-1;
	j=i/3+1+i;
	string[j--]=0;
	for(k=1;i>-1;k++) {
		string[j--]=str[i--];
		if(j>0 && !(k%3))
			string[j--]=','; }
	return(string);
}

/****************************************************************************/
/* Converts an ASCII Hex string into a ulong								*/
/****************************************************************************/
ulong ahtoul(char *str)
{
	ulong l,val=0;

	while((l=(*str++)|0x20)!=0x20)
		val=(l&0xf)+(l>>6&1)*9+val*16;
	return(val);
}

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
void getnodedat(int number, node_t *node, char lockit)
{
	int count=0;

	if(nodefile<0)
		return;
	number--;	/* make zero based */
	while(count<LOOP_NODEDAB) {
		lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
		if(lockit
			&& lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t))==-1) {
			count++;
			continue; }
		if(read(nodefile,node,sizeof(node_t))==sizeof(node_t))
			break;
		count++; }
	if(count==LOOP_NODEDAB)
		bprintf("\7Error unlocking and reading node.dab\r\n");
}

/****************************************************************************/
/* Write the data from the structure 'node' into NODE.DAB  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
void putnodedat(int number, node_t node)
{
	if(nodefile<0)
		return;
	number--;	/* make zero based */
	lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
	if(write(nodefile,&node,sizeof(node_t))!=sizeof(node_t)) {
		unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
		bprintf("\7Error writing node.dab for node %u\r\n",number+1);
		return; }
	unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
}

/****************************************************************************/
/* Checks for messages waiting for this node or interruption.				*/
/****************************************************************************/
void nodesync(void)
{
	node_t node;

	if(!ctrl_dir[0])
		return;
	getnodedat(node_num,&node,0);

	if(node.misc&NODE_MSGW)
		getsmsg(user_number);		/* getsmsg clears MSGW flag */

	if(node.misc&NODE_NMSG) 		/* getnmsg clears NMSG flag */
		getnmsg();

	if(node.misc&NODE_INTR)
		exit(0);

}

/****************************************************************************/
/* Displays the information for node number 'number' contained in 'node'    */
/****************************************************************************/
void printnodedat(int number, node_t node)
{
	char hour,mer[3],tmp[256];
	int i;

	attr(LIGHTGRAY|HIGH);
	bprintf("Node %2d: ",number);
	attr(GREEN);
	switch(node.status) {
		case NODE_WFC:
			bputs("Waiting for call");
			break;
		case NODE_OFFLINE:
			bputs("Offline");
			break;
		case NODE_NETTING:
			bputs("Networking");
			break;
		case NODE_LOGON:
			bputs("At logon prompt");
			break;
		case NODE_EVENT_WAITING:
			bputs("Waiting for all nodes to become inactive");
			break;
		case NODE_EVENT_LIMBO:
			bprintf("Waiting for node %d to finish external event",node.aux);
			break;
		case NODE_EVENT_RUNNING:
			bputs("Running external event");
			break;
		case NODE_NEWUSER:
			attr(GREEN|HIGH);
			bputs("New user");
			attr(GREEN);
			bputs(" applying for access ");
			if(!node.connection)
				bputs("Locally");
			else if(node.connection==0xffff)
				bprintf("via telnet");
			else
				bprintf("at %ubps",node.connection);
			break;
		case NODE_QUIET:
			if(!SYSOP) {
				bputs("Waiting for call");
				break; }
		case NODE_INUSE:
			attr(GREEN|HIGH);
			if(node.misc&NODE_ANON && !SYSOP)
				bputs("UNKNOWN USER");
			else
				bputs(username(node.useron));
			attr(GREEN);
			bputs(" ");
			switch(node.action) {
				case NODE_MAIN:
					bputs("at main menu");
					break;
				case NODE_RMSG:
					bputs("reading messages");
					break;
				case NODE_RMAL:
					bputs("reading mail");
					break;
				case NODE_RSML:
					bputs("reading sent mail");
					break;
				case NODE_RTXT:
					bputs("reading text files");
					break;
				case NODE_PMSG:
					bputs("posting message");
					break;
				case NODE_SMAL:
					bputs("sending mail");
					break;
				case NODE_AMSG:
					bputs("posting auto-message");
					break;
				case NODE_XTRN:
					if(!node.aux)
						bputs("at external program menu");
					else {
						bputs("running ");
						i=node.aux-1;
						if(i>=(int)total_xtrns || i<0 || !xtrn[i][0])
							bputs("external program");
						else
							bputs(xtrn[i]); }
					break;
				case NODE_DFLT:
					bputs("changing defaults");
					break;
				case NODE_XFER:
					bputs("at transfer menu");
					break;
				case NODE_RFSD:
					bprintf("retrieving from device #%d",node.aux);
					break;
				case NODE_DLNG:
					bprintf("downloading");
					break;
				case NODE_ULNG:
					bputs("uploading");
					break;
				case NODE_BXFR:
					bputs("transferring bidirectional");
					break;
				case NODE_LFIL:
					bputs("listing files");
					break;
				case NODE_LOGN:
					bputs("logging on");
					break;
				case NODE_LCHT:
					bprintf("in local chat with %s",sys_op);
					break;
				case NODE_MCHT:
					if(node.aux) {
						bprintf("in multinode chat channel %d",node.aux&0xff);
						if(node.aux&0x1f00)  /* password */
							outchar('*'); }
					else
						bputs("in multinode global chat channel");
					break;
				case NODE_PAGE:
					bprintf("paging node %u for private chat",node.aux);
					break;
				case NODE_PCHT:
					bprintf("in private chat with node %u",node.aux);
					break;
				case NODE_GCHT:
					bprintf("chatting with %s",sys_guru);
					break;
				case NODE_CHAT:
					bputs("in chat section");
					break;
				case NODE_TQWK:
					bputs("transferring QWK packet");
					break;
				case NODE_SYSP:
					bputs("performing sysop activities");
					break;
				default:
					bputs(ultoa(node.action,tmp,10));
					break;  }
			if(!node.connection)
				bputs(" locally");
			else if(node.connection==0xffff)
				bprintf(" via telnet");
			else
				bprintf(" at %ubps",node.connection);
			if(node.action==NODE_DLNG) {
				if((node.aux/60)>12) {
					hour=(node.aux/60)-12;
					strcpy(mer,"pm"); }
				else {
					if((node.aux/60)==0)    /* 12 midnite */
						hour=12;
					else hour=node.aux/60;
					strcpy(mer,"am"); }
				bprintf(" ETA %02d:%02d %s"
					,hour,node.aux%60,mer); }
			break; }
	i=NODE_LOCK;
	if(node.status==NODE_INUSE || SYSOP)
		i|=NODE_POFF|NODE_AOFF|NODE_MSGW|NODE_NMSG;
	if(node.misc&i) {
		bputs(" (");
		if(node.misc&(i&NODE_AOFF))
			outchar('A');
		if(node.misc&NODE_LOCK)
			outchar('L');
		if(node.misc&(i&(NODE_MSGW|NODE_NMSG)))
			outchar('M');
		if(node.misc&(i&NODE_POFF))
			outchar('P');
		outchar(')'); }
	if(SYSOP && ((node.misc
		&(NODE_ANON|NODE_UDAT|NODE_INTR|NODE_RRUN|NODE_EVENT|NODE_DOWN))
		|| node.status==NODE_QUIET)) {
		bputs(" [");
		if(node.misc&NODE_ANON)
			outchar('A');
		if(node.misc&NODE_INTR)
			outchar('I');
		if(node.misc&NODE_RRUN)
			outchar('R');
		if(node.misc&NODE_UDAT)
			outchar('U');
		if(node.status==NODE_QUIET)
			outchar('Q');
		if(node.misc&NODE_EVENT)
			outchar('E');
		if(node.misc&NODE_DOWN)
			outchar('D');
		outchar(']'); }
	if(node.errors && SYSOP) {
		attr(RED|HIGH|BLINK);
		bprintf(" %d error%c",node.errors, node.errors>1 ? 's' : '\0' ); }
	CRLF;
}

/****************************************************************************/
/* Prints short messages waiting for 'usernumber', if any...				*/
/* then deletes them.														*/
/****************************************************************************/
void getsmsg(int usernumber)
{
	char str[256], *buf;
	int file;
	long length;
	node_t node;

	if(!data_dir[0])
		return;
	sprintf(str,"%smsgs/%4.4u.msg",data_dir,usernumber);
	if(flength(str)<1L) {
		return; }
	if((file=nopen(str,O_RDWR))==-1) {
		bprintf("\7Error opening %s for read/write access\r\n",str);
		return; }
	length=filelength(file);
	if((buf=MALLOC(length+1))==NULL) {
		close(file);
		bprintf("\7Error allocating %u bytes of memory for %s\r\n",length+1,str);
		return; }
	if(read(file,buf,length)!=length) {
		close(file);
		FREE(buf);
		bprintf("\7Error reading %u bytes from %s\r\n",length,str);
		return; }
	chsize(file,0L);
	close(file);
	buf[length]=0;
	getnodedat(node_num,&node,0);
	if(node.action==NODE_MAIN || node.action==NODE_XFER) {
		CRLF; }
	if(node.misc&NODE_MSGW) {
		getnodedat(node_num,&node,1);
		node.misc&=~NODE_MSGW;
		putnodedat(node_num,node); }
	bputs(buf);
	FREE(buf);
}

/****************************************************************************/
/* Creates a short message for 'usernumber' than contains 'strin'			*/
/****************************************************************************/
void putsmsg(int usernumber, char *strin)
{
	char str[256];
	int file,i;
    node_t node;

	if(!data_dir[0])
		return;
	sprintf(str,"%smsgs/%4.4u.msg",data_dir,usernumber);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		bprintf("\7Error opening/creating %s for creat/append access\r\n",str);
		return; }
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		bprintf("\7Error writing %u bytes to %s\r\n",i,str);
		return; }
	close(file);
	for(i=1;i<=sys_nodes;i++) {		/* flag node if user on that msg waiting */
		getnodedat(i,&node,0);
		if(node.useron==(ushort)usernumber
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& !(node.misc&NODE_MSGW)) {
			getnodedat(i,&node,1);
			node.misc|=NODE_MSGW;
			putnodedat(i,node); } }
}

/****************************************************************************/
/* Prints short messages waiting for this node, if any...					*/
/****************************************************************************/
void getnmsg(void)
{
	char str[256], *buf;
	int file;
	long length;
	node_t thisnode;

	if(!data_dir[0])
		return;
	getnodedat(node_num,&thisnode,1);
	thisnode.misc&=~NODE_NMSG;			/* clear the NMSG flag */
	putnodedat(node_num,thisnode);

	sprintf(str,"%smsgs/n%3.3u.msg",data_dir,node_num);
	if(flength(str)<1L) {
		return; }
	if((file=nopen(str,O_RDWR))==-1) {
		printf("Couldn't open %s for read/write\r\n",str);
		return; }
	length=filelength(file);
	if((buf=MALLOC(length+1))==NULL) {
		close(file);
		printf("Couldn't allocate %lu bytes for %s\r\n",length+1,str);
		return; }
	if(read(file,buf,length)!=length) {
		close(file);
		FREE(buf);
		printf("Couldn't read %lu bytes from %s\r\n",length,str);
		return; }
	chsize(file,0L);
	close(file);
	buf[length]=0;

	bputs(buf);
	FREE(buf);
}

/****************************************************************************/
/* Creates a short message for node 'num' than contains 'strin'             */
/****************************************************************************/
void putnmsg(int num, char *strin)
{
	char str[256];
	int file,i;
    node_t node;

	if(!data_dir[0])
		return;
	sprintf(str,"%smsgs/n%3.3u.msg",data_dir,num);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		printf("Couldn't open %s for append\r\n",str);
		return; }
	i=strlen(strin);
	if(write(file,strin,i)!=i) {
		close(file);
		printf("Error writing %u bytes to %s\r\n",i,str);
		return; }
	close(file);
	getnodedat(num,&node,0);
	if((node.status==NODE_INUSE || node.status==NODE_QUIET)
		&& !(node.misc&NODE_NMSG)) {
		getnodedat(num,&node,1);
		node.misc|=NODE_NMSG;
		putnodedat(num,node); }
}

/****************************************************************************/
/* This function lists users that are online.								*/
/* If listself is true, it will list the current node.						*/
/* Returns number of active nodes (not including current node). 			*/
/****************************************************************************/
int whos_online(char listself)
{
	int i,j;
	node_t node;

	if(!ctrl_dir[0])
		return(0);
	CRLF;
	for(j=0,i=1;i<=sys_nodes;i++) {
		getnodedat(i,&node,0);
		if(i==node_num) {
			if(listself)
				printnodedat(i,node);
			continue; }
		if(node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET)) {
			printnodedat(i,node);
			if(!lastnodemsg)
				lastnodemsg=i;
			j++; } }
	if(!j)
		bputs("\1nNo other active nodes.\r\n");
	return(j);
}

/****************************************************************************/
/* Sending single line messages between nodes                               */
/****************************************************************************/
void nodemsg(void)
{
	char	line[256],buf[512];
	int 	i;
	node_t	thisnode;
	node_t 	node;

	if(!ctrl_dir[0])
		return;
	if(strchr(user_rest,'C')) {
		bputs("You cannot send messages.\r\n");
		return; }
	getnodedat(node_num,&thisnode,0);
	wordwrap[0]=0;
	if(lastnodemsg) {
		getnodedat(lastnodemsg,&node,0);
		if(node.status!=NODE_INUSE)
			lastnodemsg=0; }
	if(!whos_online(0))
		return;
	bprintf("\r\n\1n\1gNumber of node to send message to, \1w\1hA\1n\1gll, "
		"or \1w\1hQ\1n\1guit [%u]: \1w\1h",lastnodemsg);
	i=getkeys("QA",sys_nodes);
	if(i==-1)
		return;
	if(i&0x8000 || !i) {
		if(!i)
			i=lastnodemsg;
		else {
			i^=0x8000;
			lastnodemsg=i; }
		if(!i || i>sys_nodes)
			return;
		getnodedat(i,&node,0);
		if(node.status!=NODE_INUSE && !SYSOP)
			bprintf("\r\n\1_\1w\1hNode %d is not in use.\r\n",i);
		else if(i==node_num)
			bputs("\r\nThere's no need to send a message to yourself.\r\n");
		else if(node.misc&NODE_POFF && !SYSOP)
			bprintf("\r\n\1r\1h\1iDon't bug %s.\1n\r\n"
				,node.misc&NODE_ANON ? "UNKNOWN USER"
				: username(node.useron));
		else {
			bputs("\1_\1y\1hMessage: ");
			if(!getstr(line,70,K_LINE))
				return;
			sprintf(buf
				,"\7\1_\1w\1hNode %2d: \1g%s\1n\1g sent you a message:\r\n"
				"\1w\1h\0014%s\1n\r\n"
				,node_num
				,thisnode.misc&NODE_ANON ? "UNKNOWN USER" : user_name,line);
			putnmsg(i,buf); } }
	else if(i=='A') {
		bputs("\1_\1y\1hMessage: ");
		if(!getstr(line,70,K_LINE))
			return;
		sprintf(buf
			,"\7\1_\1w\1hNode %2d: \1g%s\1n\1g sent all nodes a message:\r\n"
				"\1w\1h\0014%s\1n\r\n"
			,node_num
			,thisnode.misc&NODE_ANON ? "UNKNOWN USER" : user_name,line);
		for(i=1;i<=sys_nodes;i++) {
			if(i==node_num)
				continue;
			getnodedat(i,&node,0);
			if((node.status==NODE_INUSE || (SYSOP && node.status==NODE_QUIET))
				&& (SYSOP || !(node.misc&NODE_POFF)))
				putnmsg(i,buf); } }

}

/****************************************************************************/
/* Puts a character into the input buffer									*/
/****************************************************************************/
void ungetkey(char ch)
{

	keybuf[keybuftop++]=ch;
	if(keybuftop==KEY_BUFSIZE)
		keybuftop=0;
}

