/* answer.cpp */

/* Synchronet answer "caller" function */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "telnet.h"

bool sbbs_t::answer()
{
	char	str[MAX_PATH+1],str2[MAX_PATH+1],c;
	char 	tmp[MAX_PATH+1];
	char 	tmp2[MAX_PATH+1];
	char	buf[64];
	int		i,l,in;
	struct tm tm;
	struct in_addr addr;

	useron.number=0;
	answertime=logontime=starttime=now=time(NULL);
	/* Caller ID is IP address */
	addr.s_addr=client_addr;
	SAFECOPY(cid,inet_ntoa(addr)); 

	memset(&tm,0,sizeof(tm));
    localtime_r(&now,&tm); 

	sprintf(str,"%s  %s %s %02d %u            Node %3u"
		,hhmmtostr(&cfg,&tm,str2)
		,wday[tm.tm_wday]
        ,mon[tm.tm_mon],tm.tm_mday,tm.tm_year+1900,cfg.node_num);
	logline("@ ",str);

	sprintf(str,"%s  %s [%s]", connection, client_name, cid);
	logline("@+:",str);

	if(client_ident[0]) {
		sprintf(str,"Identity: %s",client_ident);
		logline("@*",str);
	}

	online=ON_REMOTE;

	rlogin_name[0]=0;
	if(sys_status&SS_RLOGIN) {
		if(incom(1000)==0) {
			for(i=0;i<LEN_ALIAS;i++) {
				in=incom(1000);
				if(in==0 || in==NOINP)
					break;
				str[i]=in;
			}
			str[i]=0;
			for(i=0;i<LEN_ALIAS;i++) {
				in=incom(1000);
				if(in==0 || in==NOINP)
					break;
				str2[i]=in;
			}
			str2[i]=0;
			for(i=0;i<sizeof(terminal)-1;i++) {
				in=incom(1000);
				if(in==0 || in==NOINP)
					break;
				terminal[i]=in;
			}
			terminal[i]=0;
			truncstr(terminal,"/");
			lprintf("Node %d RLogin: '%s' / '%s / %s'",cfg.node_num,str,str2,terminal);
			SAFECOPY(rlogin_name
				,startup->options&BBS_OPT_USE_2ND_RLOGIN ? str2 : str);
			useron.number=userdatdupe(0, U_ALIAS, LEN_ALIAS, rlogin_name, 0);
			if(useron.number)
				getuserdat(&cfg,&useron);
		}
	}
	if(rlogin_name[0]==0)
		sys_status&=~SS_RLOGIN;

	if(!(sys_status&SS_RLOGIN)) {
		/* Disable Telnet Terminal Echo */
		send_telnet_cmd(TELNET_WILL,TELNET_ECHO);
		/* Will suppress Go Ahead */
		send_telnet_cmd(TELNET_WILL,TELNET_SUP_GA);
		/* Retrieve terminal type from telnet client --RS */
		send_telnet_cmd(TELNET_DO,TELNET_TERM_TYPE);
		sprintf(buf,"%c%c%c%c%c%c",
			TELNET_IAC,TELNET_SB,TELNET_TERM_TYPE,TELNET_TERM_SEND,
			TELNET_IAC,TELNET_SE);
		putcom(buf,6);
	}

	/* Detect terminal type */
    mswait(200);
	rioctl(IOFI);		/* flush input buffer */
	putcom( "\r\n"		/* locate cursor at column 1 */
			"\x1b[s"	/* save cursor position (necessary for HyperTerm auto-ANSI) */
    		"\x1b[99B_"	/* locate cursor as far down as possible */
			"\x1b[6n"	/* Get cursor position */
			"\x1b[u"	/* restore cursor position */
			"\x1b[!_"	/* RIP? */
			"\x1b[0t_"	/* WIP? */
			"\2\2?HTML?"/* HTML? */
			"\x1b[0m_"	/* "Normal" colors */
			"\x1b[2J"	/* clear screen */
			"\x1b[H"	/* home cursor */
			"\xC"		/* clear screen (in case not ANSI) */
			"\r"		/* Move cursor left (in case previous char printed) */
			);
	i=l=0;
	tos=1;
	strcpy(str,VERSION_NOTICE);
	strcat(str,"  ");
	strcat(str,COPYRIGHT_NOTICE);
	strip_ctrl(str);
	center(str);

	while(i++<50 && l<(int)sizeof(str)-1) { 	/* wait up to 5 seconds for response */
		c=incom(100)&0x7f;
		if(c==0)
			continue;
		i=0;
		if(l==0 && c!=ESC)	// response must begin with escape char
			continue;
		str[l++]=c;
		if(c=='R') {   /* break immediately if ANSI response */
			mswait(500);
			break; 
		}
	}

	while((c=(incom(100)&0x7f))!=0 && l<(int)sizeof(str)-1)
		str[l++]=c;
	str[l]=0;

    if(l) {
        if(str[0]==ESC && str[1]=='[') {
			if(terminal[0]==0)
				SAFECOPY(terminal,"ANSI");
			autoterm|=(ANSI|COLOR);
            rows=atoi(str+2);
			lprintf("Node %d ANSI cursor position report: %u rows"
				,cfg.node_num, rows);
			if(rows<10 || rows>99) rows=24; 
		}
		truncsp(str);
		if(strstr(str,"RIPSCRIP")) {
			if(terminal[0]==0)
				SAFECOPY(terminal,"RIP");
			logline("@R",strstr(str,"RIPSCRIP"));
			autoterm|=(RIP|COLOR|ANSI); }
		else if(strstr(str,"DC-TERM")
			&& toupper(*(strstr(str,"DC-TERM")+12))=='W') {
			if(terminal[0]==0)
				SAFECOPY(terminal,"WIP");
			logline("@W",strstr(str,"DC-TERM"));
			autoterm|=(WIP|COLOR|ANSI); }
		else if(strstr(str,"!HTML!"))  {
			if(terminal[0]==0)
				SAFECOPY(terminal,"HTML");
			logline("@H",strstr(str,"!HTML!"));
			autoterm|=HTML;
		} 
	}
	else if(terminal[0]==0)
		SAFECOPY(terminal,"DUMB");

	rioctl(IOFI); /* flush left-over or late response chars */

	if(!autoterm && str[0]) {
		lputs("Terminal Auto-detect failed, Response: ");
        str2[0]=0;
		for(i=0;str[i];i++) {
        	if(str[i]>=' ' && str[i]<='~')
            	sprintf(tmp,"%c", str[i]);
            else
				sprintf(tmp,"<%02X>", (uchar)str[i]);
            strcat(str2,tmp);
        }
        lputs(str2);
	}

	/* AutoLogon via IP or Caller ID here */
	if(!useron.number && !(sys_status&SS_RLOGIN)
		&& startup->options&BBS_OPT_AUTO_LOGON && cid[0]) {
		useron.number=userdatdupe(0, U_NOTE, LEN_NOTE, cid, 0);
		if(useron.number) {
			getuserdat(&cfg, &useron);
			if(!(useron.misc&AUTOLOGON) || !(useron.exempt&FLAG('V')))
				useron.number=0;
		}
	}

	if(!online) 
		return(false); 

	useron.misc&=~(ANSI|COLOR|RIP|WIP);
	useron.misc|=autoterm;
	SAFECOPY(useron.comp,client_name);

	if(!useron.number && sys_status&SS_RLOGIN) {
		CRLF;
		newuser();
	}

	if(!useron.number) {	/* manual/regular logon */

		/* Display ANSWER screen */
		sprintf(str,"%sanswer",cfg.text_dir);
		sprintf(tmp,"%s.%s",str,autoterm&WIP ? "wip":"rip");
		sprintf(tmp2,"%s.html",str);
		sprintf(str2,"%s.ans",str);
		if(autoterm&(RIP|WIP) && fexist(tmp))
			strcat(str,autoterm&WIP ? ".wip":".rip");
		else if(autoterm&HTML && fexist(tmp2))
			strcat(str,".html");
		else if(autoterm&ANSI && fexist(str2))
			strcat(str,".ans");
		else
			strcat(str,".asc");
		rioctl(IOSM|PAUSE);
		sys_status|=SS_PAUSEON;
		printfile(str,P_NOABORT);
		sys_status&=~SS_PAUSEON;
		exec_bin(cfg.login_mod,&main_csi);
	} else	/* auto logon here */
		if(logon()==false)
			return(false);


	if(!useron.number)
		hangup();
	if(!online) 
		return(false); 

	/* Save the IP to the user's note */
	if(cid[0]) {
		SAFECOPY(useron.note,cid);
		putuserrec(&cfg,useron.number,U_NOTE,LEN_NOTE,useron.note);
	}

	/* Save host name to the user's computer description */
	if(client_name[0]) {
		SAFECOPY(useron.comp,client_name);
		putuserrec(&cfg,useron.number,U_COMP,LEN_COMP,useron.comp);
	}

	if(!(sys_status&SS_USERON)) {
		errormsg(WHERE,ERR_CHK,"User not logged on",0);
		hangup();
		return(false); }

	return(true);
}
