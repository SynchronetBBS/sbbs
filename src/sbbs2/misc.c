#line 1 "MISC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/***************************************************************************/
/* Miscellaneous functions that are useful many places throughout the code */
/***************************************************************************/

#include "sbbs.h"
#include "crc32.h"

#ifdef __WIN32__
#include <windows.h>	// Required for kbd_state(), beep(), and mswait()
#endif

/****************************************************************************/
/* Returns the number of characters in 'str' not counting ctrl-ax codes		*/
/* or the null terminator													*/
/****************************************************************************/
int bstrlen(char *str)
{
	int i=0;

while(*str) {
	if(*str==1) /* ctrl-a */
		str++;
	else
		i++;
	if(!(*str)) break;
	str++; }
return(i);
}

void strip_ctrl(char *str)
{
	char tmp[1024];
	int i,j,k;

k=strlen(str);
for(i=j=0;i<k;i++)
	if(str[i]==1)  /* Ctrl-a */
        i++;
	else if(j && str[i]<=SP && tmp[j-1]==SP)
		continue;
	else if(i && !isalnum(str[i]) && str[i]==str[i-1])
		continue;
	else if((uchar)str[i]>=SP)
		tmp[j++]=str[i];
	else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
		tmp[j++]=SP;
tmp[j]=0;
strcpy(str,tmp);
}

void strip_exascii(char *str)
{
	char tmp[1024];
	int i,j,k;

k=strlen(str);
for(i=j=0;i<k;i++)
	if(!(str[i]&0x80))
		tmp[j++]=str[i];
tmp[j]=0;
strcpy(str,tmp);
}

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas.																	*/
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
	char str[256];
	char i,j,k;

ultoa(l,str,10);
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
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

if(access&O_DENYNONE) {
	share=SH_DENYNO;
	access&=~O_DENYNONE; }
else if(access==O_RDONLY) share=SH_DENYWR;
else share=SH_DENYRW;
while(((file=sopen(str,O_BINARY|access,share,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN)
	if(count>10)
		mswait(55);
if(count>(LOOP_NOPEN/2) && count<=LOOP_NOPEN) {
	sprintf(logstr,"NOPEN COLLISION - File: %s Count: %d"
		,str,count);
	logline("!!",logstr); }
if(file==-1 && errno==EACCES)
	bputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE *fnopen(int *file, char *str, int access)
{
	char mode[128];
	FILE *stream;

if(((*file)=nopen(str,access))==-1)
	return(NULL);

if(access&O_APPEND) {
	if(access&O_RDONLY)
		strcpy(mode,"a+");
	else
		strcpy(mode,"a"); }
else {
	if(access&O_WRONLY)
		strcpy(mode,"r+");
	else
		strcpy(mode,"r"); }
stream=fdopen((*file),mode);
if(stream==NULL) {
	close(*file);
	errormsg(WHERE,ERR_FDOPEN,str,access);
	return(NULL); }
setvbuf(stream,NULL,_IOFBF,2*1024);
return(stream);
}

#ifndef __FLAT__
/****************************************************************************/
/* This function reads files that are potentially larger than 32k.  		*/
/* Up to one megabyte of data can be read with each call.                   */
/****************************************************************************/
long lread(int file, char huge *buf,long bytes)
{
	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(read(file,(char *)buf,32767)!=32767)
		return(-1L);
if(read(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}

long lfread(char huge *buf, long bytes, FILE *fp)
{
	long count;

for(count=bytes;count>0x7fff;count-=0x7fff,buf+=0x7fff)
	if(fread((char *)buf,1,0x7fff,fp)!=0x7fff)
		return(0);
if(fread((char *)buf,1,(int)count,fp)!=count)
	return(0);
return(bytes);
}

/****************************************************************************/
/* This function writes files that are potentially larger than 32767 bytes  */
/* Up to one megabytes of data can be written with each call.				*/
/****************************************************************************/
long lwrite(int file, char huge *buf, long bytes)
{

	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(write(file,(char *)buf,32767)!=32767)
		return(-1L);
if(write(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}
#endif

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	uint c;

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
if(i && str[i-1]!='\\') {
    str[i]='\\'; str[i+1]=0; }
}

/****************************************************************************/
/* Puts a backslash on path strings if not just a drive letter and colon	*/
/****************************************************************************/
void backslashcolon(char *str)
{
    int i;

i=strlen(str);
if(i && str[i-1]!='\\' && str[i-1]!=':') {
    str[i]='\\'; str[i+1]=0; }
}

/****************************************************************************/
/* Updates 16-bit "rcrc" with character 'ch'                                */
/****************************************************************************/
void ucrc16(uchar ch, ushort *rcrc) {
	ushort i, cy;
    uchar nch=ch;
 
for (i=0; i<8; i++) {
    cy=*rcrc & 0x8000;
    *rcrc<<=1;
    if (nch & 0x80) *rcrc |= 1;
    nch<<=1;
    if (cy) *rcrc ^= 0x1021; }
}

/****************************************************************************/
/* Returns CRC-16 of string (not including terminating NULL)				*/
/****************************************************************************/
ushort crc16(char *str)
{
	int 	i=0;
	ushort	crc=0;

ucrc16(0,&crc);
while(str[i])
	ucrc16(str[i++],&crc);
ucrc16(0,&crc);
ucrc16(0,&crc);
return(crc);
}

/****************************************************************************/
/* Returns CRC-32 of string (not including terminating NULL)				*/
/****************************************************************************/
ulong crc32(char *buf, ulong len)
{
	ulong l,crc=0xffffffff;

for(l=0;l<len;l++)
	crc=ucrc32(buf[l],crc);
return(~crc);
}

/****************************************************************************/
/* Compares pointers to pointers to char. Used in conjuction with qsort()   */
/****************************************************************************/
int pstrcmp(char **str1, char **str2)
{
return(strcmp(*str1,*str2));
}

/****************************************************************************/
/* Returns the number of characters that are the same between str1 and str2 */
/****************************************************************************/
int strsame(char *str1, char *str2)
{
	int i,j=0;

for(i=0;i<strlen(str1);i++)
	if(str1[i]==str2[i]) j++;
return(j);
}

#define MV_BUFLEN	4096

/****************************************************************************/
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int mv(char *src, char *dest, char copy)
{
	char str[256],*buf,atr=curatr;
	int  ind,outd;
	long length,chunk=MV_BUFLEN,l;
	struct ftime ftime;
	FILE *inp,*outp;

if(!stricmp(src,dest))	 /* source and destination are the same! */
	return(0);
if(!fexist(src)) {
	bprintf("\r\n\7MV ERROR: Source doesn't exist\r\n'%s'\r\n"
		,src);
	return(-1); }
if(!copy && fexist(dest)) {
	bprintf("\r\n\7MV ERROR: Destination already exists\r\n'%s'\r\n"
		,dest);
	return(-1); }
if(!copy && ((src[1]!=':' && dest[1]!=':')
	|| (src[1]==':' && dest[1]==':' && toupper(src[0])==toupper(dest[0])))) {
	if(rename(src,dest)) {						/* same drive, so move */
		bprintf("\r\nMV ERROR: Error renaming '%s'"
				"\r\n                      to '%s'\r\n\7",src,dest);
		return(-1); }
	return(0); }
attr(WHITE);
if((ind=nopen(src,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,src,O_RDONLY);
	return(-1); }
if((inp=fdopen(ind,"rb"))==NULL) {
	close(ind);
	errormsg(WHERE,ERR_FDOPEN,str,O_RDONLY);
	return(-1); }
setvbuf(inp,NULL,_IOFBF,32*1024);
if((outd=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
	fclose(inp);
	errormsg(WHERE,ERR_OPEN,dest,O_WRONLY|O_CREAT|O_TRUNC);
	return(-1); }
if((outp=fdopen(outd,"wb"))==NULL) {
	close(outd);
	fclose(inp);
	errormsg(WHERE,ERR_FDOPEN,dest,O_WRONLY|O_CREAT|O_TRUNC);
	return(-1); }
setvbuf(outp,NULL,_IOFBF,8*1024);
length=filelength(ind);
if(!length) {
	fclose(inp);
	fclose(outp);
	errormsg(WHERE,ERR_LEN,src,0);
	return(-1); }
if((buf=(char *)MALLOC(MV_BUFLEN))==NULL) {
	fclose(inp);
	fclose(outp);
	errormsg(WHERE,ERR_ALLOC,nulstr,MV_BUFLEN);
	return(-1); }
l=0L;
while(l<length) {
	bprintf("%2lu%%",l ? (long)(100.0/((float)length/l)) : 0L);
	if(l+chunk>length)
		chunk=length-l;
	if(fread(buf,1,chunk,inp)!=chunk) {
		FREE(buf);
		fclose(inp);
		fclose(outp);
		errormsg(WHERE,ERR_READ,src,chunk);
		return(-1); }
	if(fwrite(buf,1,chunk,outp)!=chunk) {
		FREE(buf);
		fclose(inp);
		fclose(outp);
		errormsg(WHERE,ERR_WRITE,dest,chunk);
		return(-1); }
	l+=chunk;
	bputs("\b\b\b"); }
bputs("   \b\b\b");  /* erase it */
attr(atr);
getftime(ind,&ftime);
setftime(outd,&ftime);
FREE(buf);
fclose(inp);
fclose(outp);
if(!copy && remove(src)) {
	errormsg(WHERE,ERR_REMOVE,src,0);
	return(-1); }
return(0);
}

/****************************************************************************/
/* Prompts user for System Password. Returns 1 if user entered correct PW	*/
/****************************************************************************/
char chksyspass(int local)
{
	static	int inside;
	char	str[256],str2[256],x,y,atr;
	int 	orgcon=console;

if(inside) return(0);
if(online==ON_REMOTE && !(sys_misc&SM_R_SYSOP))
	return(0);
if(online==ON_LOCAL) {
	if(!(sys_misc&SM_L_SYSOP))
		return(0);
	if(!(node_misc&NM_SYSPW) && !(sys_misc&SM_REQ_PW))
		return(1); }
if(local) {
	x=lclwx();
	y=lclwy();
	atr=lclatr(LIGHTGRAY<<4);
	STATUSLINE;
	lclxy(1,node_scrnlen);
	lputc(CLREOL);
	lputs("  System Password: "); }
else
	bputs("SY: ");
console&=~(CON_R_ECHO|CON_L_ECHO);
inside=1;
getstr(str,40,K_UPPER);
if(local) {
	TEXTWINDOW;
	lclatr(atr);
	lclxy(x,y);
	statusline(); }
inside=0;
console=orgcon;
if(!local)
	CRLF;
if(strcmp(sys_pass,str)) {
	sprintf(str2,"%s #%u System password attempt: '%s'"
		,useron.alias,useron.number,str);
	logline("S!",str2);
	return(0); }
return(1);
}

/****************************************************************************/
/* Converts when_t.zone into ASCII format                                   */
/****************************************************************************/
char *zonestr(short zone)
{
    static char str[32];

switch((ushort)zone) {
    case 0:     return("UT");
    case AST:   return("AST");
    case EST:   return("EST");
    case CST:   return("CST");
    case MST:   return("MST");
    case PST:   return("PST");
    case YST:   return("YST");
    case HST:   return("HST");
    case BST:   return("BST");
    case ADT:   return("ADT");
    case EDT:   return("EDT");
    case CDT:   return("CDT");
    case MDT:   return("MDT");
    case PDT:   return("PDT");
    case YDT:   return("YDT");
    case HDT:   return("HDT");
    case BDT:   return("BDT");
    case MID:   return("MID");
    case VAN:   return("VAN");
    case EDM:   return("EDM");
    case WIN:   return("WIN");
    case BOG:   return("BOG");
    case CAR:   return("CAR");
    case RIO:   return("RIO");
    case FER:   return("FER");
    case AZO:   return("AZO");
    case LON:   return("LON");
    case BER:   return("BER");
    case ATH:   return("ATH");
    case MOS:   return("MOS");
    case DUB:   return("DUB");
    case KAB:   return("KAB");
    case KAR:   return("KAR");
    case BOM:   return("BOM");
    case KAT:   return("KAT");
    case DHA:   return("DHA");
    case BAN:   return("BAN");
    case HON:   return("HON");
    case TOK:   return("TOK");
    case SYD:   return("SYD");
    case NOU:   return("NOU");
    case WEL:   return("WEL");
    }

sprintf(str,"%02d:%02u",zone/60,zone<0 ? (-zone)%60 : zone%60);
return(str);
}

/****************************************************************************/
/* Waits so many seconds. Call with 2 or greater.							*/
/****************************************************************************/
void secwait(int sec)
{
	time_t start;

start=time(NULL);
while(time(NULL)-start<sec)
	mswait(1);
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{

if(!strncmp(str,"00/00/00",8))
	return(0);
curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
if(str[6]<'7')
	date.da_year=2000+((str[6]&0xf)*10)+(str[7]&0xf);
else
	date.da_year=1900+((str[6]&0xf)*10)+(str[7]&0xf);
if(sys_misc&SM_EURODATE) {
	date.da_mon=((str[3]&0xf)*10)+(str[4]&0xf);
	date.da_day=((str[0]&0xf)*10)+(str[1]&0xf); }
else {
	date.da_mon=((str[0]&0xf)*10)+(str[1]&0xf);
	date.da_day=((str[3]&0xf)*10)+(str[4]&0xf); }
return(dostounix(&date,&curtime));
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char *unixtodstr(time_t unix, char *str)
{

if(!unix)
	strcpy(str,"00/00/00");
else {
	unixtodos(unix,&date,&curtime);
	if((unsigned)date.da_mon>12) {	  /* DOS leap year bug */
		date.da_mon=1;
		date.da_year++; }
	if((unsigned)date.da_day>31)
		date.da_day=1;
	if(sys_misc&SM_EURODATE)
		sprintf(str,"%02u/%02u/%02u",date.da_day,date.da_mon
			,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900);
	else
		sprintf(str,"%02u/%02u/%02u",date.da_mon,date.da_day
			,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900); }
return(str);
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
char fexist(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0 /*FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_DIREC */)==0)
    return(1);
return(0);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	struct ffblk f;

if(findfirst(filespec,&f,0)==0)
	return(f.ff_fsize);
return(-1L);
}

time_t ftimetounix(struct ftime ft)
{
	struct date da;
	struct time ti;

ti.ti_min=ft.ft_min;
ti.ti_hour=ft.ft_hour;
ti.ti_hund=0;
ti.ti_sec=ft.ft_tsec*2;
da.da_year=1980+ft.ft_year;
da.da_day=ft.ft_day;
da.da_mon=ft.ft_month;
return(dostounix(&da,&ti));
}

struct ftime unixtoftime(time_t unix)
{
	struct date da;
	struct time ti;
	struct ftime ft;

unixtodos(unix,&da,&ti);
ft.ft_min=ti.ti_min;
ft.ft_hour=ti.ti_hour;
ft.ft_tsec=ti.ti_sec/2;
ft.ft_year=da.da_year-1980;
ft.ft_day=da.da_day;
ft.ft_month=da.da_mon;
return(ft);
}

/****************************************************************************/
/* Returns the time/date of the file in 'filespec' in time_t (unix) format  */
/****************************************************************************/
long fdate(char *filespec)
{
    int file;
    struct ftime f;
	time_t t;

if((file=nopen(filespec,O_RDONLY))==-1)
	return(0);
getftime(file,&f);
t=ftimetounix(f);
close(file);
return(t);
}

long fdate_dir(char *filespec)
{
    struct ffblk f;
    struct date fd;
    struct time ft;

if(findfirst(filespec,&f,0/* FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_DIREC */)==0) {
	fd.da_day=f.ff_fdate&0x1f;
	fd.da_mon=(f.ff_fdate>>5)&0xf;
	fd.da_year=1980+((f.ff_fdate>>9)&0x7f);
	ft.ti_hour=(f.ff_ftime>>11)&0x1f;
	ft.ti_min=(f.ff_ftime>>5)&0x3f;
	ft.ti_sec=(f.ff_ftime&0xf)*2;
    return(dostounix(&fd,&ft)); }
else return(0);
}



/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(char *str)
{
    char *p;
    faddr_t addr;

addr.zone=addr.net=addr.node=addr.point=0;
if((p=strchr(str,':'))!=NULL) {
    addr.zone=atoi(str);
    addr.net=atoi(p+1); }
else {
    if(total_faddrs)
		addr.zone=faddr[0].zone;
    else
        addr.zone=1;
    addr.net=atoi(str); }
if(!addr.zone)              /* no such thing as zone 0 */
    addr.zone=1;
if((p=strchr(str,'/'))!=NULL)
    addr.node=atoi(p+1);
else {
    if(total_faddrs)
		addr.net=faddr[0].net;
    else
        addr.net=1;
    addr.node=atoi(str); }
if((p=strchr(str,'.'))!=NULL)
    addr.point=atoi(p+1);
return(addr);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(faddr_t addr)
{
    static char str[25];
    char point[25];

sprintf(str,"%u:%u/%u",addr.zone,addr.net,addr.node);
if(addr.point) {
    sprintf(point,".%u",addr.point);
    strcat(str,point); }
return(str);
}

/****************************************************************************/
/* Returns string for 2 digit hex+ numbers up to 575						*/
/****************************************************************************/
char *hexplus(uint num, char *str)
{
sprintf(str,"%03x",num);
str[0]=num/0x100 ? 'f'+(num/0x10)-0xf : str[1];
str[1]=str[2];
str[2]=0;
return(str);
}

uint hptoi(char *str)
{
	char tmp[128];
	uint i;

if(!str[1] || toupper(str[0])<='F')
	return(ahtoul(str));
strcpy(tmp,str);
tmp[0]='F';
i=ahtoul(tmp)+((toupper(str[0])-'F')*0x10);
return(i);
}

#ifndef __FLAT__

void beep(int freq, int dur)
{
sound(freq);
mswait(dur);
nosound();
}

int kbd_state(void)
{
return(peekb(0,0x417)); 	 /* Check scroll lock */
}

#elif defined(__WIN32__)

void beep(int freq, int dur)
{
Beep(freq,dur); 				// Requires WINDOWS.H
}

void mswait(int ms)
{
Sleep(ms);						// Requires WINDOWS.H
}

#endif

#ifdef __OS2__

int kbd_state(void)
{
	KBDINFO info;

KbdGetStatus(&info,0);
return(info.fsState);
}

void mswait(int msec)
{
DosSleep(msec ? msec : 1);
}

#elif defined(__WIN32__)

#define KBDSTF_RIGHTSHIFT				0x0001
#define KBDSTF_LEFTSHIFT				0x0002
#define KBDSTF_CONTROL					0x0004
#define KBDSTF_ALT						0x0008
#define KBDSTF_SCROLLLOCK_ON			0x0010
#define KBDSTF_NUMLOCK_ON				0x0020
#define KBDSTF_CAPSLOCK_ON				0x0040
#define KBDSTF_INSERT_ON				0x0080
#define KBDSTF_LEFTCONTROL				0x0100
#define KBDSTF_LEFTALT					0x0200
#define KBDSTF_RIGHTCONTROL 			0x0400
#define KBDSTF_RIGHTALT 				0x0800
#define KBDSTF_SCROLLLOCK				0x1000
#define KBDSTF_NUMLOCK					0x2000
#define KBDSTF_CAPSLOCK 				0x4000
#define KBDSTF_SYSREQ					0x8000

int kbd_state(void)
{
	int i=0;
	ulong l;
	INPUT_RECORD rec;

PeekConsoleInput(stdin,&rec,1,&l);
if(rec.EventType==KEY_EVENT)
	l=rec.Event.KeyEvent.dwControlKeyState;
else if(rec.EventType==MOUSE_EVENT)
	l=rec.Event.MouseEvent.dwControlKeyState;
else
	return(0);

/* Translate Win32 key state to IBM key state */

if(l&RIGHT_ALT_PRESSED)
	i|=KBDSTF_RIGHTALT;
if(l&LEFT_ALT_PRESSED)
	i|=KBDSTF_LEFTALT;
if(l&RIGHT_CTRL_PRESSED)
	i|=KBDSTF_RIGHTCONTROL;
if(l&LEFT_CTRL_PRESSED)
	i|=KBDSTF_LEFTCONTROL;
if(l&CAPSLOCK_ON)
	i|=KBDSTF_CAPSLOCK;
if(l&NUMLOCK_ON)
	i|=KBDSTF_NUMLOCK;
if(l&SCROLLLOCK_ON)
	i|=KBDSTF_SCROLLLOCK;
if(l&SHIFT_PRESSED)
	i|=KBDSTF_LEFTSHIFT;

return(i);
}



#endif

