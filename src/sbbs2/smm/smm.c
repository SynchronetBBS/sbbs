/* SMM.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Synchronet Match Maker */

#include "xsdk.h"
#include "crc32.h"
#include "smmdefs.h"
#include "smmvars.c"

/* RCIOLL.ASM */

int  rioini(int com,int irq);          /* initialize com,irq */
int  setbaud(int rate);                /* set baud rate */
int  rioctl(int action);               /* remote i/o control */
int  dtr(char onoff);                  /* set/reset dtr */
int  outcom(int ch);                   /* send character */
int  incom(void);                      /* receive character */
int  ivhctl(int intcode);              /* local i/o redirection */

/************************/
/* Remote I/O Constants */
/************************/

							/* i/o mode and state flags */
#define CTSCK 0x1000     	/* check cts (mode only) */
#define RTSCK 0x2000		/* check rts (mode only) */
#define TXBOF 0x0800		/* transmit buffer overflow (outcom only) */
#define ABORT 0x0400     	/* check for ^C (mode), aborting (state) */
#define PAUSE 0x0200     	/* check for ^S (mode), pausing (state) */
#define NOINP 0x0100     	/* input buffer empty (incom only) */

							/* status flags */
#define RIODCD	0x80		/* DCD on */
#define RI		0x40		/* Ring indicate */
#define DSR 	0x20		/* Dataset ready */
#define CTS 	0x10       	/* CTS on */
#define FERR 	0x08		/* Frameing error */
#define PERR 	0x04		/* Parity error */
#define OVRR 	0x02		/* Overrun */
#define RXLOST 	0x01       	/* Receive buffer overflow */

/* rioctl() arguments */
/* returns mode or state flags in high 8 bits, status flags in low 8 bits */

							/* the following return mode in high 8 bits */
#define IOMODE 0        	/* no operation */
#define IOSM 1          	/* i/o set mode flags */
#define IOCM 2          	/* i/o clear mode flags */
							/* the following return state in high 8 bits */
#define IOSTATE 4       	/* no operation */
#define IOSS 5          	/* i/o set state flags */
#define IOCS 6          	/* i/o clear state flags */
#define IOFB 0x308      	/* i/o buffer flush */
#define IOFI 0x208      	/* input buffer flush */
#define IOFO 0x108      	/* output buffer flush */
#define IOCE 9          	/* i/o clear error flags */

							/* return count (16bit)	*/
#define RXBC	0x0a		/* get receive buffer count */
#define TXBC	0x0b		/* get transmit buffer count */
#define TXSYNC  0x0c        /* sync transmition (seconds<<8|0x0c) */
#define IDLE	0x0d		/* suspend communication routines */
#define RESUME  0x10d		/* return from suspended state */
#define RLERC	0x000e		/* read line error count and clear */
#define CPTON	0x0110		/* set input translation flag for ctrl-p on */
#define CPTOFF	0x0010		/* set input translation flag for ctrl-p off */
#define GETCPT	0x8010		/* return the status of ctrl-p translation */
#define MSR 	0x0011		/* read modem status register */
#define FIFOCTL 0x0012		/* FIFO UART control */
#define TSTYPE	0x0013		/* Time-slice API type */
#define GETTST  0x8013      /* Get Time-slice API type */


#define I14DB	0x001d		/* DigiBoard int 14h driver */
#define I14PC	0x011d		/* PC int 14h driver */
#define I14PS	0x021d		/* PS/2 int 14h driver */
#define I14FO   0x031d      /* FOSSIL int 14h driver */


							/* ivhctl() arguments */
#define INT29R 1         	/* copy int 29h output to remote */
#define INT29L 2			/* Use _putlc for int 29h */
#define INT16  0x10      	/* return remote chars to int 16h calls */
#define INTCLR 0            /* release int 16h, int 29h */

#define CLREOL		256 	/* Character to erase to end of line		*/
#define HIGH        8       /* High intensity for curatr             */

extern uint riobp;
extern int mswtyp;

unsigned _stklen=16000; 	/* Set stack size in code, not header */

int cbreakh(void);			/* ctrl-break handler */

char getage(char *birth);
char long_user_info(user_t user);
void main_user_info(user_t user);

char *nulstr="";
char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};
char tmp[256],door_sys[128];

char io_int=0;
struct date date;
struct time curtime;
time_t now;
int smm_pause=1,node_scrnlen;
questionnaire_t *que[5];
int total_ques;
user_t useron,tmpuser;
ixb_t ixb;
ulong useron_record,system_crc;
FILE *stream,*index,*trashfile;

char *PrevReadSendQuitOrMore="\r\n\1n\1b\1h[\1cP\1b]revious screen, "
	"[\1cR\1b]ead profile, [\1cS\1b]end telegram, [\1cQ\1b]uit, or "
	"[\1cM\1b]ore: \1w";

int intocm(int in)
{
return(in*2.538071);
}

int cmtoin(int cm)
{
	int i;

i=cm*0.394;
if(((float)cm*0.394)-(float)i>=.5)
	i++;
return(i);
}

int kgtolp(int kg)
{
return(kg*2.2046);
}

int lptokg(int lp)
{
	int i;

i=lp*0.453597;
if(((float)lp*0.453597)-(float)i>=.5)
	i++;
return(i);
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...) {
	char sbuf[256];
	int chcount;

chcount=vsprintf(sbuf,fmat,_va_ptr);
lputs(sbuf);
return(chcount);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *cmdstr(char *instr, char *fpath, char *fspec, char *outstr)
{
	static char static_cmd[128];
	char str[256],str2[128],*cmd;
    int i,j,len;

if(outstr==NULL)
	cmd=static_cmd;
else
	cmd=outstr;
len=strlen(instr);
for(i=j=0;i<len && j<128;i++) {
    if(instr[i]=='%') {
        i++;
        cmd[j]=0;
        switch(toupper(instr[i])) {
            case 'A':   /* User alias */
				strcat(cmd,user_name);
                break;
            case 'B':   /* Baud (DTE) Rate */
				strcat(cmd,ultoa(com_rate,str,10));
                break;
            case 'C':   /* Connect Description */
				strcat(cmd,ultoa(user_dce,str,10));
                break;
            case 'F':   /* File path */
                strcat(cmd,fpath);
                break;
            case 'G':   /* Temp directory */
				strcat(cmd,temp_dir);
                break;
            case 'I':   /* UART IRQ Line */
                strcat(cmd,itoa(com_irq,str,10));
                break;
            case 'J':
				strcat(cmd,data_dir);
                break;
            case 'K':
				strcat(cmd,ctrl_dir);
                break;
            case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                strcat(cmd,node_dir);
                break;
            case 'O':   /* SysOp */
                strcat(cmd,sys_op);
                break;
            case 'P':   /* COM Port */
				strcat(cmd,itoa(com_port,str,10));
                break;
            case 'Q':   /* QWK ID */
                strcat(cmd,sys_id);
                break;
            case 'R':   /* Rows */
				strcat(cmd,itoa(user_rows,str,10));
                break;
            case 'S':   /* File Spec */
                strcat(cmd,fspec);
                break;
            case 'T':   /* Time left in seconds */
				strcat(cmd,itoa(time(NULL)-starttime,str,10));
                break;
            case 'U':   /* UART I/O Address (in hex) */
                strcat(cmd,itoa(com_base,str,16));
                break;
            case 'W':   /* Time-slice API type (mswtype) */
#ifndef __OS2__
                strcat(cmd,itoa(mswtyp,str,10));
#endif
                break;
            case '&':   /* Address of msr */
				sprintf(str,"%lu",&riobp-1);
                strcat(cmd,str);
                break;
			case 'Z':
				strcat(cmd,text_dir);
                break;
            case '!':   /* EXEC Directory */
				strcat(cmd,exec_dir);
                break;
            case '#':   /* Node number (same as SBBSNNUM environment var) */
                sprintf(str,"%d",node_num);
                strcat(cmd,str);
                break;
            case '*':
                sprintf(str,"%03d",node_num);
                strcat(cmd,str);
                break;
            case '$':   /* Credits */
				strcat(cmd,ultoa(user_cdt,str,10));
                break;
            case '%':   /* %% for percent sign */
                strcat(cmd,"%");
                break;
            default:    /* unknown specification */
                if(isdigit(instr[i])) {
					sprintf(str,"%0*d",instr[i]&0xf,user_number);
                    strcat(cmd,str); }
                break; }
        j=strlen(cmd); }
    else
        cmd[j++]=instr[i]; }
cmd[j]=0;

return(cmd);
}

char *base41(unsigned int i, char *str)
{
	char c;
	unsigned int j=41*41,k;

for(c=0;c<3;c++) {
	k=i/j;
	str[c]='0'+k;
	i-=(k*j);
	j/=41;
	if(str[c]>=':')
		str[c]='A'+(str[c]-':');
	if(str[c]>='[')
		str[c]='#'+(str[c]-'['); }
str[c]=0;
return(str);
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
/* Returns 16-crc of string (not counting terminating NULL)                 */
/****************************************************************************/
ushort crc16(char *str)
{
    int     i=0;
    ushort  crc=0;

ucrc16(0,&crc);
while(str[i])
    ucrc16(str[i++],&crc);
ucrc16(0,&crc);
ucrc16(0,&crc);
return(crc);
}


int cdt_warning(long cdt)
{
if(cdt==0)
	return(1);
if(cdt>0) {
	bprintf("\1m\1hYou will receive \1w%luk\1m in credits for this action!\r\n"
		,cdt/1024L);
	return(1); }

bprintf("\1m\1hThis action will cost you \1w%luk\1m in credits.",(-cdt)/1024L);
if(user_cdt+cdt_adjustment<-cdt) {
	bprintf("\r\n\r\n\1r\1hSorry, you only have \1w%luk\1m in credits.\r\n"
		,(user_cdt+cdt_adjustment)/1024L);
	return(0); }
return(!noyes(" Continue"));
}

void adjust_cdt(long cdt)
{
if(cdt==0)
	return;
cdt_adjustment+=cdt;
}

int got_flags(char *req, char *got)
{
	int i,j;

for(i=0;req[i];i++) {
	for(j=0;got[j];j++)
		if(req[i]==got[j])
			break;
	if(!got[j])
		break; }
if(!req[i])
	return(1);
return(0);
}

int can_add()
{
	uchar age=getage(user_birth);

if(user_level<min_level || (age && getage(user_birth)<min_age)
	|| !got_flags(req_flags1,user_flags1)
	|| !got_flags(req_flags2,user_flags2)
	|| !got_flags(req_flags3,user_flags3)
	|| !got_flags(req_flags4,user_flags4)
	)
	return(0);
return(1);
}


int trash(char *instr)
{
	char str[128],word[128];

if(!trashfile)
	return(0);
strcpy(str,instr);
strupr(str);
rewind(trashfile);
while(!ferror(trashfile)) {
	if(!fgets(word,125,trashfile))
		break;
	truncsp(word);
	if(!word[0])
		continue;
	strupr(word);
	if(strstr(str,word))
		return(1); }
return(0);
}

long fdate_dir(char *filespec)
{
    struct ffblk f;
    struct date fd;
    struct time ft;

if(findfirst(filespec,&f,FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_DIREC)==NULL) {
	fd.da_day=f.ff_fdate&0x1f;
	fd.da_mon=(f.ff_fdate>>5)&0xf;
	fd.da_year=1980+((f.ff_fdate>>9)&0x7f);
	ft.ti_hour=(f.ff_ftime>>11)&0x1f;
	ft.ti_min=(f.ff_ftime>>5)&0x3f;
	ft.ti_sec=(f.ff_ftime&0xf)*2;
    return(dostounix(&fd,&ft)); }
else return(NULL);
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char *timestr(time_t *intime)
{
    static char str[256];
    char mer[3],hour;
    struct tm *gm;

gm=localtime(intime);
if(gm->tm_hour>=12) {
    if(gm->tm_hour==12)
        hour=12;
    else
        hour=gm->tm_hour-12;
    strcpy(mer,"pm"); }
else {
    if(gm->tm_hour==0)
        hour=12;
    else
        hour=gm->tm_hour;
    strcpy(mer,"am"); }
sprintf(str,"%s %s %02d %4d %02d:%02d %s"
    ,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,1900+gm->tm_year
    ,hour,gm->tm_min,mer);
return(str);
}


void puttgram(int usernumber, char *strin)
{
	char str[256];
	int file,i;

sprintf(str,"%4.4u.MSG",usernumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	printf("\7Error opening/creating %s for creat/append access\n",str);
	return; }
i=strlen(strin);
if(write(file,strin,i)!=i) {
	close(file);
	printf("\7Error writing %u bytes to %s\n",i,str);
	return; }
close(file);
}


int send_telegram(user_t user)
{
	uchar str[256],line[256],buf[1024];
	int i;

if(!useron.number) {
	bputs("\r\n\1h\1rYou must create a profile first.\r\n");
	pause();
	return(1); }

if(user_level<telegram_level) {
	bputs("\r\n\1h\1rYou have insufficient access to send telegrams."
		"\r\n");
	pause();
	return(1); }

main_user_info(user);
CRLF;
if(!cdt_warning(telegram_cdt))
	return(0);
if(telegram_cdt)
	CRLF;
bprintf("\1n\1hSending a telegram to \1y%s\1w:\r\n\r\n"
	,user.name);
now=time(NULL);
memset(buf,0,512);
sprintf(buf,"\1n\1c\1hMatch Maker\1b telegram from \1c%s\1b "
	"on %s:\1y\r\n"
	,useron.name,timestr(&now));
for(i=0;i<5 && !aborted;i++) {
	bprintf("\1n\1h\1g%u of 5: \1n\1g",i+1);
	if(!getstr(line,70,i==4 ? K_MSG:K_MSG|K_WRAP))
		break;
	sprintf(str,"\1n\1g%4s%s\r\n",nulstr,line);
	strcat(buf,str); }
if(!i || aborted || !yesno("\r\nSave"))
	return(0);
if(!(user.misc&USER_FROMSMB)) {
	putsmsg(user.number,TGRAM_NOTICE);
	puttgram(user.number,buf); }
else {
	if((i=nopen("TELEGRAM.DAB",O_WRONLY|O_CREAT|O_APPEND|SH_DENYNO))
		==-1) {
		bprintf("\r\n\1r\1h\1iError writing telegram!\r\n");
		pause();
		return(1); }
	write(i,useron.system,sizeof(useron.system));
	write(i,user.system,sizeof(user.system));
	write(i,&user.number,sizeof(user.number));
	write(i,buf,512);
	close(i); }
adjust_cdt(telegram_cdt);
if(notify_user && notify_user!=user_number) {
	sprintf(str,"\1n\1hSMM: \1y%s\1m sent a telegram to \1y%s\1m "
		"from the Match Maker\r\n",user_name,user.name);
	if(node_dir[0])
		putsmsg(notify_user,str);
	else
		puttgram(notify_user,str); }
return(1);
}

/****************************************************************************/
/* Returns 32-crc of string (not counting terminating NULL) 				*/
/****************************************************************************/
ulong crc32(char *str)
{
	int i=0;
	ulong crc=0xffffffffUL;

	while(str[i])
		crc=ucrc32(str[i++],crc);
	crc=~crc;
	return(crc);
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
	sprintf(str,"%02u/%02u/%02u",date.da_mon,date.da_day
		,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900); }
return(str);
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{

if(!strcmp(str,"00/00/00") || !str[0])
	return(NULL);
curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
if(str[6]<'7')
	date.da_year=2000+((str[6]&0xf)*10)+(str[7]&0xf);
else
	date.da_year=1900+((str[6]&0xf)*10)+(str[7]&0xf);
date.da_mon=((str[0]&0xf)*10)+(str[1]&0xf);
date.da_day=((str[3]&0xf)*10)+(str[4]&0xf);
return(dostounix(&date,&curtime));
}

/****************************************************************************/
/* Returns the age derived from the string 'birth' in the format MM/DD/YY	*/
/****************************************************************************/
char getage(char *birth)
{
	char age;

if(birth[0]<=SP)
	return(0);
getdate(&date);
age=(date.da_year-1900)-(((birth[6]&0xf)*10)+(birth[7]&0xf));
if(age>90)
	age-=90;
if(atoi(birth)>12 || atoi(birth+3)>31)
	return(0);
if(((birth[0]&0xf)*10)+(birth[1]&0xf)>date.da_mon ||
	(((birth[0]&0xf)*10)+(birth[1]&0xf)==date.da_mon &&
	((birth[3]&0xf)*10)+(birth[4]&0xf)>date.da_day))
	age--;
if(age<0)
	return(0);
return(age);
}

char marital_ch(char user_marital)
{
switch(user_marital) {
	case MARITAL_SINGLE:
		return('S');
	case MARITAL_DIVORCED:
		return('D');
	case MARITAL_MARRIED:
		return('M');
	case MARITAL_WIDOWED:
		return('W');
	case MARITAL_OTHER:
		return('O'); }
if(user_marital)
	return('*');
return(SP);
}

char race_ch(char user_race)
{
switch(user_race) {
	case RACE_WHITE:
		return('W');
	case RACE_BLACK:
		return('B');
	case RACE_HISPANIC:
		return('H');
	case RACE_AMERINDIAN:
		return('I');
	case RACE_ASIAN:
		return('A');
	case RACE_MIDEASTERN:
		return('M');
	case RACE_OTHER:
		return('O'); }
if(user_race)
	return('*');
return(SP);
}

char *hair(char user_hair)
{
switch(user_hair) {
	case HAIR_BLONDE:
		return("BLN");
	case HAIR_BROWN:
		return("BRN");
	case HAIR_RED:
		return("RED");
	case HAIR_BLACK:
		return("BLK");
	case HAIR_GREY:
		return("GRY");
	case HAIR_OTHER:
		return("OTH"); }
if(user_hair)
	return("*");
return(nulstr);
}

char *eyes(char user_eyes)
{
switch(user_eyes) {
	case EYES_BLUE:
		return("BLU");
	case EYES_BROWN:
		return("BRN");
	case EYES_GREEN:
		return("GRN");
	case EYES_HAZEL:
		return("HAZ");
	case EYES_OTHER:
		return("OTH"); }
if(user_eyes)
	return("*");
return(nulstr);
}

char *marital(char user_marital)
{
switch(user_marital) {
    case MARITAL_SINGLE:
		return("Single");
    case MARITAL_MARRIED:
		return("Married");
    case MARITAL_DIVORCED:
		return("Divorced");
    case MARITAL_WIDOWED:
		return("Widowed");
	case MARITAL_OTHER:
		return("Other"); }
if(user_marital)
	return("*");
return(nulstr);
}

char *race(char user_race)
{
switch(user_race) {
	case RACE_WHITE:
		return("White");
	case RACE_BLACK:
		return("Black");
	case RACE_HISPANIC:
		return("Hispanic");
	case RACE_ASIAN:
		return("Asian");
	case RACE_AMERINDIAN:
		return("American Indian");
	case RACE_MIDEASTERN:
		return("Middle Eastern");
	case RACE_OTHER:
		return("Other"); }
if(user_race)
	return("*");
return(nulstr);
}

ushort getzodiac(char *birth)
{
if((!strncmp(birth,"03",2) && atoi(birth+3)>=21)
	|| (!strncmp(birth,"04",2) && atoi(birth+3)<=19))
    return(ZODIAC_ARIES);
if((!strncmp(birth,"04",2) && atoi(birth+3)>=20)
	|| (!strncmp(birth,"05",2) && atoi(birth+3)<=20))
	return(ZODIAC_TAURUS);
if((!strncmp(birth,"05",2) && atoi(birth+3)>=21)
	|| (!strncmp(birth,"06",2) && atoi(birth+3)<=20))
	return(ZODIAC_GEMINI);
if((!strncmp(birth,"06",2) && atoi(birth+3)>=21)
	|| (!strncmp(birth,"07",2) && atoi(birth+3)<=22))
	return(ZODIAC_CANCER);
if((!strncmp(birth,"07",2) && atoi(birth+3)>=23)
	|| (!strncmp(birth,"08",2) && atoi(birth+3)<=22))
	return(ZODIAC_LEO);
if((!strncmp(birth,"08",2) && atoi(birth+3)>=23)
	|| (!strncmp(birth,"09",2) && atoi(birth+3)<=22))
	return(ZODIAC_VIRGO);
if((!strncmp(birth,"09",2) && atoi(birth+3)>=23)
	|| (!strncmp(birth,"10",2) && atoi(birth+3)<=22))
	return(ZODIAC_LIBRA);
if((!strncmp(birth,"10",2) && atoi(birth+3)>=23)
	|| (!strncmp(birth,"11",2) && atoi(birth+3)<=21))
	return(ZODIAC_SCORPIO);
if((!strncmp(birth,"11",2) && atoi(birth+3)>=22)
	|| (!strncmp(birth,"12",2) && atoi(birth+3)<=21))
	return(ZODIAC_SAGITTARIUS);
if((!strncmp(birth,"12",2) && atoi(birth+3)>=22)
	|| (!strncmp(birth,"01",2) && atoi(birth+3)<=19))
	return(ZODIAC_CAPRICORN);
if((!strncmp(birth,"01",2) && atoi(birth+3)>=20)
	|| (!strncmp(birth,"02",2) && atoi(birth+3)<=18))
	return(ZODIAC_AQUARIUS);
if((!strncmp(birth,"02",2) && atoi(birth+3)>=19)
	|| (!strncmp(birth,"03",2) && atoi(birth+3)<=20))
	return(ZODIAC_PISCES);
return(0xff);
}

char *zodiac(short user_zodiac)
{
switch(user_zodiac)
{
	case ZODIAC_ARIES:
		return("Aries");
	case ZODIAC_TAURUS:
		return("Taurus");
	case ZODIAC_GEMINI:
		return("Gemini");
	case ZODIAC_CANCER:
		return("Cancer");
	case ZODIAC_LEO:
		return("Leo");
	case ZODIAC_VIRGO:
		return("Virgo");
	case ZODIAC_LIBRA:
		return("Libra");
	case ZODIAC_SCORPIO:
		return("Scorpio");
	case ZODIAC_SAGITTARIUS:
		return("Sagittarius");
	case ZODIAC_CAPRICORN:
		return("Capricorn");
	case ZODIAC_AQUARIUS:
		return("Aquarius");
	case ZODIAC_PISCES:
		return("Pisces"); }
return(nulstr);
}

short ans2bits(char *str)
{
	int i,j;
	ushort bits=0;

j=strlen(str);
for(i=0;i<j;i++) {
	if(str[i]=='*')
		return(0xffff);
	bits|=(1<<str[i]-'A'); }
return(bits);
}

uchar ans2uchar(char *str)
{
	int i,j;
	uchar bits=0;

j=strlen(str);
for(i=0;i<j;i++) {
	if(str[i]=='*')
		return(0xff);
	bits|=(1<<str[i]-'A'); }
return(bits);
}


void bits2ans(ushort bits, char *str)
{
	char	tmp[25];
	int 	i;

str[0]=0;
if(bits==0xffff) {
	strcpy(str,"*");
	return; }
for(i=0;i<16;i++)
	if(bits&(1<<i)) {
		tmp[0]='A'+i;
		tmp[1]=0;
		strcat(str,tmp); }
}

void bits2str(ushort user, char *str)
{
	int 	i;

str[0]=0;
if(user==0xffff) {
	strcpy(str,"*");
	return; }
for(i=0;i<16;i++)
	if(user&(1<<i))
		str[i]='A'+i;
	else
		str[i]=SP;
str[i]=0;
}

void uchar2ans(uchar bits, char *str)
{
	char	tmp[25];
	int 	i;

str[0]=0;
if(bits==0xff) {
	strcpy(str,"*");
	return; }
for(i=0;i<8;i++)
	if(bits&(1<<i)) {
		tmp[0]='A'+i;
		tmp[1]=0;
		strcat(str,tmp); }
}

int basic_match(user_t user, user_t mate)
{
	int max=0,match=0,age=getage(mate.birth),zodiac=getzodiac(mate.birth);

if(user.pref_sex!='*' && user.pref_sex!=mate.sex
	&& !(user.misc&USER_FRIEND))
	return(0);

if(zodiac&user.pref_zodiac) match++;
else if(user.misc&USER_REQZODIAC)
	return(0);
if(zodiac==user.pref_zodiac) match++;
max+=2;

if(stricmp(mate.zipcode,user.min_zipcode)>=0
	&& stricmp(mate.zipcode,user.max_zipcode)<=0) match+=2;
else if(user.misc&USER_REQZIP) return(0);
max+=2;

if(mate.marital&user.pref_marital) match++;
else if(user.misc&USER_REQMARITAL)
	return(0);
if(mate.marital==user.pref_marital) match++;
max+=2;

if(mate.race&user.pref_race) match++;
else if(user.misc&USER_REQRACE)
	return(0);
if(mate.race==user.pref_race) match++;
max+=2;

if(mate.hair&user.pref_hair) match++;
else if(user.misc&USER_REQHAIR)
	return(0);
if(mate.hair==user.pref_hair) match++;
max+=2;

if(mate.eyes&user.pref_eyes) match++;
else if(user.misc&USER_REQEYES)
	return(0);
if(mate.eyes==user.pref_eyes) match++;
max+=2;

if(age>=user.min_age
	&& (!user.max_age || age<=user.max_age)) match+=2;
else {
	if(user.misc&USER_REQAGE) return(0);
	if(age<user.min_age-((user.max_age-user.min_age)/2)
		|| (user.max_age && age>user.max_age+((user.max_age-user.min_age)/2)))
		match-=4;
	else if(age<user.min_age-((user.max_age-user.min_age)/3)
		|| (user.max_age && age>user.max_age+((user.max_age-user.min_age)/3)))
		match-=3;
	else if(age<user.min_age-((user.max_age-user.min_age)/4)
		|| (user.max_age && age>user.max_age+((user.max_age-user.min_age)/4)))
		match-=2;
	else if(age<user.min_age-((user.max_age-user.min_age)/5)
		|| (user.max_age && age>user.max_age+((user.max_age-user.min_age)/5)))
		match--; }

max+=2;

if(mate.weight>=user.min_weight
	&& (!user.max_weight || mate.weight<=user.max_weight)) match+=2;
else {
	if(user.misc&USER_REQWEIGHT) return(0);
	if(mate.weight<user.min_weight-((user.max_weight-user.min_weight)/2)
		|| (user.max_weight
		&& mate.weight>user.max_weight+((user.max_weight-user.min_weight)/2)))
		match-=4;
	else if(mate.weight<user.min_weight-((user.max_weight-user.min_weight)/3)
		|| (user.max_weight
		&& mate.weight>user.max_weight+((user.max_weight-user.min_weight)/3)))
		match-=3;
	else if(mate.weight<user.min_weight-((user.max_weight-user.min_weight)/4)
		|| (user.max_weight
		&& mate.weight>user.max_weight+((user.max_weight-user.min_weight)/4)))
		match-=2;
	else if(mate.weight<user.min_weight-((user.max_weight-user.min_weight)/5)
		|| (user.max_weight
		&& mate.weight>user.max_weight+((user.max_weight-user.min_weight)/5)))
		match--; }
max+=2;

if(mate.height>=user.min_height
	&& (!user.max_height || mate.height<=user.max_height)) match+=2;
else {
	if(user.misc&USER_REQHEIGHT) return(0);
	if(mate.height<user.min_height-((user.max_height-user.min_height)/2)
		|| (user.max_height
		&& mate.height>user.max_height+((user.max_height-user.min_height)/2)))
		match-=4;
	else if(mate.height<user.min_height-((user.max_height-user.min_height)/3)
		|| (user.max_height
		&& mate.height>user.max_height+((user.max_height-user.min_height)/3)))
		match-=3;
	else if(mate.height<user.min_height-((user.max_height-user.min_height)/4)
		|| (user.max_height
		&& mate.height>user.max_height+((user.max_height-user.min_height)/4)))
		match-=2;
	else if(mate.height<user.min_height-((user.max_height-user.min_height)/5)
		|| (user.max_height
		&& mate.height>user.max_height+((user.max_height-user.min_height)/5)))
		match--; }
max+=2;

if(mate.income>=user.min_income
	&& (!user.max_income || mate.income==0xffffffffUL
	|| mate.income<=user.max_income)) match++;
else {
	if(user.misc&USER_REQINCOME) return(0);
	if(mate.income<user.min_income-((user.max_income-user.min_income)/2)
		|| (user.max_income
		&& mate.income>user.max_income+((user.max_income-user.min_income)/2)))
		match-=4;
	else if(mate.income<user.min_income-((user.max_income-user.min_income)/3)
		|| (user.max_income
		&& mate.income>user.max_income+((user.max_income-user.min_income)/3)))
		match-=3;
	else if(mate.income<user.min_income-((user.max_income-user.min_income)/4)
		|| (user.max_income
		&& mate.income>user.max_income+((user.max_income-user.min_income)/4)))
		match-=2;
	else if(mate.income<user.min_income-((user.max_income-user.min_income)/5)
		|| (user.max_income
		&& mate.income>user.max_income+((user.max_income-user.min_income)/5)))
		match--; }
max++;

if(match<=0)
	return(0);

return(((float)match/max)*100.0);
}

int sys_quenum(char *name)
{
	int i;

for(i=0;i<total_ques;i++)
	if(!stricmp(que[i]->name,name))
		break;
if(i<total_ques)
	return(i);
return(-1);
}

int user_quenum(char *name, user_t user)
{
	int i;

for(i=0;i<5;i++)
	if(!stricmp(user.queans[i].name,name))
		break;
if(i<5)
	return(i);
return(-1);
}

int total_match(user_t user, user_t mate)
{
	int i,j,s,u,match1,match2,quematches=0,quemax=0,quematch;

match1=basic_match(user,mate);
if(!match1) return(0);
match2=basic_match(mate,user);
if(!match2) return(0);

for(i=0;i<5;i++) {
	if(!user.queans[i].name[0])
		continue;
	s=sys_quenum(user.queans[i].name);
	if(s==-1)
		continue;
	u=user_quenum(user.queans[i].name,mate);
	if(u==-1)
		continue;
	for(j=0;j<que[s]->total_ques;j++) {
		if(user.queans[u].pref[j]&mate.queans[i].self[j]) quematches+=4;
		if(user.queans[u].pref[j]==mate.queans[i].self[j]) quematches+=2;
		if(user.queans[u].self[j]&mate.queans[i].pref[j]) quematches+=2;
		if(user.queans[u].self[j]==mate.queans[i].pref[j]) quematches++;
		quemax+=9; } }

if(!quemax) 	/* no questionnaires in common */
	return((match1+match1+match1+match1+match2+match2)/6);

quematch=((float)quematches/quemax)*100.0;
return((match1+match1+match1+match1+match2+match2+quematch)/7);
}


void main_user_info(user_t user)
{
	char str[128],min[64],max[64];
	int i;

attr(LIGHTGRAY);
cls();
if(SYSOP)
	bprintf("\1n\1gReal: \1h%.25s #%lu (%.40s)\r\n"
		,user.realname,user.number,user.system);
bprintf("\1n\1gName: \1h%-25.25s     \1b%-10s   \1c%-15s  \1m%s\r\n"
	,user.name
	,marital(user.marital),race(user.race)
	,user.sex=='M' ? "Male":"Female");
if(smm_misc&SMM_METRIC)
	sprintf(str," %-4u",intocm(user.height));
else
	sprintf(str,"%2u'%-2u",user.height/12,user.height%12);
bprintf("\1n\1gHair: \1h%3s  \1n\1gEyes: \1h%3s  \1n\1gHeight:"
	"\1h%s  \1n\1gWeight: \1h%-3u  \1n\1gAge: \1h%-3u "
    "\1n\1gZodiac: \1h%s\r\n"
	,hair(user.hair),eyes(user.eyes),str
	,smm_misc&SMM_METRIC ? lptokg(user.weight) : user.weight
	,getage(user.birth),zodiac(getzodiac(user.birth)));
bprintf("\1n\1gFrom: \1h%-30.30s   \1n\1gZip: \1h%-10.10s    "
	"\1n\1gIncome: \1h$%sK\1n\1g/year\r\n"
	,user.location,user.zipcode
	,user.income==0xffffffffUL ? "?":ultoa(user.income/1000UL,tmp,10));
if(smm_misc&SMM_METRIC) {
	sprintf(min,"%u",intocm(user.min_height));
	sprintf(max,"%u",intocm(user.max_height)); }
else {
	sprintf(min,"%u'%u",user.min_height/12,user.min_height%12);
	sprintf(max,"%u'%u",user.max_height/12,user.max_height%12); }
bprintf("\1n\1gPref: \1h\1%c%c\1%c%c\1m%c \1%c%3u\1n\1g-\1h\1%c%u\1n\1h  "
	"\1%c%s \1n\1ghair  \1h\1%c%s \1n\1geyes  \1h\1%c%s\1n\1g-\1h\1%c%s  "
	"\1%c%u\1n\1g-\1h\1%c%u\1n\1glbs  \1h\1%c$%luK\1n\1g-\1h\1%c%luK\1n\1%c  "
	"\1h%.3s\r\n"
	,user.misc&USER_REQMARITAL ? 'w':'b'
	,marital_ch(user.pref_marital)
	,user.misc&USER_REQRACE ? 'w':'g'
	,race_ch(user.pref_race)
	,user.pref_sex
	,user.misc&USER_REQAGE	? 'w':'g'
	,user.min_age
	,user.misc&USER_REQAGE	? 'w':'g'
	,user.max_age
	,user.misc&USER_REQHAIR  ? 'w':'g'
	,hair(user.pref_hair)
	,user.misc&USER_REQEYES  ? 'w':'g'
	,eyes(user.pref_eyes)
	,user.misc&USER_REQHEIGHT  ? 'w':'g'
	,min
	,user.misc&USER_REQHEIGHT  ? 'w':'g'
	,max
	,user.misc&USER_REQWEIGHT  ? 'w':'g'
	,smm_misc&SMM_METRIC ? lptokg(user.min_weight) : user.min_weight
	,user.misc&USER_REQWEIGHT  ? 'w':'g'
	,smm_misc&SMM_METRIC ? lptokg(user.max_weight) : user.max_weight
	,user.misc&USER_REQINCOME  ? 'w':'g'
	,user.min_income/1000L
	,user.misc&USER_REQINCOME  ? 'w':'g'
	,user.max_income/1000L
	,user.misc&USER_REQZODIAC  ? 'w':'g'
    ,zodiac(user.pref_zodiac));

bprintf("\1r\1i%s\1n\1w\1h\r\n",user.misc&USER_PHOTO ? "PHOTO":nulstr);
for(i=0;i<5;i++) {
    if(!user.note[i][0])
        break;
	bprintf("%15s%.50s\r\n",nulstr,user.note[i]); }
}

char long_user_info(user_t user)
{
	char str[128],fname[128],path[128],ch;
	int i,j,k,s,u,max,match1,match2,user_match,mate_match;

while(1) {
checkline();
main_user_info(user);

CRLF;
/**
if(user.misc&USER_FROMSMB)
	bprintf("\1r\1hImported via message base from \1w%s\1r.\r\n"
		,user.system);
**/
bprintf("\1h\1mThis user meets your profile preferences:\1w%3u%%            "
	,match1=basic_match(useron,user));
bprintf("\1n\1gCreated: \1h%s\r\n",unixtodstr(user.created,str));
bprintf("\1h\1mYou meet this user's profile preferences:\1w%3u%%            "
	,match2=basic_match(user,useron));
bprintf("\1n\1gUpdated: \1h%s\r\n",unixtodstr(user.updated,str));

if(!smm_pause)
	lncntr=0;

CRLF;
bputs("\1n\1b\1hQuestionnaires:\r\n");
for(i=0;i<5;i++) {
	max=mate_match=user_match=0;
	if(!user.queans[i].name[0])
		continue;
	s=sys_quenum(user.queans[i].name);
	if(s==-1)
		continue;
	if(que[s]->req_age>getage(useron.birth))
		continue;
	if(match1 && match2)
		u=user_quenum(user.queans[i].name,useron);
	else
		u=-1;
	bprintf("\1h\1c%-25s ",que[s]->desc);
	if(u==-1) {
		CRLF;
		continue; }
	for(j=0;j<que[s]->total_ques;j++) {
		if(useron.queans[u].pref[j]&user.queans[i].self[j]) user_match+=2;
		if(useron.queans[u].pref[j]==user.queans[i].self[j]) user_match++;
		if(useron.queans[u].self[j]&user.queans[i].pref[j]) mate_match+=2;
		if(useron.queans[u].self[j]==user.queans[i].pref[j]) mate_match++;
		max+=3; }
	bprintf("\1n\1c%s matches your pref:\1h%3u%%  "
			"\1n\1cYou match %s pref:\1h%3u%%\r\n"
			,user.sex=='M' ? "He":"She"
			,(int)(((float)user_match/max)*100.0)
			,user.sex=='M' ? "his":"her"
			,(int)(((float)mate_match/max)*100.0)); }

bprintf("\r\n\1r\1hOverall match: \1w%u%%",total_match(useron,user));
if(user.purity)
	bprintf("        \1r\1hPurity test: \1w%u%%",user.purity);
if(user.mbtype[0])
	bprintf("        \1r\1hPersonality type: \1w%s",user.mbtype);
CRLF;

if(aborted) {
	aborted=0;
	return(0); }
if(!smm_pause) {
	if(kbhit())
		return(0);
	return(1); }
nodesync();
if(lncntr>=user_rows-2)
    lncntr=0;
CRLF;
bputs("\1h\1b[\1cV\1b]iew questionnaires, ");
if(user.misc&USER_PHOTO)
	bputs("[\1cD\1b]ownload photo, ");
bputs("[\1cS\1b]end telegram, [\1cQ\1b]uit, or [Next]: \1c");
strcpy(str,"VSQN\r");
if(user.misc&USER_PHOTO)
	strcat(str,"D");
if(SYSOP)
	strcat(str,"+");
switch(getkeys(str,0)) {
	case '+':
		user.misc|=USER_PHOTO;
		break;
	case 'Q':
		return(0);
	case CR:
	case 'N':
		return(1);
	case 'D':
		if(!useron.number) {
			bputs("\r\n\1h\1rYou must create a profile first.\r\n");
			pause();
			break; }
		if(!(useron.misc&USER_PHOTO)) {
			bputs("\r\n\1h\1rYou cannot download photos until your photo is "
				"added.\r\n");
			pause();
			break; }
		for(i=0;user.system[i];i++)
			if(isalnum(user.system[i]))
				break;
		if(!user.system[i])
			fname[0]='~';
		else
			fname[0]=user.system[i];
		for(i=strlen(user.system)-1;i>0;i--)
			if(isalnum(user.system[i]))
				break;
		if(i<=0)
			fname[1]='~';
		else
			fname[1]=user.system[i];
		fname[2]=0;
		strupr(user.system);
		strcat(fname,base41(crc16(user.system),tmp));
		strcat(fname,base41(user.number,tmp));
		strcat(fname,".JPG");
		strupr(fname);
		sprintf(path,"PHOTO\\%s",fname);
		if(!fexist(path)) {
			bputs("\r\n\1n\1hUnpacking...");
			sprintf(str,"%spkunzip photo %s photo",exec_dir,fname);
			system(str);
			bputs("\r\n");
			if(!fexist(path)) {
				bputs("\r\n\1n\1h\1i\1rUnpacking failed!\1n\r\n");
				pause();
				break; } }
		if(com_port) {
			cmdstr(zmodem_send,path,path,str);
			ivhctl(INTCLR);
			ivhctl(INT29L);
			system(str);
			ivhctl(INTCLR);
			i=INT29L;
			i|=(INT29R|INT16);
			ivhctl(i); }
		else
			system(cmdstr(local_view,path,path,str));
		pause();
		break;


	case 'S':
		send_telegram(user);
		break;
	case 'V':

		if(user_level<que_level) {
			bputs("\r\n\1h\1rYou have insufficient access to read "
				"questoinnaires.\r\n");
			pause();
            break; }

		for(i=0;i<5;i++) {
			aborted=0;
			if(!user.queans[i].name[0])
				continue;
			s=sys_quenum(user.queans[i].name);
			if(s==-1)
				continue;
			if(que[s]->req_age>getage(useron.birth))
				continue;
			if(match1 && match2)
				u=user_quenum(user.queans[i].name,useron);
			else
				u=-1;
			sprintf(str,"\r\nDo you wish to view the \1w%s\1b questionnaire"
				,que[s]->desc);
			if(!yesno(str))
				continue;
			if(!cdt_warning(que_cdt))
				continue;
			adjust_cdt(que_cdt);
			for(j=0;j<que[s]->total_ques && !aborted;) {
				cls();
				bprintf("\1n\1m\1hQuestionnaire: \1y%-25s \1mQuestion: "
					"\1y%d \1mof \1y%d  \1h\1b(Ctrl-C to Abort)\r\n\r\n"
					,que[s]->desc,j+1,que[s]->total_ques);
				bprintf("\1w\1h%s\r\n\r\n",que[s]->que[j].txt);
				for(k=0;k<que[s]->que[j].answers;k++)
					bprintf("\1h\1b%c\1w) \1g%s\r\n",'A'+k
						,que[s]->que[j].ans[k]);
				bits2str(user.queans[i].self[j],str);
				bprintf("\1n\1g\r\n%25s: \1h%-16s "
					,user.name,str);
				bits2str(user.queans[i].pref[j],str);
				bprintf("\1n\1g %15s: \1h%s","Preferred mate",str);
				if(u!=-1) {
					bits2str(useron.queans[u].pref[j],str);
					bprintf("\1n\1g\r\n%25s: \1h%-16s "
						,"Your preferred mate",str);
					bits2str(useron.queans[u].self[j],str);
					bprintf("\1n\1g %15s: \1h%s","You",str); }
				CRLF;
				if(!aborted) {
					lncntr=0;
					bputs("\r\n\1h\1b[\1cP\1b]revious, [\1cQ\1b]uit, "
						"or [Next]: \1c");
					ch=getkeys("PQN\r",0);
					if(ch=='P') {
						if(j) j--;
						continue; }
					if(ch=='Q')
						break;
					j++; } } }
		break;

	} }
return(0);
}


/* Returns 0 if aborted, 1 if continue, negative if previous */
int short_user_info(user_t user)
{
	char str[128],height[64],ch;
    int i,match;
    int records;
    ulong  name_crc;
	static char name[26],lastname[26],highmatch;
    static long topixb,topdab,lastixb,lastdab;
    static int count;

if(!lncntr && smm_pause) {
	topdab=ftell(stream)-sizeof(user_t);
	topixb=ftell(index)-sizeof(ixb_t);
    if(!topdab)
        lastdab=lastixb=0;
    count=0;
	printfile("LIST_HDR.ASC");
	strcpy(lastname,name);
	name[0]=highmatch=0; }
if(user.number) {
	match=total_match(useron,user);
    if(!match) str[0]=0;
    else if(match>=90) sprintf(str,"\1w%u%%",match);
    else if(match>=80) sprintf(str,"\1c%u%%",match);
    else if(match>=70) sprintf(str,"\1y%u%%",match);
    else if(match>=60) sprintf(str,"\1g%u%%",match);
    else if(match>=50) sprintf(str,"\1m%u%%",match);
    else if(match>=40) sprintf(str,"\1r%u%%",match);
    else if(match>=30) sprintf(str,"\1b%u%%",match);
    else if(match>=20) sprintf(str,"\1n\1g%u%%",match);
    else if(match>=10) sprintf(str,"\1n\1c%u%%",match);
    else sprintf(str,"\1n%u%%",match);
	if(smm_misc&SMM_METRIC)
		sprintf(height," %-4u",intocm(user.height));
	else
		sprintf(height,"%2u'%-2u",user.height/12,user.height%12);
	bprintf("\1n\1h%c\1n\1g%c\1h\1c%c\1b%3u \1g%3s \1m%3s\1r%s \1y%-3u "
		"\1n\1g%-25.25s\1r\1h\1i%c\1n\1h%-25.25s%s\r\n"
        ,user.sex==user.pref_sex ? 'G':user.sex=='*' ? 'B'
            : marital_ch(user.marital)
        ,race_ch(user.race)
        ,user.sex
        ,getage(user.birth)
        ,hair(user.hair)
        ,eyes(user.eyes)
		,height
		,smm_misc&SMM_METRIC ? lptokg(user.weight) : user.weight
        ,user.location
		,user.misc&USER_PHOTO ? '+':SP
        ,user.name
        ,str
        );
	if(match &&
		(!name[0] || !stricmp(name,lastname)
		|| (stricmp(user.name,lastname) && match>highmatch))) {
		highmatch=match;
        sprintf(name,"%.25s",user.name);
        strupr(name); }
	}

if(lncntr>=user_rows-2 || !user.number) {
    lncntr=0;
	bputs(PrevReadSendQuitOrMore);
	ch=getkey(K_UPPER);
	if(ch=='Q') {
		cls();
		aborted=1; }
    else if(ch=='P') {
		cls();
        records=((ftell(stream)-topdab)/sizeof(user_t));
		fseek(stream,lastdab,SEEK_SET);
		fseek(index,lastixb,SEEK_SET);
		lastdab-=(records)*sizeof(user_t);
		lastixb-=(records)*sizeof(ixb_t);
        if(lastdab<0) lastdab=0;
        if(lastixb<0) lastixb=0;
        return(-count); }
	else if(ch=='R' || ch=='G' || ch=='S') {
        bprintf("\1n\r\1>\1y\1hUser name: ");
        if(getstr(name,25,K_NOCRLF|K_LINE|K_EDIT|K_AUTODEL|K_UPPER)) {
			truncsp(name);
            name_crc=crc32(name);
            cls();
            rewind(index);
            while(!feof(index)) {
                if(!fread(&ixb,sizeof(ixb_t),1,index))
                    break;
                if(!ixb.number)    /* DELETED */
                    continue;
                if(ixb.name!=name_crc)
                    continue;
                fseek(stream
                    ,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
                    *sizeof(user_t),SEEK_SET);
                if(!fread(&user,sizeof(user_t),1,stream))
                    continue;
				if(ch=='S' && send_telegram(user))
					break;
				if(ch!='S' && !long_user_info(user))
                    break; } }
        fseek(stream,topdab,SEEK_SET);
        fseek(index,topixb,SEEK_SET);
		cls();
		return(-1); }
    else {
        lastixb=topixb;
		lastdab=topdab;
		cls(); } }

count++;
if(aborted)
    return(0);
return(1);
}


int get_your_det(user_t *user)
{
	char	str[128],*p,*hdr;
	int 	i;

while(1) {
checkline();
cls();
bputs("\1h\0014 Your Detailed Personal Information \1n  \1h\1b"
    "(Ctrl-C to Abort)\r\n\r\n");

nodesync();
bputs("\1h\1bPlease enter your name or alias: ");
strcpy(str,user->name);
if(!getstr(str,25,K_LINE|K_EDIT|K_AUTODEL|K_UPRLWR))
	return(0);
truncsp(str);
if(trash(str)) {
	bprintf("\r\n\1h\1rSorry, you can't use that name.\r\n\r\n\1p");
	continue; }
strcpy(user->name,str);

CRLF;
nodesync();
bprintf("\1h\1gPlease enter your height in %s: "
	,smm_misc&SMM_METRIC ? "centimeters" : "feet'inches (example: 5'7)");
if(user->height) {
	if(smm_misc&SMM_METRIC)
		sprintf(str,"%u",intocm(user->height));
	else
		sprintf(str,"%u'%u",user->height/12,user->height%12); }
else
    str[0]=0;
if(!getstr(str,4,K_LINE|K_EDIT|K_AUTODEL))
	return(0);
if(smm_misc&SMM_METRIC)
	user->height=cmtoin(atoi(str));
else {
	user->height=atoi(str)*12;
	p=strchr(str,'\'');
	if(p) user->height+=atoi(p+1); }

CRLF;
nodesync();
bprintf("\1h\1mPlease enter your weight (in %s): "
	,smm_misc&SMM_METRIC ? "kilograms":"pounds");
if(user->weight)
	sprintf(str,"%u"
		,smm_misc&SMM_METRIC ? lptokg(user->weight) : user->weight);
else
    str[0]=0;
if(!getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL|K_LINE))
	return(0);
if(smm_misc&SMM_METRIC)
	user->weight=kgtolp(atoi(str));
else
	user->weight=atoi(str);

CRLF;
nodesync();
bputs("\1h\1rPlease enter your annual income in dollars (ENTER=undisclosed): ");
if(user->income && user->income!=0xffffffff)
	sprintf(str,"%luK",user->income/1000);
else
    str[0]=0;
getstr(str,6,K_UPPER|K_EDIT|K_AUTODEL|K_LINE);
if(aborted)
	return(0);
if(strchr(str,'K'))
	user->income=atol(str)*1000L;
else if(str[0]==0)
	user->income=0xffffffffUL;
else
	user->income=atol(str);

CRLF;
nodesync();
bputs("\1h\1cPlease enter your location (city, state): ");
strcpy(str,user->location);
if(!getstr(str,30,K_LINE|K_EDIT|K_AUTODEL|K_UPRLWR))
	return(0);
truncsp(str);
if(trash(str)) {
	bprintf("\r\n\1r\1hSorry, you can't use that location.\r\n\r\n\1p");
	continue; }
strcpy(user->location,str);


CRLF;
nodesync();
bputs("\1h\1gPlease enter your zip/postal code: ");
if(!getstr(user->zipcode,10,K_LINE|K_EDIT|K_AUTODEL))
	return(0);

CRLF;
nodesync();
for(i=0;i<5;i++) {
    bprintf("\1n\1gPersonal text - Line %u of 5: ",i+1);
    if(wordwrap[0])
        user->note[i][0]=0;
	strcpy(str,user->note[i]);
	if(!getstr(str,50,i==4 ? K_LINE|K_EDIT:K_LINE|K_EDIT|K_WRAP))
		break;
	if(trash(str)) {
		bprintf("\r\n\1r\1hSorry, you can't use that text.\r\n\r\n");
		i--;
		continue; }
	strcpy(user->note[i],str); }
while(i++<5)
    user->note[i][0]=0;
aborted=0;
nodesync();
if(yesno("\r\nIs the above information correct"))
    break; }
return(1);
}

int get_pref_det(user_t *user)
{
	char str[128],*p,*hdr;
	int i;

while(1) {
checkline();
cls();
bputs("\1h\0014 Detailed Information on Your Preferred Mate \1n  "
    "\1h\1b(Ctrl-C to Abort)\r\n\r\n");

nodesync();
bputs("\1n\1cPlease enter the minimum age of your preferred mate: \1h");
if(user->min_age)
    sprintf(str,"%u",user->min_age);
else
    str[0]=0;
getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
if(aborted)
	return(0);
user->min_age=atoi(str);

nodesync();
bputs("\1h\1cPlease enter the maximum age of your preferred mate: \1h");
if(user->max_age)
    sprintf(str,"%u",user->max_age);
else
    str[0]=0;
getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
if(aborted)
	return(0);
user->max_age=atoi(str);
if(user->min_age) {
	strcpy(str,"Do you require your mate's age fall within this range");
	if(user->misc&USER_REQAGE)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQAGE;
	else
		user->misc&=~USER_REQAGE; }
else
	user->misc&=~USER_REQAGE;
if(aborted)
	return(0);

CRLF;
nodesync();
bprintf("\1n\1gPlease enter the minimum height of your preferred mate "
	"in %s: \1w\1h",smm_misc&SMM_METRIC ? "centimeters":"feet'inches");
if(user->min_height) {
	if(smm_misc&SMM_METRIC)
		sprintf(str,"%u",intocm(user->min_height));
	else
		sprintf(str,"%u'%u",user->min_height/12,user->min_height%12); }
else
    str[0]=0;
getstr(str,4,K_EDIT|K_AUTODEL);
if(aborted)
	return(0);
if(smm_misc&SMM_METRIC)
	user->min_height=cmtoin(atoi(str));
else {
	user->min_height=atoi(str)*12;
	p=strchr(str,'\'');
	if(p) user->min_height+=atoi(p+1); }
nodesync();
bprintf("\1h\1gPlease enter the maximum height of your preferred mate "
	"in %s: \1w",smm_misc&SMM_METRIC ? "centimeters":"feet'inches");
if(user->max_height) {
	if(smm_misc&SMM_METRIC)
		sprintf(str,"%u",intocm(user->max_height));
    else
		sprintf(str,"%u'%u",user->max_height/12,user->max_height%12); }
else
    str[0]=0;
getstr(str,4,K_EDIT|K_AUTODEL);
if(aborted)
	return(0);
if(smm_misc&SMM_METRIC)
	user->max_height=cmtoin(atoi(str));
else {
	user->max_height=atoi(str)*12;
	p=strchr(str,'\'');
	if(p) user->max_height+=atoi(p+1); }
strcpy(str,"Do you require your mate's height fall within this range");
if(user->misc&USER_REQHEIGHT)
	i=yesno(str);
else
	i=!noyes(str);
if(i)
    user->misc|=USER_REQHEIGHT;
else
    user->misc&=~USER_REQHEIGHT;
if(aborted)
    return(0);


CRLF;
nodesync();
bprintf("\1n\1mPlease enter the minimum weight of your preferred mate in %s:"
	" \1w\1h",smm_misc&SMM_METRIC ? "kilograms":"pounds");
if(user->min_weight) {
	if(smm_misc&SMM_METRIC)
		sprintf(str,"%u",lptokg(user->min_weight));
	else
		sprintf(str,"%u",user->min_weight); }
else
    str[0]=0;
getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
if(aborted)
	return(0);
if(smm_misc&SMM_METRIC)
	user->min_weight=kgtolp(atoi(str));
else
	user->min_weight=atoi(str);

nodesync();
bprintf("\1h\1mPlease enter the maximum weight of your preferred mate in %s:"
	" \1w",smm_misc&SMM_METRIC ? "kilograms":"pounds");
if(user->max_weight) {
	if(smm_misc&SMM_METRIC)
		sprintf(str,"%u",lptokg(user->max_weight));
	else
		sprintf(str,"%u",user->max_weight); }
else
    str[0]=0;
getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
if(aborted)
	return(0);
if(smm_misc&SMM_METRIC)
	user->max_weight=kgtolp(atoi(str));
else
	user->max_weight=atoi(str);
if(user->max_weight) {
	strcpy(str,"Do you require your mate's weight fall within this range");
	if(user->misc&USER_REQWEIGHT)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQWEIGHT;
	else
		user->misc&=~USER_REQWEIGHT; }
else
	user->misc&=~USER_REQWEIGHT;
if(aborted)
    return(0);


CRLF;
nodesync();
bputs("\1h\1cPlease enter the lowest zip/postal code of your preferred "
    "mate: \1w");
getstr(user->min_zipcode,10,K_EDIT|K_AUTODEL|K_UPPER);
if(aborted)
	return(0);
bputs("\1h\1bPlease enter the highest zip/postal code of your preferred "
    "mate: \1w");
getstr(user->max_zipcode,10,K_EDIT|K_AUTODEL|K_UPPER);
if(aborted)
	return(0);
strcpy(str,"Do you require your mate's zip/postal code fall within this range");
if(user->misc&USER_REQZIP)
	i=yesno(str);
else
	i=!noyes(str);
if(i)
	user->misc|=USER_REQZIP;
else
	user->misc&=~USER_REQZIP;
if(aborted)
    return(0);


CRLF;
nodesync();
bputs("\1h\1gPlease enter the minimum annual income of your "
    "mate in dollars: \1w");
if(user->min_income)
    sprintf(str,"%lu",user->min_income);
else
    str[0]=0;
getstr(str,6,K_NUMBER|K_EDIT|K_AUTODEL);
if(aborted) return(0);
user->min_income=atol(str);

nodesync();
bputs("\1h\1yPlease enter the maximum annual income of your "
    "mate in dollars: \1w");
if(user->max_income)
    sprintf(str,"%lu",user->max_income);
else
    str[0]=0;
getstr(str,6,K_NUMBER|K_EDIT|K_AUTODEL);
if(aborted) return(0);
user->max_income=atol(str);
if(user->min_income) {
	strcpy(str,"Do you require that your mate's income fall within this range");
	if(user->misc&USER_REQINCOME)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQINCOME;
	else
		user->misc&=~USER_REQINCOME; }
else
	user->misc&=~USER_REQINCOME;
if(aborted)
    return(0);


nodesync();
if(yesno("\r\nIs the above information correct"))
    break; }
return(1);
}

int get_your_multi(user_t *user)
{
	char str[128],*hdr,*p;

while(1) {
checkline();
hdr="\1n\1l\1h\0014 Your Profile \1n  \1h\1b(Ctrl-C to Abort)\r\n\r\n";
bputs(hdr);
bputs("\1n\1h\1cYour Marital Status:\r\n\r\n");
mnemonics("~A) Single\r\n~B) Married\r\n~C) Divorced\r\n~D) Widowed\r\n"
    "~E) Other\r\n");
nodesync();
bputs("\r\n\1y\1hWhich: \1w");
uchar2ans(user->marital,str);
if(!getstr(str,1,K_ALPHA|K_LINE|K_EDIT|K_AUTODEL|K_UPPER))
	return(0);
if(str[0]<='E')
	break; }
user->marital=ans2uchar(str);

while(1) {
checkline();
bputs(hdr);
bputs("\1n\1h\1cYour Race:\r\n\r\n");
mnemonics("~A) White\r\n~B) Black\r\n~C) Hispanic\r\n~D) Asian\r\n"
	"~E) American Indian\r\n~F) Middle Eastern\r\n~G) Other\r\n");
nodesync();
bputs("\r\n\1y\1hWhich: \1w");
uchar2ans(user->race,str);
if(!getstr(str,1,K_ALPHA|K_LINE|K_EDIT|K_AUTODEL|K_UPPER))
	return(0);
if(str[0]<='G')
    break; }
user->race=ans2uchar(str);

while(1) {
checkline();
bputs(hdr);
bputs("\1n\1h\1cYour Hair Color:\r\n\r\n");
mnemonics("~A) Blonde\r\n~B) Brown\r\n~C) Red\r\n~D) Black\r\n"
    "~E) Grey\r\n~F) Other\r\n");
nodesync();
bputs("\r\n\1y\1hWhich: \1w");
uchar2ans(user->hair,str);
if(!getstr(str,1,K_ALPHA|K_LINE|K_EDIT|K_AUTODEL|K_UPPER))
	return(0);
if(str[0]<='F')
    break; }
user->hair=ans2uchar(str);

while(1) {
checkline();
bputs(hdr);
bputs("\1n\1h\1cYour Eye Color:\r\n\r\n");
mnemonics("~A) Blue\r\n~B) Green\r\n~C) Hazel\r\n~D) Brown\r\n"
    "~E) Other\r\n");
nodesync();
bputs("\r\n\1y\1hWhich: \1w");
uchar2ans(user->eyes,str);
if(!getstr(str,1,K_ALPHA|K_LINE|K_EDIT|K_AUTODEL|K_UPPER))
	return(0);
if(str[0]<='E')
    break; }
user->eyes=ans2uchar(str);
return(1);
}

int get_pref_multi(user_t *user)
{
	char str[128],*hdr,*p;
	int i;

while(1) {
checkline();
hdr="\1n\1l\1h\0014 Profile of Your Preferred Mate \1n  \1h\1b"
    "(Ctrl-C to Abort)\r\n\r\n";
bputs(hdr);
bputs("\1n\1h\1cSex of Your Preferred (Intimate/Romantic) Mate:\r\n\r\n");
mnemonics("~M) Male\r\n~F) Female\r\n~*) Either\r\n");
nodesync();
bputs("\r\n\1y\1hWhich: ");
str[0]=user->pref_sex;
str[1]=0;
if(!getstr(str,1,K_UPPER|K_EDIT|K_AUTODEL|K_LINE))
    break;
if(user->sex==str[0] && noyes("\r\nYou are homosexual"))
	continue;
if(str[0]=='M' || str[0]=='F' || str[0]=='*')
    break; }
user->pref_sex=str[0];
if(aborted) return(0);

strcpy(str,"\r\nAre you seeking an any-sex (non-romantic) friendship");
if(user->misc&USER_FRIEND)
	i=yesno(str);
else
	i=!noyes(str);
if(i)
	user->misc|=USER_FRIEND;
else
	user->misc&=~USER_FRIEND;

bputs(hdr);
bputs("\1n\1h\1cMarital Status of Your Preferred Mate:\r\n\r\n");
mnemonics("~A) Single\r\n~B) Married\r\n~C) Divorced\r\n~D) Widowed\r\n"
    "~E) Other\r\n~*) Any of the above\r\n");
nodesync();
bputs("\r\n\1y\1hPlease enter up to 4 answers: ");
uchar2ans(user->pref_marital,str);
if(!getstr(str,4,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
	return(0);
user->pref_marital=ans2uchar(str);
if(user->pref_marital!=0xff) {
	strcpy(str,"\r\nDo you require your mate have the martial status you "
		"indicated");
	if(user->misc&USER_REQMARITAL)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
        user->misc|=USER_REQMARITAL;
	else
		user->misc&=~USER_REQMARITAL; }
else
	user->misc&=~USER_REQMARITAL;
if(aborted) return(0);

bputs(hdr);
bputs("\1n\1h\1cRace of Your Preferred Mate:\r\n\r\n");
mnemonics("~A) White\r\n~B) Black\r\n~C) Hispanic\r\n~D) Asian\r\n"
    "~E) American Indian\r\n~F) Middle Eastern\r\n~G) Other\r\n"
    "~*) Any of the above\r\n");
nodesync();
bputs("\r\n\1y\1hPlease enter up to 6 answers: ");
uchar2ans(user->pref_race,str);
if(!getstr(str,6,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
	return(0);
user->pref_race=ans2uchar(str);
if(user->pref_race!=0xff) {
	strcpy(str,"\r\nDo you require your mate be of the race"
		" you indicated");
	if(user->misc&USER_REQRACE)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQRACE;
	else
		user->misc&=~USER_REQRACE; }
else
	user->misc&=~USER_REQRACE;
if(aborted) return(0);

bputs(hdr);
bputs("\1n\1h\1cHair Color of Your Preferred Mate:\r\n\r\n");
mnemonics("~A) Blonde\r\n~B) Brown\r\n~C) Red\r\n~D) Black\r\n"
    "~E) Grey\r\n~F) Other\r\n~*) Any of the Above\r\n");
nodesync();
bputs("\r\n\1y\1hPlease enter up to 5 answers: ");
uchar2ans(user->pref_hair,str);
if(!getstr(str,5,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
	return(0);
user->pref_hair=ans2uchar(str);
if(user->pref_hair!=0xff) {
	strcpy(str,"\r\nDo you require your mate have the hair color"
		" you indicated");
	if(user->misc&USER_REQHAIR)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQHAIR;
	else
		user->misc&=~USER_REQHAIR; }
else
	user->misc&=~USER_REQHAIR;
if(aborted) return(0);


bputs(hdr);
bputs("\1n\1h\1cEye Color of Your Preferred Mate:\r\n\r\n");
mnemonics("~A) Blue\r\n~B) Green\r\n~C) Hazel\r\n~D) Brown\r\n"
    "~E) Other\r\n~*) Any of the above\r\n");
nodesync();
bputs("\r\n\1y\1hPlease enter up to 4 answers: ");
uchar2ans(user->pref_eyes,str);
if(!getstr(str,4,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
	return(0);
user->pref_eyes=ans2uchar(str);
if(user->pref_eyes!=0xff) {
	strcpy(str,"\r\nDo you require your mate have the eye color"
		" you indicated");
	if(user->misc&USER_REQEYES)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQEYES;
	else
		user->misc&=~USER_REQEYES; }
else
	user->misc&=~USER_REQEYES;
if(aborted) return(0);

bputs(hdr);
bputs("\1n\1h\1cZodiac Sign of Your Preferred Mate:\r\n\r\n");
mnemonics("~A) Aries\r\n~B) Taurus\r\n~C) Gemini\r\n~D) Cancer\r\n"
    "~E) Leo\r\n~F) Virgo\r\n~G) Libra\r\n~H) Scorpio\r\n");
mnemonics("~I) Sagittarius\r\n~J) Capricorn\r\n~K) Aquarius\r\n"
    "~L) Pisces\r\n~*) Any of the above\r\n");
nodesync();
bputs("\r\n\1y\1hPlease enter up to 11 answers: ");
bits2ans(user->pref_zodiac,str);
if(!getstr(str,11,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
	return(0);
user->pref_zodiac=ans2bits(str);
if(user->pref_zodiac!=0xffff) {
	strcpy(str,"\r\nDo you require your mate have the zodiac sign"
		" you indicated");
	if(user->misc&USER_REQZODIAC)
		i=yesno(str);
	else
		i=!noyes(str);
	if(i)
		user->misc|=USER_REQZODIAC;
	else
		user->misc&=~USER_REQZODIAC; }
else
	user->misc&=~USER_REQZODIAC;
if(aborted) return(0);

return(1);
}

void mbtype_desc(char *mbtype)
{
	char str[128],type[128];

while(1) {
	checkline();
	cls();
	printfile("MB-TYPE.ASC");
	nodesync();
	bprintf("\r\n\1y\1hYour Type: \1w%s\r\n",mbtype);
	bputs("\1y\1hLetter or Type to define: ");
	memset(type,0,5);
	if(!getstr(type,4,K_ALPHA|K_UPPER|K_LINE))
		break;
	sprintf(str,"MB-%s.ASC",type);
	if(!fexist(str)) {
		bputs("\r\nInvalid Letter or Type!\r\n\r\n");
		pause();
		continue; }
	cls();
	printfile(str);
	if(lncntr && !aborted)
		pause(); }
}


int get_mbtype(user_t *user)
{
	char str[256];
	int file,i=0,e=0,n=0,s=0,f=0,t=0,p=0,j=0,q=0,ch;
	FILE *stream;

aborted=0;
cls();
printfile("MB-INTRO.ASC");
if(aborted)
	return(0);
bputs("\1n\1hYour answers will be kept secret.\r\n\r\n"
	"Only your personality type will be visible to other users.\r\n\r\n");
nodesync();
if(!yesno("Continue with test"))
    return(0);

if((file=nopen("MB-TYPE.QUE",O_RDONLY))==-1 ||
	(stream=fdopen(file,"r"))==NULL) {
	bputs("\7\r\i\hCan't open MBTYPE.QUE\r\n\1p");
	return(0); }
while(!feof(stream)) {
	if(!fgets(str,200,stream))
        break;
	str[57]=0;
	truncsp(str);
	if(!str[0])
		break;
	cls();
	nodesync();
	bprintf("\1y\1hMyers-Briggs Personality Test  \1bQuestion #\1w%u  "
		"\1b(Ctrl-C to Abort)\r\n"
		"\r\n%s:\r\n\r\n"
		"[\1wA\1b] %.44s\r\n"
		"[\1wB\1b] %.44s\r\n"
		"[\1wC\1b] cannot decide\r\n\r\n"
		"Which: \1w"
		,q+1,str,str+58,str+102);
	ch=getkeys("ABC",0);
	if(aborted) {
		fclose(stream);
		return(0); }
	if(ch=='C') {
		q++;
		continue; }

	if(ch=='A')
		ch=str[146];
	else
		ch=str[147];

	switch(ch) {
		case 'E':
			e++;
			break;
		case 'I':
			i++;
			break;
		case 'S':
			s++;
			break;
		case 'N':
			n++;
			break;
		case 'T':
			t++;
			break;
		case 'F':
			f++;
			break;
		case 'J':
			j++;
			break;
		case 'P':
			p++;
			break; }
	q++; }

fclose(stream);

if(i>e)
	user->mbtype[0]='I';
else if(i==e)
	user->mbtype[0]='*';
else
	user->mbtype[0]='E';

if(s>n)
	user->mbtype[1]='S';
else if(s==n)
	user->mbtype[1]='*';
else
	user->mbtype[1]='N';

if(t>f)
	user->mbtype[2]='T';
else if(t==f)
	user->mbtype[2]='*';
else
	user->mbtype[2]='F';

if(p>j)
	user->mbtype[3]='P';
else if(p==j)
	user->mbtype[3]='*';
else
	user->mbtype[3]='J';

bprintf("\1l\1y\1hYour Myers-Briggs Personality Type is \1w%s\r\n\r\n"
	,user->mbtype);

if(yesno("Would you like to see the personality type descriptions"))
	mbtype_desc(user->mbtype);
return(1);
}


int get_purity(user_t *user)
{
	char str[128];
	int file,no_ans=0,pos_ans=0;
	FILE *stream;

aborted=0;
if(!fexist("PURITY.QUE") || purity_age>getage(user->birth))
	return(0);
cls();
printfile("PURITY.ASC");
if(aborted)
	return(0);
bputs("\1n\1hYour answers will be kept secret.\r\n\r\n"
	"Only your purity score will be visible to other users.\r\n\r\n");
nodesync();
if(!yesno("Continue with test"))
	return(0);
if((file=nopen("PURITY.QUE",O_RDONLY))==-1 ||
	(stream=fdopen(file,"r"))==NULL) {
	bputs("\7\r\i\hCan't open PURITY.QUE\r\n\1p");
	return(0); }
while(!feof(stream)) {
	if(!fgets(str,81,stream))
		break;
	truncsp(str);
	cls();
	if(str[0]=='*') {
		printfile(str+1);
		if(lncntr && !aborted)
			pause();
		if(aborted)
			break;
		continue; }
	nodesync();
	bprintf("\1y\1hPurity Test  \1bQuestion #\1w%u  \1b(Ctrl-C to Abort)\r\n"
		"\r\nHave you ever:\r\n\r\n"
		,++pos_ans);
	if(noyes(str))
		no_ans++;
	if(aborted)
		break; }

fclose(stream);
if(aborted)
	return(0);

user->purity=((float)no_ans/pos_ans)*100.0;
bprintf("\1l\1y\1hCongratulations, you are \1w%u%%\1y pure!\r\n\r\n"
	,user->purity);
pause();
return(1);
}

int get_que(int i, user_t *inuser)
{
	char str[128];
	int j,k,x,y,u;
	user_t user=*inuser;

if(que[i]->req_age>getage(inuser->birth))
	return(0);
for(j=0;j<que[i]->total_ques && !aborted;j++) {

	u=user_quenum(que[i]->name,user);
	if(u==-1) { 			/* not already answered */
		for(u=0;u<5;u++)	/* search for unused questionnaire slot */
			if(sys_quenum(user.queans[u].name)==-1)
				break;
		if(u==5) {		/* All questionnaire slots used!?? */
			bputs("\r\n\7Questionnaire error, inform sysop!\r\n");
			pause();
			return(0); } }
	strcpy(user.queans[u].name,que[i]->name);
	cls();
	bprintf("\1n\1m\1hQuestionnaire: \1y%-25s \1mQuestion: "
		"\1y%d \1mof \1y%d  \1h\1b(Ctrl-C to Abort)\r\n\r\n"
		,que[i]->desc,j+1,que[i]->total_ques);
	bprintf("\1w\1h%s\r\n\r\n",que[i]->que[j].txt);
	for(k=0;k<que[i]->que[j].answers;k++)
		bprintf("\1h\1b%c\1w) \1g%s\r\n",'A'+k,que[i]->que[j].ans[k]);
	nodesync();
	bprintf("\1n\1g\r\nChoose %s%d answer%s to describe "
		"\1hyourself\1n\1g%s: "
		,que[i]->que[j].allowed > 1 ? "up to ":""
		,que[i]->que[j].allowed,que[i]->que[j].allowed > 1 ? "s":""
		,j ? " or \1h-\1n\1g to go back" : "");
	bits2ans(user.queans[u].self[j],str);
	getstr(str,que[i]->que[j].allowed,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
	if(aborted)
		return(0);
	truncsp(str);
	if(!str[0]) {
		j--;
		continue; }
	if(str[0]=='-') {
		if(j) j-=2;
		else j--;
		continue; }
	y=strlen(str);
	for(x=0;x<y;x++)
		if(!isalpha(str[x]) || str[x]-'A'>que[i]->que[j].answers-1)
			break;
	if(x<y) {
		j--;
		continue; }
	user.queans[u].self[j]=ans2bits(str);
	nodesync();
	bprintf("\1n\1g\r\n%s%d answer%s to describe your \1hpreferred mate"
		"\1n\1g or \1h*\1n\1g for any: "
		,que[i]->que[j].answers > 1 ? "Up to ":""
		,que[i]->que[j].answers,que[i]->que[j].answers > 1 ? "s":"");
	if(user.queans[u].pref[j])
		bits2ans(user.queans[u].pref[j],str);
	getstr(str,que[i]->que[j].answers,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
	if(aborted)
		return(0);
	truncsp(str);
	if(!str[0]) {
		j--;
		continue; }
	y=strlen(str);
	for(x=0;x<y;x++)
		if(isalpha(str[x]) && str[x]-'A'>que[i]->que[j].answers-1)
			break;
	if(x<y) {
		j--;
		continue; }
	user.queans[u].pref[j]=ans2bits(str); }
*inuser=user;
return(1);
}

/* Gets/updates profile from user. Returns 0 if aborted. */

char get_user_info(user_t *user)
{
	char str[128],*p,*hdr;
	int i,j,k,x,y;

while(1) {
	checkline();
	timeleft=0xffff;
	aborted=0;
	main_user_info(*user);
	CRLF;
	bputs("\1n\1h");
	bputs("\1b[\1wA\1b] Your name, height, weight, income, location, and "
			"text\r\n"
		  "\1b[\1wB\1b] Your marital status, race, hair color, and eye "
			"color\r\n"
		  "\1b[\1wC\1b] Preferred age, height, weight, location, and income\r\n"
		  "\1b[\1wD\1b] Preferred sex, marital status, race, hair color, eye "
			"color, and zodiac\r\n"
		  );
	for(i=0;i<total_ques;i++) {
		if(que[i]->req_age>getage(user->birth))
			continue;
		bprintf("\1b[\1w%u\1b] \1w%s \1bquestionnaire\1b (%u questions) \1w%s\r\n"
			,i+1,que[i]->desc,que[i]->total_ques
			,user_quenum(que[i]->name,*user)==-1 ? "[Unanswered]":"[Answered]");
			}
	bprintf("\1b[\1wM\1b] Myers-Briggs personality test (70 questions) "
		"\1w[%s]\r\n",user->mbtype[0] ? user->mbtype : "Unanswered");
	if(fexist("PURITY.QUE") && purity_age<=getage(user->birth))
		bprintf("\1b[\1wP\1b] Purity test \1w(%u%% pure)\r\n"
			,user->purity);
	bputs("\r\n\1bWhich or [\1wQ\1b]uit: \1w");
	i=getkeys("ABCDMPQ",total_ques);
	if(i&0x8000) {
		i&=~0x8000;
		i--;
		get_que(i,user);
		continue; }

	switch(i) {
		case 'A':
			get_your_det(user);
			break;
		case 'C':
			get_pref_det(user);
			break;
		case 'B':
			get_your_multi(user);
			break;
		case 'D':
			get_pref_multi(user);
			break;
		case 'M':
			get_mbtype(user);
			break;
		case 'P':
			get_purity(user);
			break;
		case 'Q':
			return(1); } }
}

void write_user()
{
    char str[256];

if(auto_update && time(NULL)-useron.updated>(long)auto_update*24L*60L*60L)
	useron.updated=time(NULL);
fseek(stream,useron_record*sizeof(user_t),SEEK_SET);
fwrite(&useron,sizeof(user_t),1,stream);
fflush(stream);

ixb.updated=useron.updated;
if(useron.misc&USER_DELETED)
    ixb.number=0;
else
    ixb.number=useron.number;
strcpy(str,useron.name);
strupr(str);
ixb.name=crc32(str);
ixb.system=system_crc;
fseek(index,useron_record*sizeof(ixb_t),SEEK_SET);
fwrite(&ixb,sizeof(ixb_t),1,index);
fflush(index);
if(!ixb.number) {
	sprintf(str,"%04u.MSG",useron.number);
	remove(str); }
}


void add_userinfo()
{
	static user_t user;
	char str[128];
	ushort tleft=timeleft;
	int i;

if(!cdt_warning(profile_cdt))
	return;
timeleft=0xffff;
user.number=user_number;
user.sex=user_sex;
strcpy(user.name,user_name);
strcpy(user.realname,user_name);
strcpy(user.system,system_name);
strcpy(user.birth,user_birth);
strcpy(user.zipcode,user_zipcode);
sprintf(user.min_zipcode,"%c0000",user_zipcode[0]);
sprintf(user.max_zipcode,"%c9999",user_zipcode[0]);
strcpy(user.location,user_location);
if(user.sex!='M' && user.sex!='F') {
	if(yesno("Are you of male gender"))
		user.sex='M';
	else
		user.sex='F'; }
if(!getage(user.birth))
	while(1) {
		checkline();
		cls();
		bputs("\1l\1y\1hYour birthdate (MM/DD/YY): ");
		if(!getstr(user.birth,8,K_UPPER|K_LINE))
			return;
		if(getage(user.birth))
			break; }

user.pref_sex=user.sex=='M' ? 'F' : 'M';
user.created=time(NULL);
if(!get_your_det(&user) || !get_your_multi(&user) || !get_pref_multi(&user)
	|| !get_pref_det(&user)) {
	timeleft=tleft;
	return; }
for(i=0;i<total_ques;i++) {
	aborted=0;
	if(que[i]->req_age>getage(user.birth))
		continue;
	timeleft=0xffff;
	cls();
	bprintf("\1w\1h%s Questionnaire: \1y%u questions\r\n\r\n"
		"\1cAnswers will be viewable by other users.\r\n\r\n"
		,que[i]->desc,que[i]->total_ques);
	if(yesno("Continue with questionnaire"))
		get_que(i,&user); }
timeleft=0xffff;
get_mbtype(&user);
timeleft=0xffff;
get_purity(&user);
if(!get_user_info(&user)) {
	timeleft=tleft;
	return; }
user.updated=time(NULL);
useron=user;
bputs("\1n\1h\r\nSaving...");
rewind(index);
while(!feof(index)) {
	if(!fread(&ixb,sizeof(ixb_t),1,index))
		break;
	if(!ixb.number) {	  /* Deleted User */
		fseek(index,ftell(index)-sizeof(ixb_t),SEEK_SET);
		break; } }

useron_record=ftell(index)/sizeof(ixb_t);

write_user();
adjust_cdt(profile_cdt);

if(notify_user && notify_user!=user_number) {
	sprintf(str,"\1n\1hSMM: \1y%s \1madded %s profile to the Match Maker.\r\n"
		,user_name,user_sex=='M' ? "his":"her");
	if(node_dir[0])
		putsmsg(notify_user,str);
	else
		puttgram(notify_user,str); }

timeleft=tleft;
}

void delphoto(user_t user)
{
	char fname[64],path[128];
	int i;
	struct ffblk ff;

if(!(user.misc&USER_PHOTO))
	return;
for(i=0;user.system[i];i++)
	if(isalnum(user.system[i]))
        break;
if(!user.system[i])
	fname[0]='~';
else
	fname[0]=user.system[i];
for(i=strlen(user.system)-1;i>0;i--)
	if(isalnum(user.system[i]))
		break;
if(i<=0)
	fname[1]='~';
else
	fname[1]=user.system[i];
fname[2]=0;
strupr(user.system);
strcat(fname,base41(crc16(user.system),tmp));
strcat(fname,base41(user.number,tmp));
strcat(fname,".*");
strupr(fname);
sprintf(path,"PHOTO\\%s",fname);
i=findfirst(path,&ff,0);
if(i)
	return;
sprintf(path,"PHOTO\\%s",ff.ff_name);
if(remove(path))
	bprintf("\1r\1h\7%s couldn't be removed!\1n\r\n",path);
else
	bprintf("\1r\1hPhoto removed.\r\n");
}


void smm_exit()
{
	char str[128];
	int i;
	FILE *in,*out;

if(io_int) {
	io_int=0;
	ivhctl(0); }
if(com_port) {
	for(i=0;i<5;i++)
		if(!rioctl(TXBC))		/* wait for rest of output */
			break;
		else
			mswait(1000);
    rioini(0,0); }
if(useron.number) {
	useron.lastin=time(NULL);
	write_user(); }
if(stream)
	fclose(stream);
if(cdt_adjustment) {
	if(node_dir[0]) {
		sprintf(str,"%sMODUSER.DAT",node_dir);
		if((out=fopen(str,"wt"))==NULL) {
			bprintf("Error opening %s for write\r\n",str);
			return; }
		fprintf(out,"%ld",cdt_adjustment);
		fclose(out); }
	else {							/* Write back credits to DOOR.SYS */
		strcpy(str,door_sys);
		str[strlen(str)-1]='_';
		remove(str);
		rename(door_sys,str);
		if((in=fopen(str,"rb"))==NULL) {
			bprintf("Error opening %s for read\r\n",str);
            return; }
		if((out=fopen(door_sys,"wb"))==NULL) {
			bprintf("Error opening %s for write\r\n",door_sys);
			return; }
		for(i=0;!feof(in);i++) {
			if(!fgets(tmp,128,in))
				break;
			if(i+1==30)
				fprintf(out,"%ld\r\n",-(cdt_adjustment/1024L));
			else if(i+1==31)
				fprintf(out,"%ld\r\n",(user_cdt+cdt_adjustment)/1024L);
			else
				fprintf(out,"%s",tmp); }
		fclose(in);
		fclose(out);
		remove(str);
		} }
}

time_t checktime()
{
	struct tm tm;

memset(&tm,0,sizeof(tm));
tm.tm_year=94;
tm.tm_mday=1;
return(mktime(&tm)^0x2D24BD00L);
}

void statusline(void)
{
	int col,row;

col=lclwx();
row=lclwy();
lclxy(1,node_scrnlen);
lclatr(CYAN|HIGH|(BLUE<<4));
lputs("  ");
lprintf("%-25.25s %02d %-25.25s  %02d %c %s"
    ,user_name,user_level,user_realname[0] ? user_realname : user_location
    ,getage(user_birth)
    ,user_sex ? user_sex : SP
    ,user_phone);
lputc(CLREOL);
lclatr(LIGHTGRAY);
lclxy(col,row);
}


int minor_protection(user_t user)
{
if(!user_number || !age_split || SYSOP)
	return(0);
if(getage(user_birth)<age_split && getage(user.birth)>=age_split)
	return(1);
if(getage(user_birth)>=age_split && getage(user.birth)<age_split)
	return(1);
return(0);
}

void main(int argc, char **argv)
{
	char	str[512],name[128],desc[128],gotoname[128],*p,ext,ch;
	int 	i,j,k,file,match,found;
	uint	base=0xffff;
	ushort	tleft;
	long	l,offset;
	ulong	name_crc,sys_crc,*crc_lst,ul;
	FILE	*fp,*que_lst;
	user_t	user;
	wall_t	wall;
	struct	ffblk ff;

nodefile=-1;
node_misc=NM_LOWPRIO;
com_port=-1;
com_base=0;
com_irq=com_rate=0;
node_dir[0]=exec_dir[0]=temp_dir[0]=ctrl_dir[0]=door_sys[0]=system_name[0]=0;
sys_name[0]=0;

p=getenv("SBBSNODE");
if(p) {
	strcpy(node_dir,p);
	if(node_dir[strlen(node_dir)-1]!='\\')
		strcat(node_dir,"\\");
	initdata(); }

gotoname[0]=0;
for(i=1;i<argc;i++) {
	if(argv[i][0]=='/') {
		switch(toupper(argv[i][1])) {
			case 'P':
				com_port=atoi(argv[i]+2);
				break;
			case 'I':
				com_irq=atoi(argv[i]+2);
				break;
			case 'C':
				com_base=ahtoul(argv[i]+2);
				break;
			case 'R':
				com_rate=atol(argv[i]+2);
				break;
			case 'T':
				mswtyp=atoi(argv[i]+2);
				break;
			case 'N':
				node_dir[0]=0;
				break;
			default:
				printf("\nusage: SMM [DOOR.SYS] [/option] [/option] [...] [user name]\n");
				printf("\n");
				printf("where:\n");
				printf("       DOOR.SYS is the path and filename of DOOR.SYS\n");
				printf("\n");
				printf("       /p# sets com port\n");
				printf("       /i# sets com irq\n");
				printf("       /c# sets com I/O address (or DIGI or FOSSIL)\n");
				printf("       /r# sets com rate\n");
				printf("       /t# sets time-slice API support\n");
				printf("\n");
				printf("       user name (if specified) is user to look-up\n");
				exit(1); }
		continue; }
	if(!node_dir[0] && !door_sys[0]) {
		strcpy(door_sys,argv[i]);
		continue; }
	if(gotoname[0])
		strcat(gotoname," ");
	strcat(gotoname,argv[i]); }

if(!node_dir[0] && !door_sys[0]) {
	printf("\n\7SBBSNODE environment variable not set and DOOR.SYS not "
		"specified.\n");
	exit(1); }

if(door_sys[0]) {
	#ifdef __TURBOC__
		ctrlbrk(cbreakh);
	#endif

	#ifdef __WATCOMC__
		putenv("TZ=UCT0");
		setvbuf(stdout,NULL,_IONBF,0);
		setvbuf(stderr,NULL,_IONBF,0);
	#endif

	if(setmode(fileno(stderr),O_BINARY)==-1) {	 /* eliminate LF expansion */
		printf("\n\7Can't set stderr to BINARY\n");
		exit(1); }

	starttime=time(NULL);			/* initialize start time stamp */
	wordwrap[0]=0;					/* set wordwrap to null */
	attr(LIGHTGRAY);				/* initialize color and curatr to plain */
	mnehigh=LIGHTGRAY|HIGH; 		/* mnemonics highlight color */
	mnelow=GREEN;					/* mnemonics normal text color */
	sec_warn=180;					/* seconds till inactivity warning */
	sec_timeout=300;				/* seconds till inactivity timeout */
	tos=lncntr=0;					/* init topofscreen and linecounter to 0 */
	lastnodemsg=0;					/* Last node to send message to */
	aborted=0;						/* Ctrl-C hit flag */

	fp=fopen(door_sys,"rb");
	if(!fp) {
		printf("\n\7ERROR opening %s\n",door_sys);
		exit(2); }
	user_misc=user_flags2[0]=user_flags3[0]=user_flags4[0]=0;
	user_rest[0]=user_exempt[0]=0;
	user_sex=user_address[0]=user_zipcode[0]=user_realname[0]=0;
	str[0]=0;
	fgets(str,81,fp);				// 01: COM port - if local
	if(com_port==-1)
		com_port=atoi(str+3);
	str[0]=0;
	fgets(str,81,fp);				// 02: DCE Rate
	user_dce=atoi(str);
	fgets(str,81,fp);				// 03: Data bits
	str[0]=0;
	fgets(str,81,fp);				// 04: Node num
	node_num=atoi(str);
	str[0]=0;
	fgets(str,81,fp);				// 05: DTE rate
	if(!com_rate)
		com_rate=atol(str);
	fgets(str,81,fp);				// 06: Screen display
	fgets(str,81,fp);				// 07: Printer toggle
	fgets(str,81,fp);				// 08: Page bell
	fgets(str,81,fp);				// 09: Caller alarm
	str[0]=0;
	fgets(str,81,fp);				// 10: User name
	sprintf(user_name,"%.25s",str);
	truncsp(user_name);
	str[0]=0;
	fgets(str,81,fp);				// 11: User location
	sprintf(user_location,"%.30s",str);
	truncsp(user_location);
	str[0]=0;
	fgets(str,81,fp);				// 12: Home phone
	sprintf(user_phone,"%.12s",str);
	truncsp(user_phone);
	fgets(str,81,fp);				// 13: Work phone
	fgets(str,81,fp);				// 14: Password
	str[0]=0;
	fgets(str,81,fp);				// 15: Security Level
	user_level=atoi(str);
	fgets(str,81,fp);				// 16: Total logons
	fgets(str,81,fp);				// 17: Last on date
	str[0]=0;
	fgets(str,81,fp);				// 18: Time left in seconds
	timeleft=atoi(str);
	fgets(str,81,fp);				// 19: Time left in minutes
	str[0]=0;
	fgets(str,81,fp);				// 20: Graphics
	if(!strnicmp(str,"GR",2))
		user_misc|=(COLOR|ANSI);
	str[0]=0;
	fgets(str,81,fp);				// 21: Screen length
	user_rows=atoi(str);
	if(user_rows<10)
		user_rows=24;
	fgets(str,81,fp);				// 22: Expert?
	str[0]=0;
	fgets(str,81,fp);				// 23: Registered conferences
	sprintf(user_flags1,"%.26s",str);
	truncsp(user_flags1);
	fgets(str,81,fp);				// 24: Conference came from
	str[0]=0;
	fgets(str,81,fp);				// 25: User's expiration date
	user_expire=dstrtounix(str);
	str[0]=0;
	fgets(str,81,fp);				// 26: User's number
	user_number=atoi(str);
	fgets(str,81,fp);				// 27: Default protocol
	fgets(str,81,fp);				// 28: Total uploads
	fgets(str,81,fp);				// 29: Total downloads
	fgets(str,81,fp);				// 30: Kbytes downloaded today
	str[0]=0;
	fgets(str,81,fp);				// 31: Max Kbytes to download today
	user_cdt=atol(str)*1024UL;
	str[0]=0;
	fgets(str,81,fp);				// 32: Birthday
	truncsp(str);
	sprintf(user_birth,"%.8s",str);
	fgets(str,81,fp);				// 33: Path to MAIN
	fgets(str,81,fp);				// 34: Path to GEN
	str[0]=0;
	fgets(str,81,fp);				 // 35: Sysop's name
	sprintf(sys_op,"%.40s",str);
	truncsp(sys_op);
	fclose(fp);
	con_fp=stderr;
	if(setmode(fileno(con_fp),O_BINARY)==-1) {	 /* eliminate LF expansion */
		printf("Can't set console output to BINARY\n");
		exit(1); }
	}

if(!user_number) {
	printf("\7\nERROR: Invalid user number (%u)\n",user_number);
	exit(5); }

if(*(&riobp-1)!=23) {
    printf("Wrong rciol.obj\n");
    exit(1); }

node_scrnlen=lclini(0xd<<8);	  /* Tab expansion, no CRLF expansion */
lclini(node_scrnlen-1);

if(com_port) {
	lprintf("\r\nInitializing COM port %u: ",com_port);
	switch(com_base) {
		case 0xb:
			lputs("PC BIOS");
			rioctl(I14PC);
			if(!com_irq) com_irq=com_port-1;
			break;
		case 0xffff:
		case 0xd:
			lputs("DigiBoard");
			rioctl(I14DB);
			if(!com_irq) com_irq=com_port-1;
			break;
		case 0xe:
			lputs("PS/2 BIOS");
			rioctl(I14PS);
			if(!com_irq) com_irq=com_port-1;
			break;
		case 0xf:
			lputs("FOSSIL");
			rioctl(I14FO);
			if(!com_irq) com_irq=com_port-1;
			break;
		case 0:
			base=com_port;
			lputs("UART I/O (BIOS), ");
			if(com_irq)
				lprintf("IRQ %d",com_irq);
			else lputs("default IRQ");
			break;
		default:
			base=com_base;
			lprintf("UART I/O %Xh, ",com_base);
			if(com_irq)
				lprintf("IRQ %d",com_irq);
			else lputs("default IRQ");
			break; }

	if(base==0xffff)
		lprintf(" channel %u",com_irq);
	i=rioini(base,com_irq);
	if(i) {
		lprintf(" - Failed! (%d)\r\n",i);
		exit(1); }
	if(mdm_misc&MDM_FLOWCTRL)
		rioctl(IOSM|CTSCK|RTSCK); /* set rts/cts chk */
	if(com_rate)
		setbaud((uint)(com_rate&0xffffL));
	msr=&riobp-1; }

/* STATUS LINE */
statusline();

rioctl(TSTYPE|mswtyp);	 /* set time-slice API type */

rioctl(CPTON);			/* ctrl-p translation */

i=INT29L;
if(com_port)
	i|=(INT29R|INT16);
ivhctl(i);
io_int=1;

atexit(smm_exit);

printf("\r\nSynchronet Match Maker  v%s  Developed 1995-1997 Rob Swindell\n\n"
    ,SMM_VER);

if(checktime()) {
    printf("Time problem!\n");
    return; }

if((file=nopen("SMM.CFG",O_RDONLY))==-1) {
	bprintf("\r\n\7Error opening/creating SMM.DAB\r\n");
    exit(1); }
if((stream=fdopen(file,"w+b"))==NULL) {
	bprintf("\r\n\7Error converting SMM.DAB file handle to stream\r\n");
    exit(1); }
str[0]=0;
fgets(str,128,stream);
purity_age=atoi(str);
str[0]=0;
fgets(str,128,stream);
min_age=atoi(str);
str[0]=0;
fgets(str,128,stream);
min_level=atoi(str);

req_flags1[0]=0;
fgets(req_flags1,128,stream);
req_flags1[27]=0;
truncsp(req_flags1);

req_flags2[0]=0;
fgets(req_flags2,128,stream);
req_flags2[27]=0;
truncsp(req_flags2);

req_flags3[0]=0;
fgets(req_flags3,128,stream);
req_flags3[27]=0;
truncsp(req_flags3);

req_flags4[0]=0;
fgets(req_flags4,128,stream);
req_flags4[27]=0;
truncsp(req_flags4);

str[0]=0;
fgets(str,128,stream);
profile_cdt=atol(str);
str[0]=0;
fgets(str,128,stream);
telegram_cdt=atol(str);
str[0]=0;
fgets(str,128,stream);
auto_update=atoi(str);
str[0]=0;
fgets(str,128,stream);
notify_user=atoi(str);

fgets(str,128,stream);		// regnum

str[0]=0;
fgets(str,128,stream);
telegram_level=atoi(str);

str[0]=0;
fgets(str,128,stream);
que_level=atoi(str);

str[0]=0;
fgets(str,128,stream);
wall_level=atoi(str);

str[0]=0;
fgets(str,128,stream);
wall_cdt=atol(str);

str[0]=0;
fgets(str,128,stream);
que_cdt=atol(str);

fgets(zmodem_send,128,stream);
if(!zmodem_send[0])
    strcpy(zmodem_send,DEFAULT_ZMODEM_SEND);
truncsp(zmodem_send);

str[0]=0;
fgets(str,128,stream);
smm_misc=atol(str);

fgets(str,128,stream);
sprintf(system_name,"%.40s",str);
truncsp(system_name);

fgets(local_view,128,stream);
truncsp(local_view);

str[0]=0;
fgets(str,128,stream);
sysop_level=atoi(str);
if(!sysop_level)
    sysop_level=90;

str[0]=0;
fgets(str,128,stream);
wall_age=atoi(str);

str[0]=0;
fgets(str,128,stream);
age_split=atoi(str);

fclose(stream);

if(!system_name[0] && sys_name[0])
	strcpy(system_name,sys_name);
if(!system_name[0]) {
    printf("\7\nERROR: System name not specified\n");
    exit(3) ; }

sprintf(str,"%.25s",system_name);
strupr(str);
system_crc=crc32(str);

if((file=open("SMM.DAB",O_RDWR|O_BINARY|SH_DENYNO|O_CREAT
    ,S_IWRITE|S_IREAD))==-1) {
	bprintf("\r\n\7Error opening/creating SMM.DAB\r\n");
    exit(1); }
if((stream=fdopen(file,"w+b"))==NULL) {
	bprintf("\r\n\7Error converting SMM.DAB file handle to stream\r\n");
    exit(1); }
setvbuf(stream,0L,_IOFBF,4096);

if((file=open("SMM.IXB",O_RDWR|O_BINARY|SH_DENYNO|O_CREAT
    ,S_IWRITE|S_IREAD))==-1) {
	bprintf("\r\n\7Error opening/creating SMM.IXB\r\n");
    exit(1); }
if((index=fdopen(file,"w+b"))==NULL) {
	bprintf("\r\n\7Error converting SMM.IXB file handle to stream\r\n");
    exit(1); }
setvbuf(stream,0L,_IOFBF,1024);

trashfile=NULL;
if((file=open("SMM.CAN",O_RDONLY|SH_DENYNO))!=-1) {
	trashfile=fdopen(file,"rb");
	setvbuf(trashfile,NULL,_IOFBF,4096); }

total_ques=0;
i=nopen("QUE.LST",O_RDONLY);
if(i!=-1)
	que_lst=fdopen(i,"rb");
else
	que_lst=NULL;
i=0;
while(que_lst && !feof(que_lst) && i<5) {
	if(!fgets(name,81,que_lst))
		break;
	truncsp(name);
	strupr(name);
	sprintf(str,"%s.QUE",name);
	if(!fgets(desc,81,que_lst))
		break;
	truncsp(desc);
	if(!fgets(tmp,81,que_lst))
		break;
	truncsp(tmp);
	if((file=nopen(str,O_RDONLY))!=-1) {
		fp=fdopen(file,"rb");
		total_ques++;
		if((que[i]=(questionnaire_t *)MALLOC(sizeof(questionnaire_t)))==NULL) {
			bprintf("Can't malloc questionnaires!\r\n");
			exit(1); }
		sprintf(que[i]->name,"%.8s",name);
		sprintf(que[i]->desc,"%.25s",desc);
		que[i]->req_age=atoi(tmp);
		fgets(str,128,fp);
		que[i]->total_ques=atoi(str);
		if(que[i]->total_ques<1 || que[i]->total_ques>20) {
			bprintf("Invalid number of questions (%d) in questionnaire #%d\r\n"
				,que[i]->total_ques,i+1);
			exit(1); }
		for(j=0;j<que[i]->total_ques;j++) {
			if(!fgets(str,128,fp))
				break;
			truncsp(str);
			if(!str[0])
				break;
			str[80]=0;
			strcpy(que[i]->que[j].txt,str);
			fgets(str,128,fp);
			que[i]->que[j].answers=atoi(str);
			if(que[i]->que[j].answers<1 || que[i]->que[j].answers>16) {
				bprintf("Invalid number of answers (%d) in question #%d "
					"of questionnaire #%d\r\n"
					,que[i]->que[j].answers,j+1,i+1);
				exit(1); }
			fgets(str,128,fp);
			que[i]->que[j].allowed=atoi(str);
			for(k=0;k<que[i]->que[j].answers;k++) {
				if(!fgets(str,128,fp))
					break;
				truncsp(str);
				if(!str[0])
					break;
				str[60]=0;
				strcpy(que[i]->que[j].ans[k],str); }
			que[i]->que[j].answers=k; }

		que[i]->total_ques=j; }
	fclose(fp);
	i++; }
fclose(que_lst);

lncntr=0;
getsmsg(user_number);
if(lncntr)
	pause();

memset(&useron,0,sizeof(user_t));

atexit(smm_exit);

bputs("\1n\1hSearching for your profile...");
rewind(index);
while(!feof(index)) {
	if(!fread(&ixb,sizeof(ixb_t),1,index))
		break;
	if(ixb.system==system_crc && ixb.number==user_number) {
		useron_record=(ftell(index)-sizeof(ixb_t))/sizeof(ixb_t);
		fseek(stream,useron_record*sizeof(user_t),SEEK_SET);
		fread(&user,sizeof(user_t),1,stream);
		if(stricmp(user.realname,user_name)) {	/* new name, so delete it */
			user.misc|=USER_DELETED;
			fseek(stream,ftell(stream)-sizeof(user_t),SEEK_SET);
			fwrite(&user,sizeof(user_t),1,stream);
			fseek(stream,0,SEEK_CUR);
			fseek(index,ftell(index)-sizeof(ixb_t),SEEK_SET);
			ixb.number=0;
			fwrite(&ixb,sizeof(ixb_t),1,index);
			sprintf(str,"%04u.MSG",user_number);
			remove(str);
			continue; }
		useron=user;		/* name matches, same user */
		break; } }
fflush(stream);
fflush(index);
CRLF;

if(useron.number && useron.sex!='M' && useron.sex!='F') {
	if(yesno("Are you of male gender"))
		useron.sex='M';
	else
		useron.sex='F';
	write_user();
	user_sex=useron.sex; }

if(!useron.number && user_sex!='M' && user_sex!='F' && can_add()) {
	if(yesno("Are you of male gender"))
		user_sex='M';
	else
		user_sex='F'; }

if(!useron.number && can_add()) {
	sprintf(str,"%04u.MSG",user_number);
	remove(str);
	printfile("SMM_LOGO.ASC");
	if(yesno("\r\nYour profile doesn't exist. Create it now"))
		add_userinfo(); }

if(gotoname[0]) {
	strupr(gotoname);
	name_crc=crc32(gotoname);
	rewind(index);
	i=0;
	while(!feof(index)) {
		if(!fread(&ixb,sizeof(ixb_t),1,index))
			break;
		if(!ixb.number)    /* DELETED */
			continue;
		if(ixb.name!=name_crc)
			continue;
		fseek(stream
			,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
			*sizeof(user_t),SEEK_SET);
		if(!fread(&user,sizeof(user_t),1,stream))
			continue;
		if(minor_protection(user))
			continue;
		i=1;
		if(!long_user_info(user))
			break; }
	if(!i) {
		bprintf("\r\n\1n\1h%s \1rnot found.\r\n",gotoname);
		pause(); }
	return; }

/*********************************************/
/* ALL User Messages (from Digital Dynamics) */
/*********************************************/
for(i=findfirst("ALL*.MSG",&ff,0);!i;i=findnext(&ff)) {
	if((j=nopen(ff.ff_name,O_RDONLY))==-1) {
		bprintf("\1n\1r\1hCan't open \1w%s\r\n",ff.ff_name);
		pause();
		continue; }
	if((fp=fdopen(j,"rb"))==NULL) {
		bprintf("\1n\1r\1hCan't fdopen \1w%s\r\n",ff.ff_name);
		pause();
		continue; }
	str[0]=0;
	fgets(str,128,fp);
	truncsp(str);
	ul=ahtoul(str);    /* Expiration date */
	ul^=0x305F6C81UL;
	attr(LIGHTGRAY);
	cls();
	while(!feof(fp)) {
		if(!fgets(str,128,fp))
			break;
		bputs(str); }
	fclose(fp);
	CRLF;
	if(ul<=time(NULL))
		remove(ff.ff_name); }

/**************************************************/
/* One-time User Messages (from Digital Dynamics) */
/**************************************************/
for(i=findfirst("ONE*.MSG",&ff,0);!i;i=findnext(&ff)) {
	if(fdate_dir(ff.ff_name)<useron.lastin)
		continue;
	if((j=nopen(ff.ff_name,O_RDONLY))==-1) {
		bprintf("\1n\1r\1hCan't open \1w%s\r\n",ff.ff_name);
		pause();
		continue; }
	if((fp=fdopen(j,"rb"))==NULL) {
		bprintf("\1n\1r\1hCan't fdopen \1w%s\r\n",ff.ff_name);
		pause();
		continue; }
	str[0]=0;
	fgets(str,128,fp);
	truncsp(str);
	ul=ahtoul(str);    /* Expiration date */
	ul^=0x305F6C81UL;
	attr(LIGHTGRAY);
	cls();
	while(!feof(fp)) {
		if(!fgets(str,128,fp))
			break;
		bputs(str); }
	fclose(fp);
	CRLF;
	if(ul<=time(NULL))
        remove(ff.ff_name); }

/******************************************/
/* Sysop Messages (from Digital Dynamics) */
/******************************************/
for(i=findfirst("SYS*.MSG",&ff,0);SYSOP && !i;i=findnext(&ff)) {
	if((j=nopen(ff.ff_name,O_RDONLY))==-1) {
		bprintf("\1n\1r\1hCan't open \1w%s\r\n",ff.ff_name);
		pause();
		continue; }
	if((fp=fdopen(j,"rb"))==NULL) {
		bprintf("\1n\1r\1hCan't fdopen \1w%s\r\n",ff.ff_name);
		pause();
		continue; }
	str[0]=0;
	fgets(str,128,fp);
	truncsp(str);
	ul=ahtoul(str);    /* Expiration date */
	ul^=0x305F6C81UL;
	attr(LIGHTGRAY);
	cls();
	while(!feof(fp)) {
		if(!fgets(str,128,fp))
			break;
		bputs(str); }
	fclose(fp);
	CRLF;
	if(ul<=time(NULL) || !noyes("Delete message"))
        remove(ff.ff_name); }

statusline();
while(1) {
	checkline();
	aborted=0;
	attr(LIGHTGRAY);
	cls();

	bprintf("\1n  \1b\1hSynchronet \1cMatch Maker  \1bv%s (XSDK v%s)  "
		"Developed 1995-1997 Rob Swindell\1n\r\n\r\n",SMM_VER,xsdk_ver);
	printfile("SMM_LOGO.ASC");
	CRLF;

	l=filelength(fileno(stream));
	if(l<0) l=0;
	sprintf(str,"\1n\1cThere are \1h%lu\1n\1c entries in the user profile "
		"database.\r\n",l/(long)sizeof(user_t));
	center(str);

	sprintf(str,"%04u.MSG",user_number);
	if(fexist(str))
		center("\1h\1mYou have awaiting telegrams!\r\n");

	if(flength("WALL.DAB")>0)
		center("\1h\1cThere is writing on the Wall.\r\n");

	if(!useron.number)
		center("\1h\1rYou have not yet created a profile!\r\n");

	printfile("SMM_MAIN.ASC");

	nodesync(); 			/* Display any waiting messages */

	bprintf("\r\n%32s\1b\1hWhich or [\1wQ\1b]uit: \1h\1w",nulstr);
	ch=getkey(K_UPPER);
//	  bprintf("%c\r\n",ch);
	bputs("\r\1>");
	switch(ch) {
		case '?':
			break;
		case '*':
			if(!SYSOP)
				break;
			bputs("\1n\1hSearching...");
			rewind(stream);
			while(!feof(stream) && !aborted) {
				if(!fread(&user,sizeof(user_t),1,stream))
					break;
				if(user.misc&USER_DELETED) {
					sprintf(str,"\r\nUndelete \1c%s \1b(\1c%s@%s\1b)"
						,user.name,user.realname,user.system);
					if(!yesno(str))
						continue;
					fseek(stream,ftell(stream)-sizeof(user_t),SEEK_SET);
					user.misc&=~USER_DELETED;
					fwrite(&user,sizeof(user_t),1,stream);
					fseek(stream,0,SEEK_CUR);
					fseek(index
						,((ftell(stream)-sizeof(user_t))/sizeof(user_t))
						*sizeof(ixb_t),SEEK_SET);
					strcpy(str,user.name);
					strupr(str);
					ixb.name=crc32(str);
					sprintf(str,"%.25s",user.system);
					strupr(str);
					ixb.system=crc32(str);
					ixb.updated=user.updated;
					ixb.number=user.number;
					fwrite(&ixb,sizeof(ixb_t),1,index);
					} }
			fflush(stream);
			fflush(index);
			break;
		case '!':
			if(!SYSOP)
				break;
			cls();
			bputs("\1n\1hSystems Participating in Your Database:\r\n\r\n\1m");
			crc_lst=NULL;
			j=0;
			rewind(stream);
			while(!feof(stream) && !aborted) {
				if(!fread(&user,sizeof(user_t),1,stream))
                    break;
				if(user.misc&USER_DELETED || !(user.misc&USER_FROMSMB))
					continue;
				if(!smm_pause)
					lncntr=0;
				sprintf(str,"%.25s",user.system);
				strupr(str);
				sys_crc=crc32(str);
				for(i=0;i<j;i++)
					if(sys_crc==crc_lst[i])
						break;
				if(i==j) {
					crc_lst=REALLOC(crc_lst,(j+1)*sizeof(ulong));
					if(crc_lst==NULL) {
						printf("REALLOC error!\n");
						pause();
						break; }
					crc_lst[j++]=sys_crc;
					bprintf("%s\r\n",user.system); } }
			if(crc_lst)
				FREE(crc_lst);
			if(j)
				bprintf("\1n\r\n\1h%u systems listed.\r\n",j);
			pause();
			break;
		case '\\':
			if(!SYSOP)
				break;
			bprintf("Rebuilding Index...");
			rewind(stream);
			rewind(index);
			while(!feof(stream)) {
				if(!fread(&user,sizeof(user_t),1,stream))
					break;
				if(user.misc&USER_DELETED)
					ixb.number=0;
				else
					ixb.number=user.number;
				strupr(user.name);
				ixb.name=crc32(user.name);
				strupr(user.system);
				user.system[25]=0;
				ixb.system=crc32(user.system);
				ixb.updated=user.updated;
				fwrite(&ixb,sizeof(ixb_t),1,index); }
			break;
		case 'R':
			if(useron.number) {
				bputs("\1y\1hMinimum match percentage: \1w");
				sprintf(str,"%u",useron.min_match);
				if(!getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL) || aborted)
					break;
				useron.min_match=atoi(str);
				if(useron.min_match>100)
					useron.min_match=100; }
			rewind(stream);
			bprintf("\1n\1l\1hSearching...");
			while(!feof(stream) && !aborted) {
				if(!fread(&user,sizeof(user_t),1,stream))
					break;
				if(user.misc&USER_DELETED)
					continue;
				if(minor_protection(user))
					continue;
				if(useron.min_match && useron.number) {
					match=total_match(useron,user);
					if(match<useron.min_match)
						continue; }
				if(!long_user_info(user))
					break;
				if(!smm_pause)
					lncntr=0;
				bprintf("\1n\1l\1hSearching..."); }
			break;
		case 'F':
			bputs("\1y\1hText to search for: ");
			if(!getstr(str,25,K_UPPER|K_LINE))
                break;
			ext=yesno("\r\nDisplay extended profiles");
			cls();
			if(!smm_pause && !ext)
				printfile("LIST_HDR.ASC");
			rewind(stream);
			i=0;
			while(!aborted) {
				bprintf("\1n\1l\1hSearching...");
				while(!feof(stream) && !aborted) {
					if(!fread(&user,sizeof(user_t),1,stream))
						break;
					if(user.misc&USER_DELETED)
						continue;
					if(minor_protection(user))
						continue;
					tmpuser=user;
					sprintf(tmp,"%lu",user.number);
					strupr(user.name);
					strupr(user.realname);
					strupr(user.system);
					strupr(user.location);
					strupr(user.note[0]);
					strupr(user.note[1]);
					strupr(user.note[2]);
					strupr(user.note[3]);
					strupr(user.note[4]);
					if(strstr(user.name,str)
						|| strstr(user.location,str)
						|| strstr(user.mbtype,str)
						|| strstr(user.note[0],str)
						|| strstr(user.note[1],str)
						|| strstr(user.note[2],str)
						|| strstr(user.note[3],str)
						|| strstr(user.note[4],str)
						|| (SYSOP && strstr(user.realname,str))
						|| (SYSOP && strstr(user.system,str))
						|| (SYSOP && !strcmp(tmp,str))
						) {
						i=1;
						if(ext && !long_user_info(tmpuser))
							break;
						if(!ext && short_user_info(tmpuser)==0)
							break;
						if(!smm_pause)
							lncntr=0; } }
				if(aborted || ext || !lncntr)
					break;
				user.number=0;
				if(short_user_info(user)>=0)
					break; }
			if(!i)
				bprintf("\r\n\1n\1h%s \1rnot found.\r\n",str);
            break;
		case 'G':
			bputs("\1y\1hUser name: ");
			if(!getstr(str,25,K_UPPER|K_LINE))
                break;
			truncsp(str);
			name_crc=crc32(str);
			rewind(index);
			i=0;
			while(!feof(index)) {
				if(!fread(&ixb,sizeof(ixb_t),1,index))
					break;
				if(!ixb.number)    /* DELETED */
					continue;
				if(ixb.name!=name_crc)
					continue;
				fseek(stream
					,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
					*sizeof(user_t),SEEK_SET);
				if(!fread(&user,sizeof(user_t),1,stream))
					continue;
				if(minor_protection(user))
                    continue;
				i=1;
				if(!long_user_info(user))
					break; }
			if(!i)
				bprintf("\r\n\1n\1h%s \1rnot found.\r\n",str);
            break;
		case 'L':
			if(useron.number) {
				bputs("\1y\1hMinimum match percentage: \1w");
				sprintf(str,"%u",useron.min_match);
				if(!getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL) || aborted)
					break;
				useron.min_match=atoi(str);
				if(useron.min_match>100)
					useron.min_match=100; }
			cls();
			if(!smm_pause)
				printfile("LIST_HDR.ASC");
			rewind(stream);
			while(!aborted) {
				while(!feof(stream) && !aborted) {
					if(!fread(&user,sizeof(user_t),1,stream))
						break;
					if(user.misc&USER_DELETED)
						continue;
					if(minor_protection(user))
						continue;
					if(useron.min_match && useron.number) {
						match=total_match(useron,user);
						if(match<useron.min_match)
							continue; }
					if(!short_user_info(user))
						break;
					if(!smm_pause)
						lncntr=0; }
				if(aborted || !lncntr)
					break;
				user.number=0;
				if(short_user_info(user)>=0)
					break; }
            break;

		case 'N':
			while(1) {
				checkline();
				bputs("\1y\1hLast update (MM/DD/YY): ");
				if(useron.number)
					unixtodstr(useron.lastin,str);
				else
					str[0]=0;
				if(!getstr(str,8,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
					break;
				if(isdigit(str[0]) && isdigit(str[1]) && str[2]=='/'
					&& isdigit(str[3]) && isdigit(str[4]) && str[5]=='/'
					&& isdigit(str[6]) && isdigit(str[7]))
					break;
				bputs("\r\n\1h\1rInvalid date!\r\n\r\n"); }
			if(!str[0] || aborted)
				break;
			l=dstrtounix(str);
			if(useron.number) {
				bputs("\1y\1h\r\nMinimum match percentage: \1w");
				sprintf(str,"%u",useron.min_match);
				if(!getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL) || aborted)
					break;
				useron.min_match=atoi(str);
				if(useron.min_match>100)
					useron.min_match=100; }
			ext=!noyes("\r\nDisplay extended profiles");
			if(aborted)
				break;
			cls();
			if(!ext && !smm_pause)
				printfile("LIST_HDR.ASC");
			rewind(index);
			while(!aborted) {
				while(!feof(index) && !aborted) {
					if(!smm_pause)
						lncntr=0;
					if(!fread(&ixb,sizeof(ixb_t),1,index))
						break;
					if(!ixb.number) 	/* DELETED */
						continue;
					if(ixb.updated>=l) {
						fseek(stream
							,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
							*sizeof(user_t),SEEK_SET);
						if(!fread(&user,sizeof(user_t),1,stream))
							continue;
						if(minor_protection(user))
							continue;
						if(useron.min_match && useron.number) {
							match=total_match(useron,user);
							if(match<useron.min_match)
								continue; }
						if(ext && !long_user_info(user))
							break;
						if(!ext && short_user_info(user)==0)
							break;
						continue; } }
				if(aborted || ext || !lncntr)
					break;
				user.number=0;
				if(short_user_info(user)>=0)
					break; }
            break;

		case 'U':
		case 'W':       /* Create/Edit Profile */
			if(!can_add()) {
				bprintf("\1h\1rYou have insufficient access to create a "
					"profile.\r\n\r\n");
				break; }

			if(!useron.number) {
				add_userinfo();
				break; }

			user=useron;
			tleft=timeleft;
			if(!get_user_info(&user)) {
				timeleft=tleft;
				break; }

			if(!memcmp(&user,&useron,sizeof(user_t))) {
				timeleft=tleft;
				break; }

			if(!yesno("Save changes")) {
				timeleft=tleft;
				break; }
			timeleft=tleft;
			user.updated=time(NULL);
			useron=user;
			write_user();
            break;

		case 'D':
			if(SYSOP) {
				bputs("Name to Delete: ");
				if(useron.number) {
					strcpy(str,useron.name);
					strupr(str); }
				else
					str[0]=0;
				getstr(str,25,K_UPPER|K_LINE|K_EDIT|K_AUTODEL);
				rewind(stream);
				while(!feof(stream)) {
					if(!fread(&user,sizeof(user_t),1,stream))
						break;
					strcpy(tmp,user.name);
					strupr(tmp);
					if(!(user.misc&USER_DELETED) && strstr(tmp,str)) {
						sprintf(tmp,"Delete %s (%s@%s)"
							,user.name,user.realname,user.system);
						i=noyes(tmp);
						if(aborted)
							break;
						if(i)
							continue;
						fseek(stream,ftell(stream)-sizeof(user_t),SEEK_SET);
						fseek(index
							,((ftell(stream)-sizeof(user_t))/sizeof(user_t))
							*sizeof(ixb_t),SEEK_SET);
						user.misc|=USER_DELETED;
						fwrite(&user,sizeof(user_t),1,stream);
						fseek(stream,0,SEEK_CUR);
						memset(&ixb,0,sizeof(ixb_t));
						fwrite(&ixb,sizeof(ixb_t),1,index);
						fflush(stream);
						fflush(index);
						delphoto(user);
						if(user.number==user_number
							&& !stricmp(user.system,system_name))
							useron.number=0;
						break; } }
				if(aborted)
					break;
				break; }
			if(!useron.number) {
				bputs("\1h\1rYour profile doesn't exist.\r\n\r\n");
                break; }
			if(noyes("Are you sure you want to delete your profile"))
                break;
			if(!cdt_warning(-profile_cdt))
				break;
			sprintf(str,"%04u.MSG",user_number);
			remove(str);
			fseek(stream,useron_record*sizeof(user_t),SEEK_SET);
			useron.misc|=USER_DELETED;
			fwrite(&useron,sizeof(user_t),1,stream);
			fflush(stream);
			memset(&ixb,0,sizeof(ixb_t));
			fseek(index,useron_record*sizeof(ixb_t),SEEK_SET);
			fwrite(&ixb,sizeof(ixb_t),1,index);
			fflush(index);
			delphoto(user);
			useron.number=0;
			aborted=0;
			bputs("\r\n\1h\1r\1iProfile deleted.\r\n");
			adjust_cdt(-profile_cdt);
            break;
		case 'P':
			smm_pause=!smm_pause;
			bprintf("\1r\1hScreen pause is now \1w%s\r\n\r\n"
				,smm_pause ? "ON":"OFF");
            break;
		case 'M':
			mbtype_desc(useron.mbtype);
			break;
		case 'H':
			cls();
			printfile("SMM_HELP.ASC");
			break;
		case 'O':
			if(noyes("Hang-up now"))
				break;
			dtr(5);
			if(node_dir) {
				sprintf(str,"%sHANGUP.NOW",node_dir);
				fopen(str,"wb");
				nodesync(); }
			return;
		case 'S':
			bputs("\1y\1hUser name: ");
			if(!getstr(str,25,K_UPPER|K_LINE))
                break;
			truncsp(str);
			name_crc=crc32(str);
			rewind(index);
			i=0;
			while(!feof(index)) {
				if(!fread(&ixb,sizeof(ixb_t),1,index))
					break;
				if(!ixb.number)    /* DELETED */
					continue;
				if(ixb.name!=name_crc)
					continue;
                fseek(stream
					,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
					*sizeof(user_t),SEEK_SET);
				if(!fread(&user,sizeof(user_t),1,stream))
					continue;
				if(minor_protection(user))
                    continue;
				i=1;
				if(send_telegram(user))
					break; }
			if(!i)
				bprintf("\r\n\1n\1h%s \1rnot found.\r\n",str);
            break;
		case 'T':
			sprintf(str,"%04u.MSG",user_number);
			if(!fexist(str)) {
				bputs("\1r\1hYou have no telegrams waiting.\r\n\r\n");
				break; }
			cls();
			printfile(str);
			if(noyes("\r\nDelete all telegrams waiting for you"))
				break;
			remove(str);
			break;
		case 'V':   /* Visit the wall */
			if(getage(user_birth)<wall_age && !SYSOP) {
				bputs("\1r\1hSorry, you're too young to view the wall.\r\n");
				pause();
				break; }
			while(1) {
				checkline();
				bputs("\1y\1hView writing since (MM/DD/YY): ");
                if(useron.number)
                    unixtodstr(useron.lastin,str);
                else
                    str[0]=0;
                if(!getstr(str,8,K_UPPER|K_LINE|K_EDIT|K_AUTODEL))
                    break;
                if(isdigit(str[0]) && isdigit(str[1]) && str[2]=='/'
                    && isdigit(str[3]) && isdigit(str[4]) && str[5]=='/'
                    && isdigit(str[6]) && isdigit(str[7]))
                    break;
                bputs("\r\n\1h\1rInvalid date!\r\n\r\n"); }
			if(aborted)
                break;
			l=dstrtounix(str);
			ext=yesno("\r\nDisplay extended information when available");
			if(aborted)
                break;
			strcpy(str,"WALL.DAB");
			if((file=sopen(str,O_RDWR|O_BINARY|O_CREAT,SH_DENYNO
				,S_IREAD|S_IWRITE))==-1) {
				bprintf("\r\nError opening %s\r\n",str);
				break; }
			cls();
			offset=0L;
			while(!eof(file) && !aborted) {
				if(read(file,&wall,sizeof(wall_t))!=sizeof(wall_t))
					break;
				if(wall.imported<l)
					continue;
				strcpy(str,wall.name);
				strupr(str);
				name_crc=crc32(str);
				sprintf(str,"%.25s",wall.system);
				strupr(str);
				sys_crc=crc32(str);
				rewind(index);
				i=0;
				user.sex=0;
				while(ext && !feof(index) && !aborted) {
					if(!fread(&ixb,sizeof(ixb_t),1,index))
						break;
					if(!ixb.number)    /* DELETED */
						continue;
					if(ixb.name!=name_crc || ixb.system!=sys_crc)
						continue;
					fseek(stream
						,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
						*sizeof(user_t),SEEK_SET);
					if(!fread(&user,sizeof(user_t),1,stream))
						continue;
					if(minor_protection(user))
						break;
					main_user_info(user);
					if(useron.number) {
						CRLF;
						bprintf("\1h\1mThis user meets your profile "
							"preferences:\1w%3u%%            "
							,basic_match(useron,user));
						bprintf("\1n\1gCreated: \1h%s\r\n"
							,unixtodstr(user.created,str));
						bprintf("\1h\1mYou meet this user's profile "
							"preferences:\1w%3u%%            "
							,basic_match(user,useron));
						bprintf("\1n\1gUpdated: \1h%s\r\n"
							,unixtodstr(user.updated,str)); }

					i=1;
					CRLF;
					break; }
				if(!i) {
					if(ext)
						cls();
					bprintf("\1n\1h%s ",wall.name); }
				bprintf("\1n\1h\1cWrote on %s:\r\n"
					,timestr(&wall.written));
				for(j=0;j<5;j++) {
					if(!wall.text[j][0])
						break;
					bprintf("\1n\1%c%5s%s\r\n"
						,user.sex=='F'?'m':'g',"",wall.text[j]); }
				if(aborted)
					break;
				if(ext) {
					nodesync();
					if(i) {
						bputs(PrevReadSendQuitOrMore);
						ch=getkey(K_UPPER);
						if(ch=='R')
							long_user_info(user);
						if(ch=='S')
							send_telegram(user);
						if(ch=='R' || ch=='S')  /* Don't move forward */
							lseek(file,tell(file)-sizeof(wall_t),SEEK_SET); }
					else {
						bputs("\r\n\1n\1hProfile not found.\r\n\r\n");
						bputs("\1b[\1cP\1b]revious, [\1cQ\1b]uit, "
							"or [\1cM\1b]ore: \1c");
						ch=getkey(K_UPPER); }
					if(ch=='P') {
						lseek(file,offset,SEEK_SET);
						if(tell(file))
							lseek(file,tell(file)-sizeof(wall_t),SEEK_SET);
						offset=tell(file);
						continue; }
					if(ch=='Q') {
						cls();
						break; }
					offset=tell(file); }
				else if(lncntr+7>=user_rows || eof(file)) {
					lncntr=0;
					bputs(PrevReadSendQuitOrMore);
					ch=getkey(K_UPPER);
					if(ch=='Q')
						break;
					else if(ch=='S' || ch=='R') {
						bprintf("\1n\r\1>\1y\1hUser name: ");
						if(getstr(name,25,K_UPPER|K_LINE|K_NOCRLF)) {
							truncsp(name);
							name_crc=crc32(name);
							rewind(index);
							i=0;
							while(!feof(index)) {
								if(!fread(&ixb,sizeof(ixb_t),1,index))
									break;
								if(!ixb.number)    /* DELETED */
									continue;
								if(ixb.name!=name_crc)
									continue;
								fseek(stream
									,((ftell(index)
										-sizeof(ixb_t))/sizeof(ixb_t))
									*sizeof(user_t),SEEK_SET);
								if(!fread(&user,sizeof(user_t),1,stream))
									continue;
								if(minor_protection(user))
									continue;
								i=1;
								if(ch=='S' && send_telegram(user))
									break;
								if(ch=='R' && !long_user_info(user))
									break;	}
							if(!i)
								bprintf("\r\n\1n\1h%s \1rnot found.\r\n",name);
								}
						lseek(file,offset,SEEK_SET); }
					else if(ch=='P') {
						lseek(file,offset,SEEK_SET);
						for(i=0;i<4;i++)
							if(tell(file))
								lseek(file,tell(file)-sizeof(wall_t),SEEK_SET);
						}
					cls();
					offset=tell(file); } }
			cls();
			if(user_level<wall_level) {
				close(file);
                bputs("\r\n\1h\1rYou have insufficient access to write on the "
                    "wall.\r\n");
                pause();
                break; }
			if(aborted || noyes("Write on the Wall")) {
				close(file);
				break; }
			if(!useron.number) {
				bputs("\r\n\1h\1rYou must create a profile first.\r\n");
				close(file);
				break; }
			if(!cdt_warning(wall_cdt)) {
				close(file);
				break; }
			memset(&wall,0,sizeof(wall_t));
			strcpy(wall.name,useron.name);
			strcpy(wall.system,system_name);
			wall.written=wall.imported=time(NULL);
			bputs("\1l\1n\1hWriting on the Wall:\r\n\r\n");
			for(i=0;i<5 && !aborted;i++) {
				bprintf("\1n\1h\1%c%u of 5: \1n\1%c"
					,useron.sex=='F'?'m':'g',i+1,useron.sex=='F'?'m':'g');
				if(!getstr(wall.text[i],70,i==4 ? K_MSG:K_MSG|K_WRAP))
					break;
				if(trash(wall.text[i])) {
					bprintf("\r\n\1r\1hSorry, you can't use that text."
						"\r\n\r\n");
					i--;
					continue; } }
			if(!i || aborted || !yesno("\r\nSave")) {
				close(file);
				break; }
			lseek(file,0L,SEEK_END);
			write(file,&wall,sizeof(wall_t));
			close(file);
			adjust_cdt(wall_cdt);
			if(notify_user && notify_user!=user_number) {
				sprintf(str,"\1n\1hSMM: \1y%s\1m wrote on the Match Maker "
					"Wall\r\n",user_name);
				if(node_dir[0])
					putsmsg(notify_user,str);
				else
					puttgram(notify_user,str); }
			break;
		case 'Q':
			return; } }
}

/* End of SMM.C */
