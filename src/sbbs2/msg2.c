#line 1 "MSG2.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

#define MAX_LINE_LEN 82L

char *qstr=" > %.76s\r\n";

int qwk_route(char *inaddr, char *fulladdr);

void remove_re(char *str)
{
while(!strnicmp(str,"RE:",3)) {
	strcpy(str,str+3);
	while(str[0]==SP)
		strcpy(str,str+1); }
}

/****************************************************************************/
/* Modify 'str' to for quoted format. Remove ^A codes, etc.                 */
/****************************************************************************/
void quotestr(char *str)
{
	char tmp[512];
	int i,j;

j=strlen(str);
while(j && (str[j-1]==SP || str[j-1]==LF || str[j-1]==CR)) j--;
str[j]=0;
remove_ctrl_a(str);
}

void editor_inf(int xeditnum,char *dest, char *title,int mode
	,uint subnum)
{
	char str[512];
	int file;

xeditnum--;

if(xedit[xeditnum]->misc&QUICKBBS) {
	sprintf(str,"%sMSGINF",node_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }
	sprintf(str,"%s\r\n%s\r\n%s\r\n%u\r\n%s\r\n%s\r\n"
		,(subnum!=INVALID_SUB && sub[subnum]->misc&SUB_NAME) ? useron.name
			: useron.alias
			,dest,title,1
			,mode&WM_NETMAIL ? "NetMail"
			:mode&WM_EMAIL ? "Electronic Mail"
			:subnum==INVALID_SUB ? nulstr
			:sub[subnum]->sname
		,mode&WM_PRIVATE ? "YES":"NO");
	write(file,str,strlen(str));
	close(file); }
else {
	sprintf(str,"%sEDITOR.INF",node_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
		return; }
	sprintf(str,"%s\r\n%s\r\n%u\r\n%s\r\n%s\r\n%u\r\n"
		,title,dest,useron.number
		,(subnum!=INVALID_SUB && sub[subnum]->misc&SUB_NAME) ? useron.name
		: useron.alias
		,useron.name,useron.level);
	write(file,str,strlen(str));
	close(file); }
}

/****************************************************************************/
/* Creates a message (post or mail) using standard line editor. 'fname' is  */
/* is name of file to create, 'top' is a buffer to place at beginning of    */
/* message and 'title' is the title (70chars max) for the message.          */
/* 'dest' contains a text description of where the message is going.        */
/* Returns positive if successful or 0 if aborted.                          */
/****************************************************************************/
char writemsg(char *fname, char *top, char *title, int mode, int subnum
	,char *dest)
{
	uchar str[256],quote[128],msgtmp[32],c,HUGE16 *buf,ex_mode=0,*p,*tp
		,useron_level;
	int i,j,file,linesquoted=0;
	long length,qlen,qtime;
	ulong l;
	FILE *stream;

useron_level=useron.level;

if((buf=(char HUGE16*)LMALLOC(level_linespermsg[useron_level]*MAX_LINE_LEN))
	==NULL) {
	errormsg(WHERE,ERR_ALLOC,fname
		,level_linespermsg[useron_level]*MAX_LINE_LEN);
    return(0); }

if(mode&WM_NETMAIL ||
	(!(mode&(WM_EMAIL|WM_NETMAIL)) && sub[subnum]->misc&SUB_PNET))
	mode|=WM_NOTOP;

if(useron.xedit && xedit[useron.xedit-1]->misc&QUICKBBS)
	strcpy(msgtmp,"MSGTMP");
else
	strcpy(msgtmp,"INPUT.MSG");

if(mode&WM_QUOTE && !(useron.rest&FLAG('J'))
	&& ((mode&(WM_EMAIL|WM_NETMAIL) && sys_misc&SM_QUOTE_EM)
	|| (!(mode&(WM_EMAIL|WM_NETMAIL)) && (uint)subnum!=INVALID_SUB
		&& sub[subnum]->misc&SUB_QUOTE))) {

	/* Quote entire message to MSGTMP or INPUT.MSG */

	if(useron.xedit && xedit[useron.xedit-1]->misc&QUOTEALL) {
		sprintf(str,"%sQUOTES.TXT",node_dir);
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			LFREE(buf);
			return(0); }

		sprintf(str,"%s%s",node_dir,msgtmp);    /* file for quoted msg */
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			LFREE(buf);
			fclose(stream);
			return(0); }

		while(!feof(stream) && !ferror(stream)) {
			if(!fgets(str,255,stream))
				break;
			quotestr(str);
			sprintf(tmp,qstr,str);
			write(file,tmp,strlen(tmp));
			linesquoted++; }
		fclose(stream);
		close(file); }

	/* Quote nothing to MSGTMP or INPUT.MSG automatically */

	else if(useron.xedit && xedit[useron.xedit-1]->misc&QUOTENONE)
		;

	else if(yesno(text[QuoteMessageQ])) {
		sprintf(str,"%sQUOTES.TXT",node_dir);
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			LFREE(buf);
			return(0); }

		sprintf(str,"%s%s",node_dir,msgtmp);    /* file for quoted msg */
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
			LFREE(buf);
			fclose(stream);
			return(0); }

		l=ftell(stream);			/* l now points to start of message */

		while(online) {
			sprintf(str,text[QuoteLinesPrompt],linesquoted ? "Done":"All");
			mnemonics(str);
			i=getstr(quote,10,K_UPPER);
			if(sys_status&SS_ABORT) {
				fclose(stream);
				close(file);
				LFREE(buf);
				return(0); }
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
	sprintf(str,"%sQUOTES.TXT",node_dir);
	remove(str); }

if(!online || sys_status&SS_ABORT) {
	LFREE(buf);
	return(0); }

if(!(mode&WM_EXTDESC)) {
	if(mode&WM_FILE) {
		c=12;
		CRLF;
		bputs(text[Filename]); }
	else {
		c=LEN_TITLE;
		bputs(text[TitlePrompt]); }
	if(!(mode&(WM_EMAIL|WM_NETMAIL)) && !(mode&WM_FILE)
		&& sub[subnum]->misc&(SUB_QNET /* |SUB_PNET */ ))
        c=25;
	if(mode&WM_QWKNET)
		c=25;
	if(!getstr(title,c,mode&WM_FILE ? K_LINE|K_UPPER : K_LINE|K_EDIT|K_AUTODEL)
		&& useron_level && useron.logons) {
		LFREE(buf);
        return(0); }
    if(!(mode&(WM_EMAIL|WM_NETMAIL)) && sub[subnum]->misc&SUB_QNET
		&& !SYSOP
		&& (!stricmp(title,"DROP") || !stricmp(title,"ADD")
		|| !strnicmp(dest,"SBBS",4))) {
		LFREE(buf);   /* Users can't post DROP or ADD in QWK netted subs */
		return(0); } }	/* or messages to "SBBS" */

if(!online || sys_status&SS_ABORT) {
	LFREE(buf);
	return(0); }

/* Create WWIV compatible EDITOR.INF file */

if(useron.xedit) {
	editor_inf(useron.xedit,dest,title,mode,subnum);
	if(xedit[useron.xedit-1]->type) {
		gettimeleft();
		xtrndat(useron.alias,node_dir,xedit[useron.xedit-1]->type,timeleft); }
	}

if(console&CON_RAW_IN) {
    bprintf(text[EnterMsgNowRaw]
		,(ulong)level_linespermsg[useron_level]*MAX_LINE_LEN);
	if(top[0] && !(mode&WM_NOTOP)) {
		strcpy((char *)buf,top);
		strcat((char *)buf,crlf);
		l=strlen((char *)buf); }
	else
        l=0;
	while(l<level_linespermsg[useron_level]*MAX_LINE_LEN) {
        c=getkey(0);
        if(sys_status&SS_ABORT) {  /* Ctrl-C */
			LFREE(buf);
            return(0); }
        if((c==ESC || c==1) && useron.rest&FLAG('A')) /* ANSI restriction */
            continue;
#ifndef __OS2__
		if(lclaes() && c=='"') c=7; /* convert keyboard reassignmnet to beep */
#endif
        if(c==7 && useron.rest&FLAG('B'))   /* Beep restriction */
            continue;
		if(!(console&CON_RAW_IN))	/* Ctrl-Z was hit */
            break;
        outchar(c);
        buf[l++]=c; }
    buf[l]=0;
	if(l==level_linespermsg[useron_level]*MAX_LINE_LEN)
		bputs(text[OutOfBytes]); }


else if((online==ON_LOCAL && node_misc&NM_LCL_EDIT && node_editor[0])
    || useron.xedit) {

	if(useron.xedit && xedit[useron.xedit-1]->misc&IO_INTS) {
		ex_mode|=EX_OUTL;
		if(online==ON_REMOTE)
			ex_mode|=(EX_OUTR|EX_INR);
		if(xedit[useron.xedit-1]->misc&WWIVCOLOR)
			ex_mode|=EX_WWIV; }

	sprintf(str,"%s%s",node_dir,msgtmp);    /* temporary file for input */
	if(!linesquoted && fexist(str))
		remove(str);
	if(linesquoted) {
		qlen=flength(str);
		qtime=fdate(str); }
    if(online==ON_LOCAL) {
        if(node_misc&NM_LCL_EDIT && node_editor[0])
			external(cmdstr(node_editor,str,nulstr,NULL)
				,0);  /* EX_CC removed and EX_OUTL removed */
        else
			external(cmdstr(xedit[useron.xedit-1]->lcmd,str,nulstr,NULL)
				,ex_mode); }

    else {
        rioctl(IOCM|PAUSE|ABORT);
		external(cmdstr(xedit[useron.xedit-1]->rcmd,str,nulstr,NULL),ex_mode);
        rioctl(IOSM|PAUSE|ABORT); }
	checkline();
	if(!fexist(str) || !online
		|| (linesquoted && qlen==flength(str) && qtime==fdate(str))) {
		LFREE(buf);
        return(0); }
	buf[0]=0;
	if(!(mode&WM_NOTOP))
		strcpy((char *)buf,top);
	if((file=nopen(str,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		LFREE(buf);
        return(0); }
    length=filelength(file);
	l=strlen((char *)buf);	  /* reserve space for top and terminating null */
	/* truncate if too big */
	if(length>(level_linespermsg[useron_level]*MAX_LINE_LEN)-(l+1)) {
		length=(level_linespermsg[useron_level]*MAX_LINE_LEN)-(l+1);
		bputs(text[OutOfBytes]); }
	lread(file,buf+l,length);
    close(file);
	// remove(str); 	   /* no need to save the temp input file */
	buf[l+length]=0; }
else {
    buf[0]=0;
	if(linesquoted) {
		sprintf(str,"%s%s",node_dir,msgtmp);
		if((file=nopen(str,O_RDONLY))!=-1) {
			length=filelength(file);
			l=length>level_linespermsg[useron_level]*MAX_LINE_LEN
				? level_linespermsg[useron_level]*MAX_LINE_LEN : length;
			lread(file,buf,l);
			buf[l]=0;
			close(file);
			// remove(str);
			} }
	if(!(msgeditor((char *)buf,mode&WM_NOTOP ? nulstr : top,title))) {
		LFREE(buf);
		return(0); } }

now=time(NULL);
bputs(text[Saving]);
if((stream=fnopen(&file,fname,O_WRONLY|O_CREAT|O_TRUNC))==NULL) {
	errormsg(WHERE,ERR_OPEN,fname,O_WRONLY|O_CREAT|O_TRUNC);
	LFREE(buf);
    return(0); }
for(l=i=0;buf[l] && i<level_linespermsg[useron_level];l++) {
	if(buf[l]==141 && useron.xedit && xedit[useron.xedit-1]->misc&QUICKBBS) {
		fwrite(crlf,2,1,stream);
		i++;
		continue; }
	if(buf[l]==LF && (!l || buf[l-1]!=CR) && useron.xedit
		&& xedit[useron.xedit-1]->misc&EXPANDLF) {
		fwrite(crlf,2,1,stream);
		i++;
		continue; }
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
fclose(stream);
LFREE((char *)buf);
bprintf(text[SavedNBytes],l,i);
return(1);
}


/****************************************************************************/
/* Removes from file 'str' every LF terminated line that starts with 'str2' */
/* That is divisable by num. Function skips first 'skip' number of lines    */
/****************************************************************************/
void removeline(char *str, char *str2, char num, char skip)
{
	char	HUGE16 *buf;
    char    slen,c;
    int     i,file;
	ulong	l=0,m,flen;
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
ulong msgeditor(char *buf, char *top, char *title)
{
	int i,j,line,lines=0,maxlines;
	char strin[256],**str,done=0;
    ulong l,m;

if(online==ON_REMOTE) {
    rioctl(IOCM|ABORT);
    rioctl(IOCS|ABORT); }

maxlines=level_linespermsg[useron.level];

if((str=(char **)MALLOC(sizeof(char *)*(maxlines+1)))==NULL) {
	errormsg(WHERE,ERR_ALLOC,"msgeditor",sizeof(char *)*(maxlines+1));
	return(0); }
m=strlen(buf);
l=0;
while(l<m && lines<maxlines) {
    msgabort(); /* to allow pausing */
	if((str[lines]=MALLOC(MAX_LINE_LEN))==NULL) {
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
		if((str[line]=MALLOC(MAX_LINE_LEN))==NULL) {
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
				if((str[i]=MALLOC(MAX_LINE_LEN))==NULL) {
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
                i=!noyes(text[WithLineNumbers]);
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
        else if(!stricmp(strin,"/T")) { /* Edit title */
            if(line==lines)
				FREE(str[line]);
            if(title[0]) {
                bputs(text[TitlePrompt]);
				getstr(title,LEN_TITLE,K_LINE|K_EDIT|K_AUTODEL);
                SYNC;
                CRLF; }
            continue; }
        else if(!stricmp(strin,"/?")) {
            if(line==lines)
				FREE(str[line]);
            menu("EDITOR"); /* User Editor Commands */
            SYNC;
            continue; }
		else if(!stricmp(strin,"/ATTR"))    {
            if(line==lines)
				FREE(str[line]);
            menu("ATTR");   /* User ANSI Commands */
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
void editfile(char *str)
{
	char *buf,str2[128],mode=0;   /* EX_CC */
    int file;
	long length,maxlines,lines,l;

maxlines=level_linespermsg[useron.level];
sprintf(str2,"%sQUOTES.TXT",node_dir);
remove(str2);
if(node_editor[0] && online==ON_LOCAL) {
	external(cmdstr(node_editor,str,nulstr,NULL),0); /* EX_CC */
    return; }
if(useron.xedit) {
	editor_inf(useron.xedit,nulstr,nulstr,0,INVALID_SUB);
	if(xedit[useron.xedit-1]->misc&IO_INTS) {
		mode|=EX_OUTL;
		if(online==ON_REMOTE)
			mode|=(EX_OUTR|EX_INR);
		if(xedit[useron.xedit-1]->misc&WWIVCOLOR)
			mode|=EX_WWIV; }
    if(online==ON_LOCAL)
		external(cmdstr(xedit[useron.xedit-1]->lcmd,str,nulstr,NULL),mode);
    else {
        rioctl(IOCM|PAUSE|ABORT);
		external(cmdstr(xedit[useron.xedit-1]->rcmd,str,nulstr,NULL),mode);
        rioctl(IOSM|PAUSE|ABORT); }
    return; }
if((buf=(char *)MALLOC(maxlines*MAX_LINE_LEN))==NULL) {
	errormsg(WHERE,ERR_ALLOC,nulstr,maxlines*MAX_LINE_LEN);
    return; }
if((file=nopen(str,O_RDONLY))!=-1) {
    length=filelength(file);
	if(length>(ulong)maxlines*MAX_LINE_LEN) {
        attr(color[clr_err]);
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
if(write(file,buf,strlen(buf))!=strlen(buf)) {
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
void copyfattach(uint to, uint from, char *title)
{
	char str[128],str2[128],str3[128],*tp,*sp,*p;
    uint i;

strcpy(str,title);
tp=str;
while(1) {
    p=strchr(tp,SP);
    if(p) *p=0;
    sp=strrchr(tp,'/');              /* sp is slash pointer */
    if(!sp) sp=strrchr(tp,'\\');
    if(sp) tp=sp+1;
    sprintf(str2,"%sFILE\\%04u.IN\\%s"  /* str2 is path/fname */
        ,data_dir,to,tp);
	sprintf(str3,"%sFILE\\%04u.IN\\%s"  /* str2 is path/fname */
		,data_dir,from,tp);
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
void forwardmail(smbmsg_t *msg, ushort usernumber)
{
	char str[256],touser[128];
	int i;
	node_t		node;
	msghdr_t	hdr=msg->hdr;
	idxrec_t	idx=msg->idx;

if(useron.etoday>=level_emailperday[useron.level] && !SYSOP) {
	bputs(text[TooManyEmailsToday]);
    return; }
if(useron.rest&FLAG('F')) {
    bputs(text[R_Forward]);
    return; }
if(usernumber==1 && useron.rest&FLAG('S')) {
    bprintf(text[R_Feedback],sys_op);
    return; }
if(usernumber!=1 && useron.rest&FLAG('E')) {
    bputs(text[R_Email]);
    return; }

msg->idx.attr&=~(MSG_READ|MSG_DELETE);
msg->hdr.attr=msg->idx.attr;


smb_hfield(msg,SENDER,strlen(useron.alias),useron.alias);
sprintf(str,"%u",useron.number);
smb_hfield(msg,SENDEREXT,strlen(str),str);

username(usernumber,touser);
smb_hfield(msg,RECIPIENT,strlen(touser),touser);
sprintf(str,"%u",usernumber);
smb_hfield(msg,RECIPIENTEXT,sizeof(str),str);
msg->idx.to=usernumber;

now=time(NULL);
smb_hfield(msg,FORWARDED,sizeof(time_t),&now);


if((i=smb_open_da(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
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

bprintf(text[Forwarded],username(usernumber,str),usernumber);
sprintf(str,"Forwarded mail to %s #%d",username(usernumber,tmp)
    ,usernumber);
logline("E",str);
msg->idx=idx;
msg->hdr=hdr;


if(usernumber==1) {
    useron.fbacks++;
    logon_fbacks++;
    putuserrec(useron.number,U_FBACKS,5,itoa(useron.fbacks,tmp,10)); }
else {
    useron.emails++;
    logon_emails++;
    putuserrec(useron.number,U_EMAILS,5,itoa(useron.emails,tmp,10)); }
useron.etoday++;
putuserrec(useron.number,U_ETODAY,5,itoa(useron.etoday,tmp,10));

for(i=1;i<=sys_nodes;i++) { /* Tell user, if online */
    getnodedat(i,&node,0);
    if(node.useron==usernumber && !(node.misc&NODE_POFF)
        && (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
        sprintf(str,text[EmailNodeMsg],node_num,useron.alias);
		putnmsg(i,str);
        break; } }
if(i>sys_nodes) {	/* User wasn't online, so leave short msg */
	sprintf(str,text[UserSentYouMail],useron.alias);
    putsmsg(usernumber,str); }
}

/****************************************************************************/
/* Auto-Message Routine ('A' from the main menu)                            */
/****************************************************************************/
void automsg()
{
    char str[256],buf[300],anon=0;
    int file;
    struct ffblk ff;

while(online) {
    SYNC;
    mnemonics(text[AutoMsg]);
    switch(getkeys("RWQ",0)) {
        case 'R':
            sprintf(str,"%sMSGS\\AUTO.MSG",data_dir);
			printfile(str,P_NOABORT|P_NOATCODES);
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
                sprintf(str,"%sMSGS\\AUTO.MSG",data_dir);
                if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
                    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
                    return; }
                if(anon)
                    sprintf(tmp,"%.80s",text[Anonymous]);
                else
                    sprintf(tmp,"%s #%d",useron.alias,useron.number);
                sprintf(str,text[AutoMsgBy],tmp);
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
void editmsg(smbmsg_t *msg, uint subnum)
{
	char	str[256],buf[SDT_BLOCK_LEN];
	ushort	xlat;
	int 	file,i,j,x;
	ulong	length,offset;
	FILE	*instream;

if(!msg->hdr.total_dfields)
	return;
sprintf(str,"%sINPUT.MSG",node_dir);
remove(str);
msgtotxt(*msg,str,0,1);
editfile(str);
length=flength(str)+2;	 /* +2 for translation string */
if(length<1L)
	return;

if((i=smb_locksmbhdr(&smb))!=0) {
	errormsg(WHERE,ERR_LOCK,smb.file,i);
    return; }

if((i=smb_getstatus(&smb))!=0) {
	errormsg(WHERE,ERR_READ,smb.file,i);
	return; }

if(!(smb.status.attr&SMB_HYPERALLOC)) {
	if((i=smb_open_da(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,i);
        return; }
	for(x=0;x<msg->hdr.total_dfields;x++)
		if((i=smb_freemsgdat(&smb,msg->hdr.offset+msg->dfield[x].offset
			,msg->dfield[x].length,1))!=0)
			errormsg(WHERE,ERR_WRITE,smb.file,i); }

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
	if((subnum!=INVALID_SUB && sub[subnum]->misc&SUB_FAST)
		|| (subnum==INVALID_SUB && sys_misc&SM_FASTMAIL))
		offset=smb_fallocdat(&smb,length,1);
	else
		offset=smb_allocdat(&smb,length,1);
	smb_close_da(&smb); }

msg->hdr.offset=offset;
if((file=open(str,O_RDONLY|O_BINARY))==-1
	|| (instream=fdopen(file,"rb"))==NULL) {
	smb_unlocksmbhdr(&smb);
	smb_freemsgdat(&smb,offset,length,1);
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_BINARY);
	return; }

setvbuf(instream,NULL,_IOFBF,2*1024);
fseek(smb.sdt_fp,offset,SEEK_SET);
xlat=XLAT_NONE;
fwrite(&xlat,2,1,smb.sdt_fp);
x=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
while(!feof(instream)) {
	memset(buf,0,x);
	j=fread(buf,1,x,instream);
	if((j!=x || feof(instream)) && buf[j-1]==LF && buf[j-2]==CR)
		buf[j-1]=buf[j-2]=0;	/* Convert to NULL */
	fwrite(buf,j,1,smb.sdt_fp);
	x=SDT_BLOCK_LEN; }
fflush(smb.sdt_fp);
fclose(instream);

smb_unlocksmbhdr(&smb);
msg->hdr.length=smb_getmsghdrlen(msg);
if((i=smb_putmsghdr(&smb,msg))!=0)
	errormsg(WHERE,ERR_WRITE,smb.file,i);
}

/****************************************************************************/
/* Moves a message from one message base to another 						*/
/****************************************************************************/
char movemsg(smbmsg_t msg, uint subnum)
{
	char str[256],*buf;
	int i,j,x,file,newgrp,newsub,storage;
	long l;
	ulong offset,length;

for(i=0;i<usrgrps;i++)		 /* Select New Group */
	uselect(1,i,"Message Group",grp[usrgrp[i]]->lname,0);
if((newgrp=uselect(0,0,0,0,0))<0)
	return(0);

for(i=0;i<usrsubs[newgrp];i++)		 /* Select New Sub-Board */
	uselect(1,i,"Sub-Board",sub[usrsub[newgrp][i]]->lname,0);
if((newsub=uselect(0,0,0,0,0))<0)
	return(0);
newsub=usrsub[newgrp][newsub];

length=smb_getmsgdatlen(&msg);
if((buf=(char *)MALLOC(length))==NULL) {
	errormsg(WHERE,ERR_ALLOC,smb.file,length);
	return(0); }

fseek(smb.sdt_fp,msg.hdr.offset,SEEK_SET);
fread(buf,length,1,smb.sdt_fp);

if((i=smb_stack(&smb,SMB_STACK_PUSH))!=0) {
	FREE(buf);
	errormsg(WHERE,ERR_OPEN,sub[newsub]->code,i);
	return(0); }

sprintf(smb.file,"%s%s",sub[newsub]->data_dir,sub[newsub]->code);
smb.retry_time=smb_retry_time;
if((i=smb_open(&smb))!=0) {
	FREE(buf);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	return(0); }

if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
	smb.status.max_crcs=sub[newsub]->maxcrcs;
	smb.status.max_msgs=sub[newsub]->maxmsgs;
	smb.status.max_age=sub[newsub]->maxage;
	smb.status.attr=sub[newsub]->misc&SUB_HYPER ? SMB_HYPERALLOC :0;
	if((i=smb_create(&smb))!=0) {
		FREE(buf);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_CREATE,smb.file,i);
		return(0); } }

if((i=smb_locksmbhdr(&smb))!=0) {
	FREE(buf);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_LOCK,smb.file,i);
	return(0); }

if((i=smb_getstatus(&smb))!=0) {
	FREE(buf);
	smb_close(&smb);
	smb_stack(&smb,SMB_STACK_POP);
	errormsg(WHERE,ERR_READ,smb.file,i);
	return(0); }

if(smb.status.attr&SMB_HYPERALLOC) {
	offset=smb_hallocdat(&smb);
    storage=SMB_HYPERALLOC; }
else {
	if((i=smb_open_da(&smb))!=0) {
		FREE(buf);
		smb_close(&smb);
		smb_stack(&smb,SMB_STACK_POP);
		errormsg(WHERE,ERR_OPEN,smb.file,i);
		return(0); }
	if(sub[newsub]->misc&SUB_FAST) {
		offset=smb_fallocdat(&smb,length,1);
		storage=SMB_FASTALLOC; }
	else {
		offset=smb_allocdat(&smb,length,1);
		storage=SMB_SELFPACK; }
	smb_close_da(&smb); }

msg.hdr.offset=offset;

memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=smb_ver();

smb_unlocksmbhdr(&smb);

fseek(smb.sdt_fp,offset,SEEK_SET);
fwrite(buf,length,1,smb.sdt_fp);
fflush(smb.sdt_fp);
FREE(buf);

i=smb_addmsghdr(&smb,&msg,storage);
smb_close(&smb);
smb_stack(&smb,SMB_STACK_POP);

if(i) {
	smb_freemsgdat(&smb,offset,length,1);
	errormsg(WHERE,ERR_WRITE,smb.file,i);
	return(0); }

bprintf("\r\nMoved to %s %s\r\n\r\n"
	,grp[usrgrp[newgrp]]->sname,sub[newsub]->lname);
sprintf(str,"Moved message from %s %s to %s %s"
	,grp[newgrp]->sname,sub[newsub]->sname
	,grp[sub[subnum]->grp]->sname,sub[subnum]->sname);
logline("M+",str);
if(sub[newsub]->misc&SUB_FIDO && sub[newsub]->echomail_sem[0])
	if((file=nopen(sub[newsub]->echomail_sem
		,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
		close(file);
return(1);
}

ushort chmsgattr(ushort attr)
{
	int ch;

while(online && !(sys_status&SS_ABORT)) {
	CRLF;
	show_msgattr(attr);
	menu("MSGATTR");
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
