/* writemsg.cpp */

/* Synchronet message creation routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define MAX_LINE_LEN 82L

const char *qstr=" > %.76s\r\n";
void quotestr(char *str);

#ifdef __unix__
	#define DEBUG_ME lprintf("%s %d",__FILE__,__LINE__);
#else
	#define DEBUG_ME
#endif

/****************************************************************************/
/* Creates a message (post or mail) using standard line editor. 'fname' is  */
/* is name of file to create, 'top' is a buffer to place at beginning of    */
/* message and 'title' is the title (70chars max) for the message.          */
/* 'dest' contains a text description of where the message is going.        */
/****************************************************************************/
bool sbbs_t::writemsg(char *fname, char *top, char *title, long mode, int subnum
	,char *dest)
{
	char	str[256],quote[128],c,HUGE16 *buf,*p,*tp
				,useron_level;
	char	msgtmp[MAX_PATH+1];
	char 	tmp[512];
	int		i,j,file,linesquoted=0;
	long	length,qlen=0,qtime=0,ex_mode=0;
	ulong	l;
	FILE *	stream;

	useron_level=useron.level;

	if((buf=(char HUGE16*)LMALLOC(cfg.level_linespermsg[useron_level]*MAX_LINE_LEN))
		==NULL) {
		errormsg(WHERE,ERR_ALLOC,fname
			,cfg.level_linespermsg[useron_level]*MAX_LINE_LEN);
		return(false); }

	if(mode&WM_NETMAIL ||
		(!(mode&(WM_EMAIL|WM_NETMAIL)) && cfg.sub[subnum]->misc&SUB_PNET))
		mode|=WM_NOTOP;

	if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&QUICKBBS)
		sprintf(msgtmp,"%sMSGTMP",cfg.node_dir);	/* QuickBBS editors are dumb */
	else
		sprintf(msgtmp,"%sINPUT.MSG",cfg.temp_dir);

	if(mode&WM_QUOTE && !(useron.rest&FLAG('J'))
		&& ((mode&(WM_EMAIL|WM_NETMAIL) && cfg.sys_misc&SM_QUOTE_EM)
		|| (!(mode&(WM_EMAIL|WM_NETMAIL)) && (uint)subnum!=INVALID_SUB
			&& cfg.sub[subnum]->misc&SUB_QUOTE))) {

		/* Quote entire message to MSGTMP or INPUT.MSG */

		if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&QUOTEALL) {
			sprintf(str,"%sQUOTES.TXT",cfg.node_dir);
			if((stream=fnopen(NULL,str,O_RDONLY))==NULL) {
				errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
				LFREE(buf);
				return(false); 
			}

			if((file=nopen(msgtmp,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
				errormsg(WHERE,ERR_OPEN,msgtmp,O_WRONLY|O_CREAT|O_TRUNC);
				LFREE(buf);
				fclose(stream);
				return(false); 
			}

			while(!feof(stream) && !ferror(stream)) {
				if(!fgets(str,255,stream))
					break;
				quotestr(str);
				sprintf(tmp,qstr,str);
				write(file,tmp,strlen(tmp));
				linesquoted++; 
			}
			fclose(stream);
			close(file); 
		}

		/* Quote nothing to MSGTMP or INPUT.MSG automatically */

		else if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&QUOTENONE)
			;

		else if(yesno(text[QuoteMessageQ])) {
			sprintf(str,"%sQUOTES.TXT",cfg.node_dir);
			if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
				errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
				LFREE(buf);
				return(false); }

			if((file=nopen(msgtmp,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
				errormsg(WHERE,ERR_OPEN,msgtmp,O_WRONLY|O_CREAT|O_TRUNC);
				LFREE(buf);
				fclose(stream);
				return(false); }

			l=ftell(stream);			/* l now points to start of message */

			while(online) {
				sprintf(str,text[QuoteLinesPrompt],linesquoted ? "Done":"All");
				mnemonics(str);
				i=getstr(quote,10,K_UPPER);
				if(sys_status&SS_ABORT) {
					fclose(stream);
					close(file);
					LFREE(buf);
					return(false); }
				if(!i && linesquoted)
					break;
				if(!i || quote[0]=='A') {                   /* Quote all */
					fseek(stream,l,SEEK_SET);
					while(!feof(stream) && !ferror(stream)) {
						if(!fgets(str,255,stream))
							break;
						quotestr(str);
						sprintf(tmp,qstr,str);
						write(file,tmp,strlen(tmp));
						linesquoted++; }
					break; }
				if(quote[0]=='L') {
					fseek(stream,l,SEEK_SET);
					i=1;
					CRLF;
					attr(LIGHTGRAY);
					while(!feof(stream) && !ferror(stream) && !msgabort()) {
						if(!fgets(str,255,stream))
							break;
						quotestr(str);
						bprintf("%3d: %.74s\r\n",i,str);
						i++; }
					continue; }

				if(!isdigit(quote[0]))
					break;
				p=quote;
				while(p) {
					if(*p==',' || *p==SP)
						p++;
					i=atoi(p);
					if(!i)
						break;
					fseek(stream,l,SEEK_SET);
					j=1;
					while(!feof(stream) && !ferror(stream) && j<i) {
						if(!fgets(tmp,255,stream))
							break;
						j++; }		/* skip beginning */
					tp=strchr(p,'-');   /* tp for temp pointer */
					if(tp) {		 /* range */
						i=atoi(tp+1);
						while(!feof(stream) && !ferror(stream) && j<=i) {
							if(!fgets(str,255,stream))
								break;
							quotestr(str);
							sprintf(tmp,qstr,str);
							write(file,tmp,strlen(tmp));
							linesquoted++;
							j++; } }
					else {			/* one line */
						if(fgets(str,255,stream)) {
							quotestr(str);
							sprintf(tmp,qstr,str);
							write(file,tmp,strlen(tmp));
							linesquoted++; } }
					p=strchr(p,',');
					// if(!p) p=strchr(p,SP);  02/05/96 huh?
					} }

			fclose(stream);
			close(file); } }
	else {
		sprintf(str,"%sQUOTES.TXT",cfg.node_dir);
		remove(str); }

	if(!online || sys_status&SS_ABORT) {
		LFREE(buf);
		return(false); }

	if(!(mode&WM_EXTDESC)) {
		if(mode&WM_FILE) {
			c=12;
			CRLF;
			bputs(text[Filename]); }
		else {
			c=LEN_TITLE;
			bputs(text[SubjectPrompt]); }
		if(!(mode&(WM_EMAIL|WM_NETMAIL)) && !(mode&WM_FILE)
			&& cfg.sub[subnum]->misc&(SUB_QNET /* |SUB_PNET */ ))
			c=25;
		if(mode&WM_QWKNET)
			c=25;
		DEBUG_ME
		if(!getstr(title,c,mode&WM_FILE ? K_LINE|K_UPPER : K_LINE|K_EDIT|K_AUTODEL)
			&& useron_level && useron.logons) {
			LFREE(buf);
			return(false); }
		DEBUG_ME
		if(!(mode&(WM_EMAIL|WM_NETMAIL)) && cfg.sub[subnum]->misc&SUB_QNET
			&& !SYSOP
			&& (!stricmp(title,"DROP") || !stricmp(title,"ADD")
			|| !strnicmp(dest,"SBBS",4))) {
			LFREE(buf);   /* Users can't post DROP or ADD in QWK netted subs */
			return(false); } }	/* or messages to "SBBS" */

	if(!online || sys_status&SS_ABORT) {
		LFREE(buf);
		return(false); }

	/* Create WWIV compatible EDITOR.INF file */

	if(useron.xedit) {
		DEBUG_ME
		editor_inf(useron.xedit,dest,title,mode,subnum);
		DEBUG_ME
		if(cfg.xedit[useron.xedit-1]->type) {
			gettimeleft();
			xtrndat(useron.alias,cfg.node_dir,cfg.xedit[useron.xedit-1]->type
 			   ,timeleft,cfg.xedit[useron.xedit-1]->misc); 
		}
		DEBUG_ME
	}

	if(console&CON_RAW_IN) {
		bprintf(text[EnterMsgNowRaw]
			,(ulong)cfg.level_linespermsg[useron_level]*MAX_LINE_LEN);
		if(top[0] && !(mode&WM_NOTOP)) {
			strcpy((char *)buf,top);
			strcat((char *)buf,crlf);
			l=strlen((char *)buf); }
		else
			l=0;
		while(l<(ulong)(cfg.level_linespermsg[useron_level]*MAX_LINE_LEN)) {
			c=getkey(0);
			if(sys_status&SS_ABORT) {  /* Ctrl-C */
				LFREE(buf);
				return(false); }
			if((c==ESC || c==CTRL_A) && useron.rest&FLAG('A')) /* ANSI restriction */
				continue;
			if(c==BEL && useron.rest&FLAG('B'))   /* Beep restriction */
				continue;
			if(!(console&CON_RAW_IN))	/* Ctrl-Z was hit */
				break;
			outchar(c);
			buf[l++]=c; }
		buf[l]=0;
		if(l==(ulong)cfg.level_linespermsg[useron_level]*MAX_LINE_LEN)
			bputs(text[OutOfBytes]); }


	else if((online==ON_LOCAL && cfg.node_misc&NM_LCL_EDIT && cfg.node_editor[0])
		|| useron.xedit) {

		DEBUG_ME
		if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&IO_INTS) {
			if(online==ON_REMOTE)
				ex_mode|=(EX_OUTR|EX_INR);
			if(cfg.xedit[useron.xedit-1]->misc&WWIVCOLOR)
				ex_mode|=EX_WWIV; }
		if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&XTRN_NATIVE)
			ex_mode|=EX_NATIVE;
		if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&XTRN_SH)
			ex_mode|=EX_SH;

		if(!linesquoted && fexist(msgtmp))
			remove(msgtmp);
		if(linesquoted) {
			qlen=flength(msgtmp);
			qtime=fdate(msgtmp); }
		DEBUG_ME
		if(online==ON_LOCAL) {
			if(cfg.node_misc&NM_LCL_EDIT && cfg.node_editor[0])
				external(cmdstr(cfg.node_editor,msgtmp,nulstr,NULL)
					,0,cfg.node_dir); 
			else
				external(cmdstr(cfg.xedit[useron.xedit-1]->lcmd,msgtmp,nulstr,NULL)
					,ex_mode,cfg.node_dir); }

		else {
			rioctl(IOCM|PAUSE|ABORT);
			external(cmdstr(cfg.xedit[useron.xedit-1]->rcmd,msgtmp,nulstr,NULL),ex_mode,cfg.node_dir);
			rioctl(IOSM|PAUSE|ABORT); }
		checkline();
		if(!fexist(msgtmp) || !online
			|| (linesquoted && qlen==flength(msgtmp) && qtime==fdate(msgtmp))) {
			LFREE(buf);
			return(false); }
		buf[0]=0;
		if(!(mode&WM_NOTOP))
			strcpy((char *)buf,top);
		if((file=nopen(msgtmp,O_RDONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,msgtmp,O_RDONLY);
			LFREE(buf);
			return(false); }
		length=filelength(file);
		l=strlen((char *)buf);	  /* reserve space for top and terminating null */
		/* truncate if too big */
		if(length>(long)((cfg.level_linespermsg[useron_level]*MAX_LINE_LEN)-(l+1))) {
			length=(cfg.level_linespermsg[useron_level]*MAX_LINE_LEN)-(l+1);
			bputs(text[OutOfBytes]); }
		lread(file,buf+l,length);
		close(file);
		// remove(msgtmp); 	   /* no need to save the temp input file */
		buf[l+length]=0; }
	else {
		DEBUG_ME
		buf[0]=0;
		if(linesquoted) {
			DEBUG_ME
			if((file=nopen(msgtmp,O_RDONLY))!=-1) {
				length=filelength(file);
				l=length>cfg.level_linespermsg[useron_level]*MAX_LINE_LEN
					? cfg.level_linespermsg[useron_level]*MAX_LINE_LEN : length;
				lread(file,buf,l);
				buf[l]=0;
				close(file);
				// remove(msgtmp);
			} 
			DEBUG_ME
		}
		if(!(msgeditor((char *)buf,mode&WM_NOTOP ? nulstr : top,title))) {
			LFREE(buf);
			return(false); 
		} 
	}

	now=time(NULL);
	bputs(text[Saving]);
	if((stream=fnopen(&file,fname,O_WRONLY|O_CREAT|O_TRUNC))==NULL) {
		errormsg(WHERE,ERR_OPEN,fname,O_WRONLY|O_CREAT|O_TRUNC);
		LFREE(buf);
		return(false); }
	for(l=i=0;buf[l] && i<cfg.level_linespermsg[useron_level];l++) {
		if((uchar)buf[l]==141 && useron.xedit
    		&& cfg.xedit[useron.xedit-1]->misc&QUICKBBS) {
			fwrite(crlf,2,1,stream);
			i++;
			continue; 
		}
		/* Expand LF to CRLF? */
		if(buf[l]==LF && (!l || buf[l-1]!=CR) && useron.xedit
			&& cfg.xedit[useron.xedit-1]->misc&EXPANDLF) {
			fwrite(crlf,2,1,stream);
			i++;
			continue; 
		}
		/* Strip FidoNet Kludge Lines? */
		if(buf[l]==1 && useron.xedit
			&& cfg.xedit[useron.xedit-1]->misc&STRIPKLUDGE) {
			while(buf[l] && buf[l]!=LF) 
				l++;
			if(buf[l]==0)
				break;
			continue;
		}
		if(!(mode&(WM_EMAIL|WM_NETMAIL))
			&& (!l || buf[l-1]==LF)
			&& buf[l]=='-' && buf[l+1]=='-' && buf[l+2]=='-'
			&& (buf[l+3]==SP || buf[l+3]==TAB || buf[l+3]==CR))
			buf[l+1]='+';
		if(buf[l]==LF)
			i++;
		fputc(buf[l],stream); }

	if(buf[l])
		bputs(text[NoMoreLines]);

	/* Signature file */
	if(subnum==INVALID_SUB || !(cfg.sub[subnum]->misc&SUB_NOUSERSIG)) {
		sprintf(str,"%suser/%04u.sig",cfg.data_dir,useron.number);
		FILE* sig;
		if(fexist(str) && (sig=fopen(str,"rb"))!=NULL) {
			while(!feof(sig)) {
				if(!fgets(str,sizeof(str),sig))
					break;
				fputs(str,stream);
				l+=strlen(str);	/* byte counter */
				i++;			/* line counter */
			}
			fclose(sig);
		}
	}

	fclose(stream);
	LFREE((char *)buf);
	bprintf(text[SavedNBytes],l,i);
	return(true);
}

/****************************************************************************/
/* Modify 'str' to for quoted format. Remove ^A codes, etc.                 */
/****************************************************************************/
void quotestr(char *str)
{
	int j;

	j=strlen(str);
	while(j && (str[j-1]==SP || str[j-1]==LF || str[j-1]==CR)) j--;
	str[j]=0;
	remove_ctrl_a(str,NULL);
}

void sbbs_t::editor_inf(int xeditnum,char *dest, char *title, long mode
	,uint subnum)
{
	char str[512];
	int file;

	xeditnum--;

	if(cfg.xedit[xeditnum]->misc&QUICKBBS) {
		sprintf(str,"%sMSGINF",cfg.node_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			return; }
		sprintf(str,"%s\r\n%s\r\n%s\r\n%u\r\n%s\r\n%s\r\n"
			,(subnum!=INVALID_SUB && cfg.sub[subnum]->misc&SUB_NAME) ? useron.name
				: useron.alias
				,dest,title,1
				,mode&WM_NETMAIL ? "NetMail"
				:mode&WM_EMAIL ? "Electronic Mail"
				:subnum==INVALID_SUB ? nulstr
				:cfg.sub[subnum]->sname
			,mode&WM_PRIVATE ? "YES":"NO");
		write(file,str,strlen(str));
		close(file); }
	else {
		sprintf(str,"%sEDITOR.INF",cfg.node_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			return; }
		sprintf(str,"%s\r\n%s\r\n%u\r\n%s\r\n%s\r\n%u\r\n"
			,title,dest,useron.number
			,(subnum!=INVALID_SUB && cfg.sub[subnum]->misc&SUB_NAME) ? useron.name
			: useron.alias
			,useron.name,useron.level);
		write(file,str,strlen(str));
		close(file); }
}



/****************************************************************************/
/* Removes from file 'str' every LF terminated line that starts with 'str2' */
/* That is divisable by num. Function skips first 'skip' number of lines    */
/****************************************************************************/
void sbbs_t::removeline(char *str, char *str2, char num, char skip)
{
	char	HUGE16 *buf;
    char    slen;
    int     i,file;
	long	l=0,flen;
    FILE    *stream;

	if((file=nopen(str,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		return; }
	flen=filelength(file);
	slen=strlen(str2);
	if((buf=(char *)MALLOC(flen))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,flen);
		return; }
	if(lread(file,buf,flen)!=flen) {
		close(file);
		errormsg(WHERE,ERR_READ,str,flen);
		FREE(buf);
		return; }
	close(file);
	if((stream=fnopen(&file,str,O_WRONLY|O_TRUNC))==NULL) {
		close(file);
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_TRUNC);
		FREE(buf);
		return; }
	for(i=0;l<flen && i<skip;l++) {
		fputc(buf[l],stream);
		if(buf[l]==LF)
			i++; }
	while(l<flen) {
		if(!strncmp((char *)buf+l,str2,slen)) {
			for(i=0;i<num && l<flen;i++) {
				while(l<flen && buf[l]!=LF) l++;
				l++; } }
		else {
			for(i=0;i<num && l<flen;i++) {
				while(l<flen && buf[l]!=LF) fputc(buf[l++],stream);
				fputc(buf[l++],stream); } } }
	fclose(stream);
	FREE((char *)buf);
}

/*****************************************************************************/
/* The Synchronet editor.                                                    */
/* Returns the number of lines edited.                                       */
/*****************************************************************************/
ulong sbbs_t::msgeditor(char *buf, char *top, char *title)
{
	int		i,j,line,lines=0,maxlines;
	char	strin[256],**str,done=0;
	char 	tmp[512];
    ulong	l,m;

	if(online==ON_REMOTE) {
		rioctl(IOCM|ABORT);
		rioctl(IOCS|ABORT); }

	maxlines=cfg.level_linespermsg[useron.level];

	if((str=(char **)MALLOC(sizeof(char *)*(maxlines+1)))==NULL) {
		errormsg(WHERE,ERR_ALLOC,"msgeditor",sizeof(char *)*(maxlines+1));
		return(0); }
	m=strlen(buf);
	l=0;
	while(l<m && lines<maxlines) {
		msgabort(); /* to allow pausing */
		if((str[lines]=(char *)MALLOC(MAX_LINE_LEN))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,MAX_LINE_LEN);
			for(i=0;i<lines;i++)
				FREE(str[lines]);
			FREE(str);
			if(online==ON_REMOTE)
				rioctl(IOSM|ABORT);
			return(0); }
		for(i=0;i<79 && l<m;i++,l++) {
			if(buf[l]==CR) {
				l+=2;
				break; }
			if(buf[l]==TAB) {
				if(!(i%8))                  /* hard-coded tabstop of 8 */
					str[lines][i++]=SP;     /* for expansion */
				while(i%8 && i<79)
					str[lines][i++]=SP;
				i--;
				/***
				bprintf("\r\nMessage editor: Expanded tab on line #%d",lines+1);
				***/ }
			else str[lines][i]=buf[l]; }
		if(i==79) {
			if(buf[l]==CR)
				l+=2;
			else
				bprintf("\r\nMessage editor: Split line #%d",lines+1); }
		str[lines][i]=0;
		lines++; }
	if(lines)
		bprintf("\r\nMessage editor: Read in %d lines\r\n",lines);
	bprintf(text[EnterMsgNow],maxlines);
	for(i=0;i<79;i++)
		if(i%TABSIZE || !i)
			outchar('-');
		else outchar('+');
	CRLF;
	putmsg(top,P_SAVEATR|P_NOATCODES);
	for(line=0;line<lines && !msgabort();line++) { /* display lines in buf */
		putmsg(str[line],P_SAVEATR|P_NOATCODES);
		if(useron.misc&ANSI)
			bputs("\x1b[K");  /* delete to end of line */
		CRLF; }
	SYNC;
	if(online==ON_REMOTE)
		rioctl(IOSM|ABORT);
	while(online && !done) {
		checkline();
		if(line==lines) {
			if((str[line]=(char *)MALLOC(MAX_LINE_LEN))==NULL) {
				errormsg(WHERE,ERR_ALLOC,nulstr,MAX_LINE_LEN);
				for(i=0;i<lines;i++)
					FREE(str[i]);
				FREE(str);
				return(0); }
			str[line][0]=0; }
		if(line>(maxlines-10)) {
			if(line==maxlines)
				bputs(text[NoMoreLines]);
			else
				bprintf(text[OnlyNLinesLeft],maxlines-line); }
		strcpy(strin,str[line]);
		do {
			if(!line)
				outchar(CR);
			getstr(strin,79,K_WRAP|K_MSG|K_EDIT);
			} while(console&CON_UPARROW && !line);

		if(sys_status&SS_ABORT) {
			if(line==lines)
				FREE(str[line]);
			continue; }
		if(strin[0]=='/' && strlen(strin)<8) {
			if(!stricmp(strin,"/DEBUG") && SYSOP) {
				if(line==lines)
					FREE(str[line]);
				bprintf("\r\nline=%d lines=%d rows=%d\r\n",line,lines,rows);
				continue; }
			else if(!stricmp(strin,"/ABT")) {
				if(line==lines) 		/* delete a line */
					FREE(str[line]);
				for(i=0;i<lines;i++)
					FREE(str[i]);
				FREE(str);
				return(0); }
			else if(toupper(strin[1])=='D') {
				if(line==lines)         /* delete a line */
					FREE(str[line]);
				if(!lines)
					continue;
				i=atoi(strin+2)-1;
				if(i==-1)   /* /D means delete last line */
					i=lines-1;
				if(i>=lines || i<0)
					bputs(text[InvalidLineNumber]);
				else {
					FREE(str[i]);
					lines--;
					while(i<lines) {
						str[i]=str[i+1];
						i++; }
					if(line>lines)
						line=lines; }
				continue; }
			else if(toupper(strin[1])=='I') {
				if(line==lines)         /* insert a line before number x */
					FREE(str[line]);
				if(line==maxlines || !lines)
					continue;
				i=atoi(strin+2)-1;
				if(i==-1)
					i=lines-1;
				if(i>=lines || i<0)
					bputs(text[InvalidLineNumber]);
				else {
					for(line=lines;line>i;line--)   /* move the pointers */
						str[line]=str[line-1];
					if((str[i]=(char *)MALLOC(MAX_LINE_LEN))==NULL) {
						errormsg(WHERE,ERR_ALLOC,nulstr,MAX_LINE_LEN);
						for(i=0;i<lines;i++)
							FREE(str[i]);
						FREE(str);
						return(0); }
					str[i][0]=0;
					line=++lines; }
				continue; }
			else if(toupper(strin[1])=='E') {
				if(line==lines)         /* edit a line */
					FREE(str[line]);
				if(!lines)
					continue;
				i=atoi(strin+2)-1;
				j=K_MSG|K_EDIT; /* use j for the getstr mode */
				if(i==-1) { /* /E means edit last line */
					i=lines-1;
					j|=K_WRAP; }    /* wrap when editing last line */
				if(i>=lines || i<0)
					bputs(text[InvalidLineNumber]);
				else
					getstr(str[i],79,j);
				continue; }
			else if(!stricmp(strin,"/CLR")) {
				bputs(text[MsgCleared]);
				if(line!=lines)
					lines--;
				for(i=0;i<=lines;i++)
					FREE(str[i]);
				line=0;
				lines=0;
				putmsg(top,P_SAVEATR|P_NOATCODES);
				continue; }
			else if(toupper(strin[1])=='L') {   /* list message */
				if(line==lines)
					FREE(str[line]);
				if(lines)
					i=!noyes(text[WithLineNumbersQ]);
				CRLF;
				attr(LIGHTGRAY);
				putmsg(top,P_SAVEATR|P_NOATCODES);
				if(!lines) {
					continue; }
				j=atoi(strin+2);
				if(j) j--;  /* start from line j */
				while(j<lines && !msgabort()) {
					if(i) { /* line numbers */
						sprintf(tmp,"%3d: %-.74s",j+1,str[j]);
						putmsg(tmp,P_SAVEATR|P_NOATCODES); }
					else
						putmsg(str[j],P_SAVEATR|P_NOATCODES);
					if(useron.misc&ANSI)
						bputs("\x1b[K");  /* delete to end of line */
					CRLF;
					j++; }
				SYNC;
				continue; }
			else if(!stricmp(strin,"/S")) { /* Save */
				if(line==lines)
					FREE(str[line]);
				done=1;
				continue;}
			else if(!stricmp(strin,"/T")) { /* Edit title/subject */
				if(line==lines)
					FREE(str[line]);
				if(title[0]) {
					bputs(text[SubjectPrompt]);
					getstr(title,LEN_TITLE,K_LINE|K_EDIT|K_AUTODEL);
					SYNC;
					CRLF; }
				continue; }
			else if(!stricmp(strin,"/?")) {
				if(line==lines)
					FREE(str[line]);
				menu("editor"); /* User Editor Commands */
				SYNC;
				continue; }
			else if(!stricmp(strin,"/ATTR"))    {
				if(line==lines)
					FREE(str[line]);
				menu("attr");   /* User ANSI Commands */
				SYNC;
				continue; } }
		strcpy(str[line],strin);
		if(line<maxlines)
			line++;
		else
			FREE(str[line]);
		if(line>lines)
			lines++;
		if(console&CON_UPARROW) {
			outchar(CR);
			bprintf("\x1b[A\x1b[K");    /* up one line, clear to eol */
			line-=2; }
		}
	if(!online) {
		for(i=0;i<lines;i++)
			FREE(str[i]);
		FREE(str);
		return(0); }
	strcpy(buf,top);
	for(i=0;i<lines;i++) {
		strcat(buf,str[i]);
		strcat(buf,crlf);
		FREE(str[i]); }
	FREE(str);
	return(lines);
}


/****************************************************************************/
/* Edits an existing file or creates a new one in MSG format                */
/****************************************************************************/
void sbbs_t::editfile(char *str)
{
	char *buf,str2[128];
    int file;
	long length,maxlines,lines,l,mode=0;

	maxlines=cfg.level_linespermsg[useron.level];
	sprintf(str2,"%sQUOTES.TXT",cfg.node_dir);
	remove(str2);
	if(cfg.node_editor[0] && online==ON_LOCAL) {
		external(cmdstr(cfg.node_editor,str,nulstr,NULL),0,cfg.node_dir);
		return; }
	if(useron.xedit) {
		editor_inf(useron.xedit,nulstr,nulstr,0,INVALID_SUB);
		if(cfg.xedit[useron.xedit-1]->misc&XTRN_NATIVE)
			mode|=EX_NATIVE;
		if(cfg.xedit[useron.xedit-1]->misc&XTRN_SH)
			mode|=EX_SH;
		if(cfg.xedit[useron.xedit-1]->misc&IO_INTS) {
			if(online==ON_REMOTE)
				mode|=(EX_OUTR|EX_INR);
			if(cfg.xedit[useron.xedit-1]->misc&WWIVCOLOR)
				mode|=EX_WWIV; }
		if(online==ON_LOCAL)
			external(cmdstr(cfg.xedit[useron.xedit-1]->lcmd,str,nulstr,NULL),mode,cfg.node_dir);
		else {
			rioctl(IOCM|PAUSE|ABORT);
			external(cmdstr(cfg.xedit[useron.xedit-1]->rcmd,str,nulstr,NULL),mode,cfg.node_dir);
			rioctl(IOSM|PAUSE|ABORT); }
		return; }
	if((buf=(char *)MALLOC(maxlines*MAX_LINE_LEN))==NULL) {
		errormsg(WHERE,ERR_ALLOC,nulstr,maxlines*MAX_LINE_LEN);
		return; }
	if((file=nopen(str,O_RDONLY))!=-1) {
		length=filelength(file);
		if(length>(long)maxlines*MAX_LINE_LEN) {
			attr(cfg.color[clr_err]);
			bprintf("\7\r\nFile size (%lu bytes) is larger than (%lu).\r\n"
				,length,(ulong)maxlines*MAX_LINE_LEN);
			close(file);
			FREE(buf); }
		if(read(file,buf,length)!=length) {
			close(file);
			FREE(buf);
			errormsg(WHERE,ERR_READ,str,length);
			return; }
		buf[length]=0;
		close(file); }
	else {
		buf[0]=0;
		bputs(text[NewFile]); }
	if(!msgeditor(buf,nulstr,nulstr)) {
		FREE(buf);
		return; }
	bputs(text[Saving]);
	if((file=nopen(str,O_CREAT|O_WRONLY|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY|O_TRUNC);
		FREE(buf);
		return; }
	if((size_t)write(file,buf,strlen(buf))!=strlen(buf)) {
		close(file);
		errormsg(WHERE,ERR_WRITE,str,strlen(buf));
		FREE(buf);
		return; }
	for(l=lines=0;buf[l];l++)
		if(buf[l]==LF)
			lines++;
	bprintf(text[SavedNBytes],l,lines);
	close(file);
	FREE(buf);
	return;
}

/*************************/
/* Copy file attachments */
/*************************/
void sbbs_t::copyfattach(uint to, uint from, char *title)
{
	char str[128],str2[128],str3[128],*tp,*sp,*p;

	strcpy(str,title);
	tp=str;
	while(1) {
		p=strchr(tp,SP);
		if(p) *p=0;
		sp=strrchr(tp,'/');              /* sp is slash pointer */
		if(!sp) sp=strrchr(tp,'\\');
		if(sp) tp=sp+1;
		sprintf(str2,"%sfile/%04u.in/%s"  /* str2 is path/fname */
			,cfg.data_dir,to,tp);
		sprintf(str3,"%sfile/%04u.in/%s"  /* str2 is path/fname */
			,cfg.data_dir,from,tp);
		if(strcmp(str2,str3))
			mv(str3,str2,1);
		if(!p)
			break;
		tp=p+1; }
}


/****************************************************************************/
/* Forwards mail (fname) to usernumber                                      */
/* Called from function readmail											*/
/****************************************************************************/
void sbbs_t::forwardmail(smbmsg_t *msg, int usernumber)
{
	char		str[256],touser[128];
	char 		tmp[512];
	int			i;
	node_t		node;
	msghdr_t	hdr=msg->hdr;
	idxrec_t	idx=msg->idx;

	if(useron.etoday>=cfg.level_emailperday[useron.level] && !SYSOP) {
		bputs(text[TooManyEmailsToday]);
		return; }
	if(useron.rest&FLAG('F')) {
		bputs(text[R_Forward]);
		return; }
	if(usernumber==1 && useron.rest&FLAG('S')) {
		bprintf(text[R_Feedback],cfg.sys_op);
		return; }
	if(usernumber!=1 && useron.rest&FLAG('E')) {
		bputs(text[R_Email]);
		return; }

	msg->idx.attr&=~(MSG_READ|MSG_DELETE);
	msg->hdr.attr=msg->idx.attr;


	smb_hfield(msg,SENDER,strlen(useron.alias),useron.alias);
	sprintf(str,"%u",useron.number);
	smb_hfield(msg,SENDEREXT,strlen(str),str);

	username(&cfg,usernumber,touser);
	smb_hfield(msg,RECIPIENT,strlen(touser),touser);
	sprintf(str,"%u",usernumber);
	smb_hfield(msg,RECIPIENTEXT,sizeof(str),str);
	msg->idx.to=usernumber;

	now=time(NULL);
	smb_hfield(msg,FORWARDED,sizeof(time_t),&now);


	if((i=smb_open_da(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return; }
	if((i=smb_incdat(&smb,msg->hdr.offset,smb_getmsgdatlen(msg),1))!=0) {
		errormsg(WHERE,ERR_WRITE,smb.file,i);
		return; }
	smb_close_da(&smb);


	if((i=smb_addmsghdr(&smb,msg,SMB_SELFPACK))!=0) {
		errormsg(WHERE,ERR_WRITE,smb.file,i);
		return; }

	if(msg->hdr.auxattr&MSG_FILEATTACH)
		copyfattach(usernumber,useron.number,msg->subj);

	bprintf(text[Forwarded],username(&cfg,usernumber,str),usernumber);
	sprintf(str,"%s forwarded mail to %s #%d"
		,useron.alias
		,username(&cfg,usernumber,tmp)
		,usernumber);
	logline("E",str);
	msg->idx=idx;
	msg->hdr=hdr;


	if(usernumber==1) {
		useron.fbacks++;
		logon_fbacks++;
		putuserrec(&cfg,useron.number,U_FBACKS,5,ultoa(useron.fbacks,tmp,10)); }
	else {
		useron.emails++;
		logon_emails++;
		putuserrec(&cfg,useron.number,U_EMAILS,5,ultoa(useron.emails,tmp,10)); }
	useron.etoday++;
	putuserrec(&cfg,useron.number,U_ETODAY,5,ultoa(useron.etoday,tmp,10));

	for(i=1;i<=cfg.sys_nodes;i++) { /* Tell user, if online */
		getnodedat(i,&node,0);
		if(node.useron==usernumber && !(node.misc&NODE_POFF)
			&& (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
			sprintf(str,text[EmailNodeMsg],cfg.node_num,useron.alias);
			putnmsg(&cfg,i,str);
			break; } }
	if(i>cfg.sys_nodes) {	/* User wasn't online, so leave short msg */
		sprintf(str,text[UserSentYouMail],useron.alias);
		putsmsg(&cfg,usernumber,str); }
}

/****************************************************************************/
/* Auto-Message Routine ('A' from the main menu)                            */
/****************************************************************************/
void sbbs_t::automsg()
{
    char	str[256],buf[300],anon=0;
	char 	tmp[512];
	char	automsg[MAX_PATH+1];
    int		file;
	time_t	now=time(NULL);

	sprintf(automsg,"%smsgs/auto.msg",cfg.data_dir);
	while(online) {
		SYNC;
		mnemonics(text[AutoMsg]);
		switch(getkeys("RWQ",0)) {
			case 'R':
				printfile(automsg,P_NOABORT|P_NOATCODES);
				break;
			case 'W':
				if(useron.rest&FLAG('W')) {
					bputs(text[R_AutoMsg]);
					break; }
				action=NODE_AMSG;
				SYNC;
				bputs("\r\n3 lines:\r\n");
				if(!getstr(str,68,K_WRAP|K_MSG))
					break;
				strcpy(buf,str);
				strcat(buf,"\r\n          ");
				getstr(str,68,K_WRAP|K_MSG);
				strcat(buf,str);
				strcat(buf,"\r\n          ");
				getstr(str,68,K_MSG);
				strcat(str,crlf);
				strcat(buf,str);
				if(yesno(text[OK])) {
					if(useron.exempt&FLAG('A')) {
						if(!noyes(text[AnonymousQ]))
							anon=1; }
					if((file=nopen(automsg,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
						errormsg(WHERE,ERR_OPEN,automsg,O_WRONLY|O_CREAT|O_TRUNC);
						return; }
					if(anon)
						sprintf(tmp,"%.80s",text[Anonymous]);
					else
						sprintf(tmp,"%s #%d",useron.alias,useron.number);
					sprintf(str,text[AutoMsgBy],tmp,timestr(&now));
					strcat(str,"          ");
					write(file,str,strlen(str));
					write(file,buf,strlen(buf));
					close(file); }
				break;
			case 'Q':
				return; } }
}

/****************************************************************************/
/* Edits messages															*/
/****************************************************************************/
void sbbs_t::editmsg(smbmsg_t *msg, uint subnum)
{
	char	buf[SDT_BLOCK_LEN];
	char	msgtmp[MAX_PATH+1];
	ushort	xlat;
	int 	file,i,j,x;
	long	length,offset;
	FILE	*instream;

	if(!msg->hdr.total_dfields)
		return;
	if(useron.xedit && cfg.xedit[useron.xedit-1]->misc&QUICKBBS)
		sprintf(msgtmp,"%sMSGTMP",cfg.node_dir);	/* QuickBBS editors are dumb */
	else
		sprintf(msgtmp,"%sINPUT.MSG",cfg.temp_dir);

	remove(msgtmp);
	msgtotxt(msg,msgtmp,0,1);
	editfile(msgtmp);
	length=flength(msgtmp);
	if(length<1L)
		return;

	length+=2;	 /* +2 for translation string */

	if((i=smb_locksmbhdr(&smb))!=0) {
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		return; }

	if((i=smb_getstatus(&smb))!=0) {
		errormsg(WHERE,ERR_READ,smb.file,i);
		return; }

	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		if((i=smb_open_da(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			return; }
		for(x=0;x<msg->hdr.total_dfields;x++)
			if((i=smb_freemsgdat(&smb,msg->hdr.offset+msg->dfield[x].offset
				,msg->dfield[x].length,1))!=0)
				errormsg(WHERE,ERR_WRITE,smb.file,i,smb.last_error); }

	msg->dfield[0].type=TEXT_BODY;				/* Make one single data field */
	msg->dfield[0].length=length;
	msg->dfield[0].offset=0;
	for(x=1;x<msg->hdr.total_dfields;x++) { 	/* Clear the other data fields */
		msg->dfield[x].type=UNUSED; 			/* so we leave the header length */
		msg->dfield[x].length=0;				/* unchanged */
		msg->dfield[x].offset=0; }


	if(smb.status.attr&SMB_HYPERALLOC) {
		offset=smb_hallocdat(&smb); }
	else {
		if((subnum!=INVALID_SUB && cfg.sub[subnum]->misc&SUB_FAST)
			|| (subnum==INVALID_SUB && cfg.sys_misc&SM_FASTMAIL))
			offset=smb_fallocdat(&smb,length,1);
		else
			offset=smb_allocdat(&smb,length,1);
		smb_close_da(&smb); }

	msg->hdr.offset=offset;
	if((file=open(msgtmp,O_RDONLY|O_BINARY))==-1
		|| (instream=fdopen(file,"rb"))==NULL) {
		smb_unlocksmbhdr(&smb);
		smb_freemsgdat(&smb,offset,length,1);
		errormsg(WHERE,ERR_OPEN,msgtmp,O_RDONLY|O_BINARY);
		return; }

	setvbuf(instream,NULL,_IOFBF,2*1024);
	fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
	while(!feof(instream)) {
		memset(buf,0,x);
		j=fread(buf,1,x,instream);
		if(j<1)
			break;
		if(j>1 && (j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
			buf[j-1]=buf[j-2]=0;	/* Convert to NULL */
		fwrite(buf,j,1,smb.sdt_fp);
		x=SDT_BLOCK_LEN; }
	fflush(smb.sdt_fp);
	fclose(instream);

	smb_unlocksmbhdr(&smb);
	msg->hdr.length=(ushort)smb_getmsghdrlen(msg);
	if((i=smb_putmsghdr(&smb,msg))!=0)
		errormsg(WHERE,ERR_WRITE,smb.file,i);
}

/****************************************************************************/
/* Moves a message from one message base to another 						*/
/****************************************************************************/
bool sbbs_t::movemsg(smbmsg_t* msg, uint subnum)
{
	char str[256],*buf;
	uint i;
	int file,newgrp,newsub,storage;
	ulong offset,length;

	for(i=0;i<usrgrps;i++)		 /* Select New Group */
		uselect(1,i,"Message Group",cfg.grp[usrgrp[i]]->lname,0);
	if((newgrp=uselect(0,0,0,0,0))<0)
		return(false);

	for(i=0;i<usrsubs[newgrp];i++)		 /* Select New Sub-Board */
		uselect(1,i,"Sub-Board",cfg.sub[usrsub[newgrp][i]]->lname,0);
	if((newsub=uselect(0,0,0,0,0))<0)
		return(false);
	newsub=usrsub[newgrp][newsub];

	length=smb_getmsgdatlen(msg);
	if((buf=(char *)MALLOC(length))==NULL) {
		errormsg(WHERE,ERR_ALLOC,smb.file,length);
		return(false); }

	fseek(smb.sdt_fp,msg->hdr.offset,SEEK_SET);
	fread(buf,length,1,smb.sdt_fp);

	if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
		FREE(buf);
		errormsg(WHERE,ERR_OPEN,cfg.sub[newsub]->code,i);
		return(false); }

	sprintf(smb.file,"%s%s",cfg.sub[newsub]->data_dir,cfg.sub[newsub]->code);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=newsub;
	if((i=smb_open(&smb))!=0) {
		FREE(buf);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
		return(false); }

	if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
		smb.status.max_crcs=cfg.sub[newsub]->maxcrcs;
		smb.status.max_msgs=cfg.sub[newsub]->maxmsgs;
		smb.status.max_age=cfg.sub[newsub]->maxage;
		smb.status.attr=cfg.sub[newsub]->misc&SUB_HYPER ? SMB_HYPERALLOC :0;
		if((i=smb_create(&smb))!=0) {
			FREE(buf);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_CREATE,smb.file,i);
			return(false); } }

	if((i=smb_locksmbhdr(&smb))!=0) {
		FREE(buf);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		return(false); }

	if((i=smb_getstatus(&smb))!=0) {
		FREE(buf);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_READ,smb.file,i);
		return(false); }

	if(smb.status.attr&SMB_HYPERALLOC) {
		offset=smb_hallocdat(&smb);
		storage=SMB_HYPERALLOC; }
	else {
		if((i=smb_open_da(&smb))!=0) {
			FREE(buf);
			smb_close(&smb);
			smb_stack(&smb,SMB_STACK_POP);
			errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
			return(false); }
		if(cfg.sub[newsub]->misc&SUB_FAST) {
			offset=smb_fallocdat(&smb,length,1);
			storage=SMB_FASTALLOC; }
		else {
			offset=smb_allocdat(&smb,length,1);
			storage=SMB_SELFPACK; }
		smb_close_da(&smb); }

	msg->hdr.offset=offset;
	msg->hdr.version=smb_ver();

	fseek(smb.sdt_fp,offset,SEEK_SET);
	fwrite(buf,length,1,smb.sdt_fp);
	fflush(smb.sdt_fp);
	FREE(buf);

	i=smb_addmsghdr(&smb,msg,storage);	// calls smb_unlocksmbhdr() 
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);

	if(i) {
		smb_freemsgdat(&smb,offset,length,1);
		errormsg(WHERE,ERR_WRITE,smb.file,i);
		return(false); }

	bprintf("\r\nMoved to %s %s\r\n\r\n"
		,cfg.grp[usrgrp[newgrp]]->sname,cfg.sub[newsub]->lname);
	sprintf(str,"%s moved message from %s %s to %s %s"
		,useron.alias
		,cfg.grp[newgrp]->sname,cfg.sub[newsub]->sname
		,cfg.grp[cfg.sub[subnum]->grp]->sname,cfg.sub[subnum]->sname);
	logline("M+",str);
	if(cfg.sub[newsub]->misc&SUB_FIDO && cfg.sub[newsub]->echomail_sem[0])
		if((file=nopen(cmdstr(cfg.sub[newsub]->echomail_sem,nulstr,nulstr,NULL)
			,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
			close(file);
	return(true);
}

ushort sbbs_t::chmsgattr(ushort attr)
{
	int ch;

	while(online && !(sys_status&SS_ABORT)) {
		CRLF;
		show_msgattr(attr);
		menu("msgattr");
		ch=getkey(K_UPPER);
		if(ch)
			bprintf("%c\r\n",ch);
		switch(ch) {
			case 'P':
				attr^=MSG_PRIVATE;
				break;
			case 'R':
				attr^=MSG_READ;
				break;
			case 'K':
				attr^=MSG_KILLREAD;
				break;
			case 'A':
				attr^=MSG_ANONYMOUS;
				break;
			case 'N':   /* Non-purgeable */
				attr^=MSG_PERMANENT;
				break;
			case 'M':
				attr^=MSG_MODERATED;
				break;
			case 'V':
				attr^=MSG_VALIDATED;
				break;
			case 'D':
				attr^=MSG_DELETE;
				break;
			case 'L':
				attr^=MSG_LOCKED;
				break;
			default:
				return(attr); } }
	return(attr);
}
