#line 1 "STR.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*****************************************************/
/* Functions that perform lengthy string i/o rotines */
/*****************************************************/

#include "sbbs.h"

/****************************************************************************/
/* Lists all users who have access to the current sub.                      */
/****************************************************************************/
void userlist(char mode)
{
	uchar name[256],sort=0;
	int i,j,k,users=0;
	uchar *line[1000];
	user_t user;

if(lastuser()<=1000)
	sort=yesno(text[SortAlphaQ]);
if(sort) {
	bputs(text[CheckingSlots]); }
else {
	CRLF; }
j=0;
k=lastuser();
for(i=1;i<=k && !msgabort();i++) {
	if(sort && (online==ON_LOCAL || !rioctl(TXBC)))
		bprintf("%-4d\b\b\b\b",i);
	user.number=i;
	getuserdat(&user);
	if(user.misc&(DELETED|INACTIVE))
		continue;
	users++;
	if(mode==UL_SUB) {
		if(!usrgrps)
			continue;
		if(!chk_ar(grp[usrgrp[curgrp]]->ar,user))
			continue;
		if(!chk_ar(sub[usrsub[curgrp][cursub[curgrp]]]->ar,user)
			|| (sub[usrsub[curgrp][cursub[curgrp]]]->read_ar[0]
			&& !chk_ar(sub[usrsub[curgrp][cursub[curgrp]]]->read_ar,user)))
			continue; }
	else if(mode==UL_DIR) {
		if(user.rest&FLAG('T'))
			continue;
		if(!usrlibs)
			continue;
		if(!chk_ar(lib[usrlib[curlib]]->ar,user))
			continue;
		if(!chk_ar(dir[usrdir[curlib][curdir[curlib]]]->ar,user))
			continue; }
	if(sort) {
		if((line[j]=(char *)MALLOC(128))==0) {
			errormsg(WHERE,ERR_ALLOC,nulstr,83);
			for(i=0;i<j;i++)
				FREE(line[i]);
			return; }
		sprintf(name,"%s #%d",user.alias,i);
		sprintf(line[j],text[UserListFmt],name
			,sys_misc&SM_LISTLOC ? user.location : user.note
			,unixtodstr(user.laston,tmp)
			,user.modem); }
	else {
		sprintf(name,"%s #%u",user.alias,i);
		bprintf(text[UserListFmt],name
			,sys_misc&SM_LISTLOC ? user.location : user.note
			,unixtodstr(user.laston,tmp)
			,user.modem); }
	j++; }
if(i<=k) {	/* aborted */
	if(sort)
		for(i=0;i<j;i++)
			FREE(line[i]);
	return; }
if(!sort) {
	CRLF; }
bprintf(text[NTotalUsers],users);
if(mode==UL_SUB)
	bprintf(text[NUsersOnCurSub],j);
else if(mode==UL_DIR)
	bprintf(text[NUsersOnCurDir],j);
if(!sort)
	return;
CRLF;
qsort((void *)line,j,sizeof(line[0])
	,(int(*)(const void*, const void*))pstrcmp);
for(i=0;i<j && !msgabort();i++)
	bputs(line[i]);
for(i=0;i<j;i++)
	FREE(line[i]);
}

/****************************************************************************/
/* SIF input function. See SIF.DOC for more info        					*/
/****************************************************************************/
void sif(char *fname, char *answers, long len)
{
	char str[256],template[256],HUGE16 *buf,t,max,min,mode,cr;
	int file;
	ulong length,l=0,m,top,a=0;

sprintf(str,"%s%s.SIF",text_dir,strupr(fname));
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	answers[0]=0;
	return; }
length=filelength(file);
if((buf=(char *)MALLOC(length))==0) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,str,length);
	answers[0]=0;
	return; }
if(lread(file,buf,length)!=length) {
	close(file);
	errormsg(WHERE,ERR_READ,str,length);
	answers[0]=0;
	return; }
close(file);
while(l<length && online) {
	mode=min=max=t=cr=0;
	top=l;
	while(l<length && buf[l++]!=STX);
	for(m=l;m<length;m++)
		if(buf[m]==ETX || !buf[m]) {
			buf[m]=0;
			break; }
	if(l>=length) break;
	if(online==ON_REMOTE) {
		rioctl(IOCM|ABORT);
		rioctl(IOCS|ABORT); }
	putmsg(buf+l,P_SAVEATR);
	m++;
	if(toupper(buf[m])!='C' && toupper(buf[m])!='S')
        continue;
	SYNC;
	if(online==ON_REMOTE)
		rioctl(IOSM|ABORT);
	if(a>=len) {
		errormsg(WHERE,ERR_LEN,fname,len);
		break; }
	if((buf[m]&0xdf)=='C') {
    	if((buf[m+1]&0xdf)=='U') {		/* Uppercase only */
			mode|=K_UPPER;
			m++; }
		else if((buf[m+1]&0xdf)=='N') {	/* Numbers only */
			mode|=K_NUMBER;
			m++; }
		if((buf[m+1]&0xdf)=='L') {		/* Draw line */
        	if(useron.misc&COLOR)
				attr(color[clr_inputline]);
			else
				attr(BLACK|(LIGHTGRAY<<4));
			bputs(" \b");
			m++; }
		if((buf[m+1]&0xdf)=='R') {		/* Add CRLF */
			cr=1;
			m++; }
		if(buf[m+1]=='"') {
			m+=2;
			for(l=m;l<length;l++)
				if(buf[l]=='"') {
					buf[l]=0;
					break; }
			answers[a++]=getkeys((char *)buf+m,0); }
		else {
			answers[a]=getkey(mode);
			outchar(answers[a++]);
			attr(LIGHTGRAY);
			CRLF; }
		if(cr) {
			answers[a++]=CR;
			answers[a++]=LF; } }
	else if((buf[m]&0xdf)=='S') {		/* String */
		if((buf[m+1]&0xdf)=='U') {		/* Uppercase only */
			mode|=K_UPPER;
			m++; }
		else if((buf[m+1]&0xdf)=='F') { /* Force Upper/Lowr case */
			mode|=K_UPRLWR;
			m++; }
		else if((buf[m+1]&0xdf)=='N') {	/* Numbers only */
			mode|=K_NUMBER;
			m++; }
		if((buf[m+1]&0xdf)=='L') {		/* Draw line */
			mode|=K_LINE;
			m++; }
		if((buf[m+1]&0xdf)=='R') {		/* Add CRLF */
			cr=1;
			m++; }
		if(isdigit(buf[m+1])) {
			max=buf[++m]&0xf;
			if(isdigit(buf[m+1]))
				max=max*10+(buf[++m]&0xf); }
		if(buf[m+1]=='.' && isdigit(buf[m+2])) {
			m++;
			min=buf[++m]&0xf;
			if(isdigit(buf[m+1]))
				min=min*10+(buf[++m]&0xf); }
		if(buf[m+1]=='"') {
			m++;
			mode&=~K_NUMBER;
			while(buf[++m]!='"' && t<80)
				template[t++]=buf[m];
			template[t]=0;
			max=strlen(template); }
		if(t) {
			if(gettmplt(str,template,mode)<min) {
				l=top;
				continue; } }
		else {
			if(!max)
				continue;
			if(getstr(str,max,mode)<min) {
				l=top;
				continue; } }
		if(!cr) {
			for(cr=0;str[cr];cr++)
				answers[a+cr]=str[cr];
			while(cr<max)
				answers[a+cr++]=ETX;
			a+=max; }
		else {
			putrec(answers,a,max,str);
			putrec(answers,a+max,2,crlf);
			a+=max+2; } } }
answers[a]=0;
FREE((char *)buf);
}

/****************************************************************************/
/* SIF output function. See SIF.DOC for more info        					*/
/****************************************************************************/
void sof(char *fname, char *answers, long len)
{
	char str[256],HUGE16 *buf,max,min,cr;
	int file;
	ulong length,l=0,m,a=0;

sprintf(str,"%s%s.SIF",text_dir,strupr(fname));
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	answers[0]=0;
	return; }
length=filelength(file);
if((buf=(char *)MALLOC(length))==0) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,str,length);
	answers[0]=0;
	return; }
if(lread(file,buf,length)!=length) {
	close(file);
	errormsg(WHERE,ERR_READ,str,length);
	answers[0]=0;
	return; }
close(file);
while(l<length && online) {
	min=max=cr=0;
	while(l<length && buf[l++]!=STX);
	for(m=l;m<length;m++)
		if(buf[m]==ETX || !buf[m]) {
			buf[m]=0;
			break; }
	if(l>=length) break;
	if(online==ON_REMOTE) {
		rioctl(IOCM|ABORT);
		rioctl(IOCS|ABORT); }
	putmsg(buf+l,P_SAVEATR);
	m++;
	if(toupper(buf[m])!='C' && toupper(buf[m])!='S')
		continue;
	SYNC;
	if(online==ON_REMOTE)
		rioctl(IOSM|ABORT);
	if(a>=len) {
		bprintf("\r\nSOF: %s defined more data than buffer size "
			"(%lu bytes)\r\n",fname,len);
		break; }
	if((buf[m]&0xdf)=='C') {
		if((buf[m+1]&0xdf)=='U')  		/* Uppercase only */
			m++;
		else if((buf[m+1]&0xdf)=='N')  	/* Numbers only */
			m++;
		if((buf[m+1]&0xdf)=='L') {		/* Draw line */
        	if(useron.misc&COLOR)
				attr(color[clr_inputline]);
			else
				attr(BLACK|(LIGHTGRAY<<4));
			bputs(" \b");
			m++; }
		if((buf[m+1]&0xdf)=='R') {		/* Add CRLF */
			cr=1;
			m++; }
		outchar(answers[a++]);
		attr(LIGHTGRAY);
		CRLF;
		if(cr)
			a+=2; }
	else if((buf[m]&0xdf)=='S') {		/* String */
		if((buf[m+1]&0xdf)=='U')
			m++;
		else if((buf[m+1]&0xdf)=='F')
			m++;
		else if((buf[m+1]&0xdf)=='N')   /* Numbers only */
			m++;
		if((buf[m+1]&0xdf)=='L') {
        	if(useron.misc&COLOR)
				attr(color[clr_inputline]);
			else
				attr(BLACK|(LIGHTGRAY<<4));
			m++; }
		if((buf[m+1]&0xdf)=='R') {
			cr=1;
			m++; }
		if(isdigit(buf[m+1])) {
			max=buf[++m]&0xf;
			if(isdigit(buf[m+1]))
				max=max*10+(buf[++m]&0xf); }
		if(buf[m+1]=='.' && isdigit(buf[m+2])) {
			m++;
			min=buf[++m]&0xf;
			if(isdigit(buf[m+1]))
				min=min*10+(buf[++m]&0xf); }
		if(buf[m+1]=='"') {
			max=0;
			m++;
			while(buf[++m]!='"' && max<80)
				max++; }
		if(!max)
			continue;
		getrec(answers,a,max,str);
		bputs(str);
		attr(LIGHTGRAY);
		CRLF;
		if(!cr)
			a+=max;
		else
			a+=max+2; } }
FREE((char *)buf);
}

/****************************************************************************/
/* Creates data file 'datfile' from input via sif file 'siffile'            */
/****************************************************************************/
void create_sif_dat(char *siffile, char *datfile)
{
	char *buf;
	int file;

if((buf=(char *)MALLOC(SIF_MAXBUF))==NULL) {
	errormsg(WHERE,ERR_ALLOC,siffile,SIF_MAXBUF);
	return; }
memset(buf,SIF_MAXBUF,0);	 /* initialize to null */
sif(siffile,buf,SIF_MAXBUF);
if((file=nopen(datfile,O_WRONLY|O_TRUNC|O_CREAT))==-1) {
	FREE(buf);
	errormsg(WHERE,ERR_OPEN,datfile,O_WRONLY|O_TRUNC|O_CREAT);
	return; }
write(file,buf,strlen(buf));
close(file);
FREE(buf);
}

/****************************************************************************/
/* Reads data file 'datfile' and displays output via sif file 'siffile'     */
/****************************************************************************/
void read_sif_dat(char *siffile, char *datfile)
{
	char *buf;
	int file;
	long length;

if((file=nopen(datfile,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,datfile,O_RDONLY);
	return; }
length=filelength(file);
if(!length) {
	close(file);
	return; }
if((buf=(char *)MALLOC(length))==NULL) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,datfile,length);
	return; }
read(file,buf,length);
close(file);
sof(siffile,buf,length);
FREE(buf);
}

/****************************************************************************/
/* Get string by template. A=Alpha, N=Number, !=Anything                    */
/* First character MUST be an A,N or !.                                     */
/* Modes - K_LINE and K_UPPER are supported.                                */
/****************************************************************************/
char gettmplt(char *strout,char *template,int mode)
{
	uchar t=strlen(template),c=0,ch,str[256];

sys_status&=~SS_ABORT;
strupr(template);
if(useron.misc&ANSI) {
	if(mode&K_LINE) {
		if(useron.misc&COLOR)
            attr(color[clr_inputline]);
		else
			attr(BLACK|(LIGHTGRAY<<4)); }
	while(c<t) {
		if(template[c]=='N' || template[c]=='A' || template[c]=='!')
			outchar(SP);
		else
			outchar(template[c]);
		c++; }
	bprintf("\x1b[%dD",t); }
c=0;
if(mode&K_EDIT) {
	strcpy(str,strout);
	bputs(str);
	c=strlen(str); }
while((ch=getkey(mode))!=CR && online && !(sys_status&SS_ABORT)) {
	if(ch==BS) {
		if(!c)
			continue;
		for(ch=1,c--;c;c--,ch++)
			if(template[c]=='N' || template[c]=='A' || template[c]=='!')
				break;
		if(useron.misc&ANSI)
			bprintf("\x1b[%dD",ch);
		else while(ch--)
			outchar(BS);
		bputs(" \b");
		continue; }
	if(ch==24) {	/* Ctrl-X */
		for(c--;c!=0xff;c--) {
			outchar(BS);
			if(template[c]=='N' || template[c]=='A' || template[c]=='!')
				bputs(" \b"); }
		c=0; }
	else if(c<t) {
		if(template[c]=='N' && !isdigit(ch))
			continue;
		if(template[c]=='A' && !isalpha(ch))
			continue;
		outchar(ch);
		str[c++]=ch;
		while(c<t && template[c]!='N' && template[c]!='A' && template[c]!='!'){
			str[c]=template[c];
			outchar(template[c++]); } } }
str[c]=0;
attr(LIGHTGRAY);
CRLF;
if(!(sys_status&SS_ABORT))
	strcpy(strout,str);
return(c);
}

/*****************************************************************************/
/* Accepts a user's input to change a new-scan time pointer                  */
/* Returns 0 if input was aborted or invalid, 1 if complete					 */
/*****************************************************************************/
char inputnstime(time_t *dt)
{
	int hour;
	struct date tmpdate;
	struct time tmptime;
	char pm,str[256];

bputs(text[NScanDate]);
bputs(timestr(dt));
CRLF;
unixtodos(*dt,&tmpdate,&tmptime);
bputs(text[NScanYear]);
itoa(tmpdate.da_year,str,10);
if(!getstr(str,4,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
	CRLF;
	return(0); }
tmpdate.da_year=atoi(str);
if(tmpdate.da_year<1970) {
	CRLF;
	return(0); }
bputs(text[NScanMonth]);
itoa(tmpdate.da_mon,str,10);
if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
	CRLF;
	return(0); }
tmpdate.da_mon=atoi(str);
if(tmpdate.da_mon<1 || tmpdate.da_mon>12) {
	CRLF;
	return(0); }
bputs(text[NScanDay]);
itoa(tmpdate.da_day,str,10);
if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
	CRLF;
	return(0); }
tmpdate.da_day=atoi(str);
if(tmpdate.da_day<1 || tmpdate.da_day>31) {
	CRLF;
	return(0); }
bputs(text[NScanHour]);
if(sys_misc&SM_MILITARY)
	hour=tmptime.ti_hour;
else {
	if(tmptime.ti_hour==0) {	/* 12 midnite */
		pm=0;
		hour=12; }
	else if(tmptime.ti_hour>12) {
		hour=tmptime.ti_hour-12;
		pm=1; }
	else {
		hour=tmptime.ti_hour;
		pm=0; } }
itoa(hour,str,10);
if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
	CRLF;
	return(0); }

tmptime.ti_hour=atoi(str);
if(tmptime.ti_hour>24) {
	CRLF;
	return(0); }
bputs(text[NScanMinute]);
itoa(tmptime.ti_min,str,10);
if(!getstr(str,2,K_EDIT|K_AUTODEL|K_NUMBER|K_NOCRLF) || sys_status&SS_ABORT) {
	CRLF;
	return(0); }

tmptime.ti_min=atoi(str);
if(tmptime.ti_min>59) {
	CRLF;
	return(0); }
tmptime.ti_sec=0;
if(!(sys_misc&SM_MILITARY) && tmptime.ti_hour && tmptime.ti_hour<13) {
	if(pm && yesno(text[NScanPmQ])) {
			if(tmptime.ti_hour<12)
				tmptime.ti_hour+=12; }
	else if(!pm && !yesno(text[NScanAmQ])) {
			if(tmptime.ti_hour<12)
				tmptime.ti_hour+=12; }
	else if(tmptime.ti_hour==12)
		tmptime.ti_hour=0; }
else {
	CRLF; }
*dt=dostounix(&tmpdate,&tmptime);
return(1);
}

/*****************************************************************************/
/* Checks a password for uniqueness and validity                              */
/*****************************************************************************/
char chkpass(char *pass, user_t user)
{
	char c,d,first[128],last[128],sysop[41],sysname[41],*p;

if(strlen(pass)<4) {
	bputs(text[PasswordTooShort]);
	return(0); }
if(!strcmp(pass,user.pass)) {
	bputs(text[PasswordNotChanged]);
	return(0); }
d=strlen(pass);
for(c=1;c<d;c++)
	if(pass[c]!=pass[c-1])
		break;
if(c==d) {
	bputs(text[PasswordInvalid]);
	return(0); }
for(c=0;c<3;c++)	/* check for 1234 and ABCD */
	if(pass[c]!=pass[c+1]+1)
		break;
if(c==3) {
	bputs(text[PasswordObvious]);
	return(0); }
for(c=0;c<3;c++)	/* check for 4321 and ZYXW */
	if(pass[c]!=pass[c+1]-1)
		break;
if(c==3) {
	bputs(text[PasswordObvious]);
    return(0); }
strupr(user.name);
strupr(user.alias);
strcpy(first,user.alias);
p=strchr(first,SP);
if(p) {
	*p=0;
	strcpy(last,p+1); }
else
	last[0]=0;
strupr(user.handle);
strcpy(sysop,sys_op);
strupr(sysop);
strcpy(sysname,sys_name);
strupr(sysname);
if((user.pass[0]
		&& (strstr(pass,user.pass) || strstr(user.pass,pass)))
	|| (user.name[0]
		&& (strstr(pass,user.name) || strstr(user.name,pass)))
	|| strstr(pass,user.alias) || strstr(user.alias,pass)
	|| strstr(pass,first) || strstr(first,pass)
	|| (last[0]
		&& (strstr(pass,last) || strstr(last,pass)))
	|| strstr(pass,user.handle) || strstr(user.handle,pass)
	|| (user.zipcode[0]
		&& (strstr(pass,user.zipcode) || strstr(user.zipcode,pass)))
	|| (sysname[0]
		&& (strstr(pass,sysname) || strstr(sysname,pass)))
	|| (sysop[0]
		&& (strstr(pass,sysop) || strstr(sysop,pass)))
	|| (sys_id[0]
		&& (strstr(pass,sys_id) || strstr(sys_id,pass)))
	|| (node_phone[0] && strstr(pass,node_phone))
	|| (user.phone[0] && strstr(user.phone,pass))
	|| !strncmp(pass,"QWER",3)
	|| !strncmp(pass,"ASDF",3)
	|| !strncmp(pass,"!@#$",3)
	)
	{
	bputs(text[PasswordObvious]);
	return(0); }
return(1);
}

/****************************************************************************/
/* Prompts user for detailed information regarding their computer 			*/
/* and places that information into 'computer'								*/
/* Called from function newuser												*/
/****************************************************************************/
void getcomputer(char *computer)
{
	char str[256];

if(!(uq&UQ_MC_COMP)) {
	while(online) {
		bputs(text[EnterYourComputer]);
		if(getstr(computer,LEN_COMP,K_LINE))
			break; }
	return; }
bputs(text[ComputerTypeMenu]);
bputs(text[ComputerTypePrompt]);
switch(getkeys("ABCDE",0)) {
	case 'A':
		sif("COMPUTER",str,8);
		if(!online) return;
		switch(str[0]) {
			case 'A':
				strcpy(computer,"XT");
				break;
			case 'B':
				strcpy(computer,"286");
				break;
			case 'C':
				strcpy(computer,"386SX");
				break;
			case 'D':
				strcpy(computer,"386DX");
				break;
			case 'E':
				strcpy(computer,"486");
				break; }
		switch(str[1]) {
			case 'A':
				strcat(computer,"-4 ");
				break;
			case 'B':
				strcat(computer,"-6 ");
				break;
			case 'C':
				strcat(computer,"-8 ");
				break;
			case 'D':
				strcat(computer,"-10 ");
				break;
			case 'E':
				strcat(computer,"-12 ");
				break;
			case 'F':
				strcat(computer,"-16 ");
				break;
			case 'G':
				strcat(computer,"-20 ");
				break;
			case 'H':
				strcat(computer,"-25 ");
				break;
			case 'I':
				strcat(computer,"-33 ");
				break;
			case 'J':
				strcat(computer,"-40 ");
				break;
			case 'K':
				strcat(computer,"-50 ");
				break; }
		switch(str[2]) {
			case 'A':
				strcat(computer,"8bit ");
				break;
			case 'B':
				strcat(computer,"ISA ");
				break;
			case 'C':
				strcat(computer,"MCA ");
				break;
			case 'D':
				strcat(computer,"EISA ");
				break; }
		switch(str[3]) {
			case 'A':
				strcat(computer,"MDA ");
				break;
			case 'B':
				strcat(computer,"HERC ");
				break;
			case 'C':
				strcat(computer,"CGA ");
				break;
			case 'D':
				strcat(computer,"EGA ");
				break;
			case 'E':
				strcat(computer,"MCGA ");
				break;
			case 'F':
				strcat(computer,"VGA ");
				break;
			case 'G':
				strcat(computer,"SVGA ");
				break;
			case 'H':
				strcat(computer,"MVGA ");
				break;
			case 'I':
				strcat(computer,"8514 ");
				break;
			case 'J':
				strcat(computer,"XGA ");
				break;
			case 'K':
				strcat(computer,"TIGA ");
				break; }
		switch(str[4]) {
			case 'A':
				strcat(computer,"<1 ");
				break;
			case 'B':
				strcat(computer,"1 ");
				break;
			case 'C':
				strcat(computer,"2 ");
				break;
			case 'D':
				strcat(computer,"3 ");
				break;
			case 'E':
				strcat(computer,"4 ");
				break;
			case 'F':
				strcat(computer,"5 ");
				break;
			case 'G':
				strcat(computer,"6 ");
				break;
			case 'H':
				strcat(computer,"8 ");
				break;
			case 'I':
				strcat(computer,"10 ");
				break;
			case 'J':
				strcat(computer,"12 ");
				break;
			case 'K':
				strcat(computer,"16 ");
				break;
			case 'L':
				strcat(computer,"18 ");
				break;
			case 'M':
				strcat(computer,"24 ");
				break;
			case 'N':
				strcat(computer,"32 ");
				break;
			case 'O':
				strcat(computer,"64 ");
				break; }
		switch(str[5]) {
			case 'A':
				strcat(computer,"0 ");
				break;
			case 'B':
				strcat(computer,"10 ");
				break;
			case 'C':
				strcat(computer,"20 ");
				break;
			case 'D':
				strcat(computer,"30 ");
				break;
			case 'E':
				strcat(computer,"40 ");
				break;
			case 'F':
				strcat(computer,"60 ");
				break;
			case 'G':
				strcat(computer,"80 ");
				break;
			case 'H':
				strcat(computer,"100 ");
				break;
			case 'I':
				strcat(computer,"120 ");
				break;
			case 'J':
				strcat(computer,"150 ");
				break;
			case 'K':
				strcat(computer,"200 ");
				break;
			case 'L':
				strcat(computer,"250 ");
				break;
			case 'M':
				strcat(computer,"300 ");
				break;
			case 'N':
				strcat(computer,"400 ");
				break;
			case 'O':
				strcat(computer,"500 ");
				break;
			case 'P':
				strcat(computer,"600 ");
				break;
			case 'Q':
				strcat(computer,"700 ");
				break;
			case 'R':
				strcat(computer,"800 ");
				break;
			case 'S':
				strcat(computer,"900 ");
				break;
			case 'T':
				strcat(computer,"1GB ");
				break; }
		switch(str[6]) {
			case 'A':
				strcat(computer,"ST506");
				break;
			case 'B':
				strcat(computer,"SCSI");
				break;
			case 'C':
				strcat(computer,"SCSI2");
				break;
			case 'D':
				strcat(computer,"ESDI");
				break;
			case 'E':
				strcat(computer,"IDE"); }
		break;
	case 'B':
		sprintf(computer,"%.*s",LEN_COMP,text[ComputerTypeB]);
		return;
	case 'C':
		sprintf(computer,"%.*s",LEN_COMP,text[ComputerTypeC]);
		return;
	case 'D':
		sprintf(computer,"%.*s",LEN_COMP,text[ComputerTypeD]);
		return;
	case 'E':
		sprintf(computer,"%.*s",LEN_COMP,text[ComputerTypeE]);
		return; }
}

/****************************************************************************/
/* Displays information about sub-board subnum								*/
/****************************************************************************/
void subinfo(uint subnum)
{
	char str[256];

bputs(text[SubInfoHdr]);
bprintf(text[SubInfoLongName],sub[subnum]->lname);
bprintf(text[SubInfoShortName],sub[subnum]->sname);
bprintf(text[SubInfoQWKName],sub[subnum]->qwkname);
bprintf(text[SubInfoMaxMsgs],sub[subnum]->maxmsgs);
if(sub[subnum]->misc&SUB_QNET)
	bprintf(text[SubInfoTagLine],sub[subnum]->tagline);
if(sub[subnum]->misc&SUB_FIDO)
	bprintf(text[SubInfoFidoNet]
		,sub[subnum]->origline
		,faddrtoa(sub[subnum]->faddr));
sprintf(str,"%s%s.MSG",sub[subnum]->data_dir,sub[subnum]->code);
if(fexist(str) && yesno(text[SubInfoViewFileQ]))
	printfile(str,0);
}

/****************************************************************************/
/* Displays information about transfer directory dirnum 					*/
/****************************************************************************/
void dirinfo(uint dirnum)
{
	char str[256];

bputs(text[DirInfoHdr]);
bprintf(text[DirInfoLongName],dir[dirnum]->lname);
bprintf(text[DirInfoShortName],dir[dirnum]->sname);
if(dir[dirnum]->exts[0])
	bprintf(text[DirInfoAllowedExts],dir[dirnum]->exts);
bprintf(text[DirInfoMaxFiles],dir[dirnum]->maxfiles);
sprintf(str,"%s%s.MSG",dir[dirnum]->data_dir,dir[dirnum]->code);
if(fexist(str) && yesno(text[DirInfoViewFileQ]))
	printfile(str,0);
}

/****************************************************************************/
/* Searches the file <name>.CAN in the TEXT directory for matches			*/
/* Returns 1 if found in list, 0 if not.									*/
/****************************************************************************/
char trashcan(char *insearch, char *name)
{
	char str[256],search[256],c,found=0;
	int file;
	FILE *stream;

sprintf(str,"%s%s.CAN",text_dir,name);
if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
	if(fexist(str))
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return(0); }
strcpy(search,insearch);
strupr(search);
while(!feof(stream) && !ferror(stream) && !found) {
	if(!fgets(str,81,stream))
		break;
	truncsp(str);
	c=strlen(str);
	if(c) {
		c--;
		strupr(str);
		if(str[c]=='~') {
			str[c]=0;
			if(strstr(search,str))
				found=1; }

		else if(str[c]=='^') {
			str[c]=0;
			if(!strncmp(str,search,c))
				found=1; }

		else if(!strcmp(str,search))
			found=1; } }
fclose(stream);
if(found) {
	sprintf(str,"%sBAD%s.MSG",text_dir,name);
	if(fexist(str))
		printfile(str,0);
	return(1); }
return(0);
}

/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG and NODE.LOG         */
/****************************************************************************/
void errormsg(int line, char *source, char action, char *object, ulong access)
{
    char str[512];
    char actstr[256];

switch(action) {
    case ERR_OPEN:
        strcpy(actstr,"opening");
        break;
    case ERR_CLOSE:
		strcpy(actstr,"closing");
        break;
    case ERR_FDOPEN:
        strcpy(actstr,"fdopen");
        break;
    case ERR_READ:
        strcpy(actstr,"reading");
        break;
    case ERR_WRITE:
        strcpy(actstr,"writing");
        break;
    case ERR_REMOVE:
        strcpy(actstr,"removing");
        break;
    case ERR_ALLOC:
        strcpy(actstr,"allocating memory");
        break;
    case ERR_CHK:
        strcpy(actstr,"checking");
        break;
    case ERR_LEN:
        strcpy(actstr,"checking length");
        break;
    case ERR_EXEC:
        strcpy(actstr,"executing");
        break;
    case ERR_CHDIR:
        strcpy(actstr,"changing directory");
        break;
	case ERR_CREATE:
		strcpy(actstr,"creating");
		break;
	case ERR_LOCK:
		strcpy(actstr,"locking");
		break;
	case ERR_UNLOCK:
		strcpy(actstr,"unlocking");
        break;
    default:
        strcpy(actstr,"UNKNOWN"); }
bprintf("\7\r\nERROR -   action: %s",actstr);   /* tell user about error */
bprintf("\7\r\n          object: %s",object);
bprintf("\7\r\n          access: %ld",access);
if(access>9 && (long)access!=-1 && (short)access!=-1 && (char)access!=-1)
	bprintf(" (%lXh)",access);
if(sys_misc&SM_ERRALARM) {
	beep(500,220); beep(250,220);
	beep(500,220); beep(250,220);
	beep(500,220); beep(250,220);
	nosound(); }
bputs("\r\n\r\nThe sysop has been notified. <Hit a key>");
getkey(0);
CRLF;
sprintf(str,"\r\n      file: %s\r\n      line: %d\r\n    action: %s\r\n"
	"    object: %s\r\n    access: %ld"
	,source,line,actstr,object,access);
if(access>9 && (long)access!=-1 && (short)access!=-1 && (char)access!=-1) {
	sprintf(tmp," (%lXh)",access);
	strcat(str,tmp); }
if(errno) {
	sprintf(tmp,"\r\n     errno: %d",errno);
    strcat(str,tmp); }
if(_doserrno && _doserrno!=errno) {
	sprintf(tmp,"\r\n  doserrno: %d",_doserrno);
    strcat(str,tmp); }
errno=_doserrno=0;
errorlog(str);
}

/*****************************************************************************/
/* Error logging to NODE.LOG and DATA\ERROR.LOG function                     */
/*****************************************************************************/
void errorlog(char *text)
{
	static char inside;
    char hdr[256],str[256],tmp2[256];
    int file;

if(inside)		/* let's not go recursive on this puppy */
	return;
inside=1;
getnodedat(node_num,&thisnode,1);
criterrs=++thisnode.errors;
putnodedat(node_num,thisnode);
now=time(NULL);
logline("!!",text);
sprintf(str,"%sERROR.LOG",data_dir);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
    sprintf(tmp2,"ERROR opening/creating %s",str);
    logline("!!",tmp2);
	inside=0;
    return; }
sprintf(hdr,"%s Node %2d: %s #%d"
    ,timestr(&now),node_num,useron.alias,useron.number);
write(file,hdr,strlen(hdr));
write(file,crlf,2);
write(file,text,strlen(text));
write(file,"\r\n\r\n",4);
close(file);
inside=0;
}

