/* SMMUTIL.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <io.h>
#include <dos.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <malloc.h>
#include <stdarg.h>
#include "gen_defs.h"
#include "smmdefs.h"
#include "crc32.h"

struct date date;
struct time curtime;

FILE *log=NULL;

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

/**********************/
/* Log print function */
/**********************/
void logprintf(char *str, ...)
{
    va_list argptr;
    char buf[256];
    time_t now;
    struct tm *gm;

va_start(argptr,str);
vsprintf(buf,str,argptr);
va_end(argptr);
fprintf(stderr,"\n%s",buf);
if(!log) return;
now=time(NULL);
gm=localtime(&now);
fseek(log,0L,SEEK_END);
fprintf(log,"%02u/%02u/%02u %02u:%02u:%02u %s\r\n"
    ,gm->tm_mon+1,gm->tm_mday,TM_YEAR(gm->tm_year),gm->tm_hour,gm->tm_min,gm->tm_sec
    ,buf);
fflush(log);
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


void delphoto(user_t user)
{
	char fname[64],path[128],tmp[128];
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
	logprintf("Photo (%s) couldn't be removed!",path);
else
	logprintf("Photo (%s) removed",path);
}


int main(int argc, char **argv)
{
	int  i,file,max_age=0,max_wall=0,age;
	long l,m,total_ixbs=0
		,users=0,photos=0,networked=0
		,male_straight=0
		,male_gay=0
		,male_bi=0
		,female_straight=0
		,female_gay=0
		,female_bi=0
		,age12=0
		,age15=0
		,age20=0,age25=0
		,age30=0,age35=0
		,age40=0,age45=0
		,age50=0,age55=0
		,age60=0,age65=0
		,age70=0,age71=0
		,zodiac_aries=0
		,zodiac_taurus=0
		,zodiac_gemini=0
		,zodiac_cancer=0
		,zodiac_leo=0
		,zodiac_virgo=0
		,zodiac_libra=0
		,zodiac_scorpio=0
		,zodiac_sagittarius=0
		,zodiac_capricorn=0
		,zodiac_aquarius=0
		,zodiac_pisces=0
		,hair_blonde=0
		,hair_brown=0
		,hair_red=0
		,hair_black=0
		,hair_grey=0
		,hair_other=0
		,eyes_blue=0
		,eyes_green=0
		,eyes_hazel=0
		,eyes_brown=0
		,eyes_other=0
		,race_white=0
		,race_black=0
		,race_hispanic=0
		,race_asian=0
		,race_amerindian=0
		,race_mideastern=0
		,race_other=0
		,marital_single=0
		,marital_married=0
		,marital_divorced=0
		,marital_widowed=0
		,marital_other=0
		;
	FILE *ixb_fp,*dab_fp,*tmp_fp;
	ixb_t huge *ixb=NULL,ixbrec;
	user_t user;
	wall_t wall;
	time_t now;

fprintf(stderr,"\nSMMUTIL - Synchronet Match Maker Utility - v2.02\n\n");

for(i=1;i<argc;i++)
	if(isdigit(argv[i][0])) {
		if(max_age)
			max_wall=atoi(argv[i]);
		else
			max_age=atoi(argv[i]); }
	else {
		printf("usage: SMMUTIL max_profile_age_in_days "
			"max_wall_writing_age_in_days\n");
		printf("\n");
		printf("example: SMMUTIL 90 7\n");
		exit(1); }

if((file=open("SMM.IXB",O_RDWR|O_BINARY|O_CREAT|O_DENYNONE
	,S_IWRITE|S_IREAD))==-1
	|| (ixb_fp=fdopen(file,"r+b"))==NULL) {
	printf("Error opening SMM.IXB\n");
	exit(1); }

if((file=open("SMM.DAB",O_RDWR|O_BINARY|O_DENYNONE))==-1
	|| (dab_fp=fdopen(file,"r+b"))==NULL) {
	printf("Error opening SMM.DAB\n");
    exit(1); }

if((file=open("SMM.TMP",O_WRONLY|O_CREAT|O_TRUNC|O_BINARY|O_DENYALL
	,S_IWRITE|S_IREAD))==-1
	|| (tmp_fp=fdopen(file,"r+b"))==NULL) {
	printf("Error opening SMM.TMP\n");
    exit(1); }

if((file=open("SMMUTIL.LOG",O_WRONLY|O_CREAT|O_APPEND|O_BINARY|O_DENYALL
	,S_IWRITE|S_IREAD))==-1
	|| (log=fdopen(file,"w+b"))==NULL) {
	printf("Error opening SMMUTIL.LOG\n");
	exit(1); }

fprintf(stderr,"Reading profile data...");
rewind(dab_fp);
while(!feof(dab_fp)) {
	if(!fread(&user,sizeof(user_t),1,dab_fp))
		break;
	if((ixb=REALLOC(ixb,sizeof(ixb_t)*(total_ixbs+1)))==NULL) {
		printf("Malloc error\n");
		exit(1); }
	user.name[25]=0;
	strupr(user.name);
	ixb[total_ixbs].name=crc32(user.name);
	user.system[25]=0;
	strupr(user.system);
	ixb[total_ixbs].system=crc32(user.system);
	ixb[total_ixbs].updated=user.updated;
	if(user.misc&USER_DELETED)
		ixb[total_ixbs].number=0;
	else
		ixb[total_ixbs].number=user.number;
	total_ixbs++; }
fprintf(stderr,"\n");

now=time(NULL);
fprintf(stderr,"Creating new profile index and data files...");
chsize(fileno(ixb_fp),0);
rewind(ixb_fp);
rewind(tmp_fp);
for(l=0;l<total_ixbs;l++) {
	fseek(dab_fp,l*sizeof(user_t),SEEK_SET);
    if(!fread(&user,sizeof(user_t),1,dab_fp)) {
		logprintf("%04lX Couldn't read user record",l);
        continue; }

	/* Make sure all strings are NULL terminated */
	user.name[25]=user.realname[25]=user.system[40]=user.birth[8]=0;
	user.zipcode[10]=user.location[30]=0;
	user.min_zipcode[10]=user.max_zipcode[10]=0;
	for(i=0;i<5;i++)
		user.note[i][50]=0;

	if(!ixb[l].number) {
		logprintf("%04lX %5lu %-25s Deleted user"
			,l,user.number,user.system);
		delphoto(user);
		continue; }
	if(ixb[l].number&0xffff0000UL) {
		logprintf("%04lX %5lu %-25s Invalid user number"
			,l,ixb[l].number,user.system);
		delphoto(user);
		continue; }
	if(max_age
		&& now>ixb[l].updated	// Not in the future
		&& (now-ixb[l].updated)/(24UL*60UL*60UL)>max_age) {
		logprintf("%04lX %5lu %-25s Not updated in %lu days"
			,l,user.number,user.system,(now-ixb[l].updated)/(24UL*60UL*60UL));
		delphoto(user);
		continue; }
	for(m=l+1;m<total_ixbs;m++)
		if(ixb[l].number
			&& ixb[l].number==ixb[m].number && ixb[l].system==ixb[m].system)
			break;
	if(m<total_ixbs) {		/* Duplicate found! */
		logprintf("%04lX %5lu %-25s Duplicate user"
			,l,user.number,user.system);
		delphoto(user);
		continue; }

	if(user.name[0]<SP || user.realname[0]<SP || user.system[0]<SP
		|| user.location[0]<SP || user.zipcode[0]<SP || user.birth[0]<SP) {
		logprintf("%04lX %5lu %-25s Invalid user string"
			,l,user.number,user.system);
		delphoto(user);
		continue; }
	if(!user.sex || !user.marital || !user.race || !user.hair || !user.eyes) {
		logprintf("%04lX %5lu %-25s Null field"
			,l,user.number,user.system);
		delphoto(user);
		continue; }
	if(user.sex=='M') {
		if(user.pref_sex=='F')
			male_straight++;
		else if(user.pref_sex=='M')
			male_gay++;
		else
			male_bi++; }
	else if(user.sex=='F') {
		if(user.pref_sex=='M')
			female_straight++;
		else if(user.pref_sex=='F')
			female_gay++;
		else
			female_bi++; }
	else {
		logprintf("%04lX %5lu %-25s Invalid sex (%02X)"
			,l,user.number,user.system,user.sex);
		delphoto(user);
		continue; }
	users++;
	if(user.misc&USER_PHOTO)
		photos++;
	if(user.misc&USER_FROMSMB)
		networked++;
	age=getage(user.birth);
	if(age<13) age12++;
	else if(age<16) age15++;
	else if(age<21) age20++;
	else if(age<26) age25++;
	else if(age<31) age30++;
	else if(age<36) age35++;
	else if(age<41) age40++;
	else if(age<46) age45++;
	else if(age<51) age50++;
	else if(age<56) age55++;
	else if(age<61) age60++;
	else if(age<66) age65++;
	else if(age<71) age70++;
	else age71++;
	switch(user.hair) {
		case HAIR_BLONDE:
			hair_blonde++;
			break;
		case HAIR_BROWN:
			hair_brown++;
			break;
		case HAIR_RED:
			hair_red++;
			break;
		case HAIR_BLACK:
			hair_black++;
			break;
		case HAIR_GREY:
			hair_grey++;
			break;
		default:
			hair_other++;
			break; }

	switch(user.eyes) {
		case EYES_BLUE:
			eyes_blue++;
			break;
        case EYES_BROWN:
			eyes_brown++;
			break;
        case EYES_GREEN:
			eyes_green++;
            break;
        case EYES_HAZEL:
			eyes_hazel++;
            break;
		default:
			eyes_other++;
			break; }
	switch(user.marital) {
		case MARITAL_SINGLE:
			marital_single++;
			break;
		case MARITAL_MARRIED:
			marital_married++;
            break;
        case MARITAL_DIVORCED:
			marital_divorced++;
            break;
        case MARITAL_WIDOWED:
			marital_widowed++;
            break;
        default:
			marital_other++;
			break; }

	switch(user.race) {
		case RACE_WHITE:
			race_white++;
			break;
		case RACE_BLACK:
			race_black++;
			break;
		case RACE_HISPANIC:
			race_hispanic++;
			break;
		case RACE_ASIAN:
			race_asian++;
			break;
		case RACE_AMERINDIAN:
			race_amerindian++;
			break;
		case RACE_MIDEASTERN:
			race_mideastern++;
			break;
		default:
			race_other++;
			break; }

	if((!strncmp(user.birth,"03",2) && atoi(user.birth+3)>=21)
		|| (!strncmp(user.birth,"04",2) && atoi(user.birth+3)<=19))
		zodiac_aries++;
	else if((!strncmp(user.birth,"04",2) && atoi(user.birth+3)>=20)
		|| (!strncmp(user.birth,"05",2) && atoi(user.birth+3)<=20))
		zodiac_taurus++;
	else if((!strncmp(user.birth,"05",2) && atoi(user.birth+3)>=21)
		|| (!strncmp(user.birth,"06",2) && atoi(user.birth+3)<=20))
		zodiac_gemini++;
	else if((!strncmp(user.birth,"06",2) && atoi(user.birth+3)>=21)
		|| (!strncmp(user.birth,"07",2) && atoi(user.birth+3)<=22))
		zodiac_cancer++;
	else if((!strncmp(user.birth,"07",2) && atoi(user.birth+3)>=23)
		|| (!strncmp(user.birth,"08",2) && atoi(user.birth+3)<=22))
		zodiac_leo++;
	else if((!strncmp(user.birth,"08",2) && atoi(user.birth+3)>=23)
		|| (!strncmp(user.birth,"09",2) && atoi(user.birth+3)<=22))
		zodiac_virgo++;
	else if((!strncmp(user.birth,"09",2) && atoi(user.birth+3)>=23)
		|| (!strncmp(user.birth,"10",2) && atoi(user.birth+3)<=22))
		zodiac_libra++;
	else if((!strncmp(user.birth,"10",2) && atoi(user.birth+3)>=23)
		|| (!strncmp(user.birth,"11",2) && atoi(user.birth+3)<=21))
		zodiac_scorpio++;
	else if((!strncmp(user.birth,"11",2) && atoi(user.birth+3)>=22)
		|| (!strncmp(user.birth,"12",2) && atoi(user.birth+3)<=21))
		zodiac_sagittarius++;
	else if((!strncmp(user.birth,"12",2) && atoi(user.birth+3)>=22)
		|| (!strncmp(user.birth,"01",2) && atoi(user.birth+3)<=19))
		zodiac_capricorn++;
	else if((!strncmp(user.birth,"01",2) && atoi(user.birth+3)>=20)
		|| (!strncmp(user.birth,"02",2) && atoi(user.birth+3)<=18))
		zodiac_aquarius++;
	else if((!strncmp(user.birth,"02",2) && atoi(user.birth+3)>=19)
		|| (!strncmp(user.birth,"03",2) && atoi(user.birth+3)<=20))
		zodiac_pisces++;

	fwrite(&ixb[l],sizeof(ixb_t),1,ixb_fp);
	fwrite(&user,sizeof(user_t),1,tmp_fp);
	}
fprintf(stderr,"\n");
fcloseall();
remove("SMM.DAB");
rename("SMM.TMP","SMM.DAB");

printf("Synchronet Match Maker Statistics\n");
printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
printf("\n");
printf("%-25s         : %lu\n","Total",users);
printf("%-25s         : %lu\n","Photos",photos);
printf("%-25s         : %lu\n","Networked",networked);

if(users) {	/* divide-by-zero fix, Jan-2003 */
	printf("%-25s (%4.1f%%) : %lu\n","Sex: male (hetero)"
		,(((float)male_straight/users)*100.0),male_straight);
	printf("%-25s (%4.1f%%) : %lu\n","Sex: male (gay)"
		,(((float)male_gay/users)*100.0),male_gay);
	printf("%-25s (%4.1f%%) : %lu\n","Sex: male (bi)"
		,(((float)male_bi/users)*100.0),male_bi);
	printf("%-25s (%4.1f%%) : %lu\n","Sex: female (hetero)"
		,(((float)female_straight/users)*100.0),female_straight);
	printf("%-25s (%4.1f%%) : %lu\n","Sex: female (gay)"
		,(((float)female_gay/users)*100.0),female_gay);
	printf("%-25s (%4.1f%%) : %lu\n","Sex: female (bi)"
		,(((float)female_bi/users)*100.0),female_bi);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 12 and younger"
		,(((float)age12/users)*100.0),age12);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 13 to 15 years old"
		,(((float)age15/users)*100.0),age15);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 16 to 20 years old"
		,(((float)age20/users)*100.0),age20);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 21 to 25 years old"
		,(((float)age25/users)*100.0),age25);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 26 to 30 years old"
		,(((float)age30/users)*100.0),age30);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 31 to 35 years old"
		,(((float)age35/users)*100.0),age35);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 36 to 40 years old"
		,(((float)age40/users)*100.0),age40);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 41 to 45 years old"
		,(((float)age45/users)*100.0),age45);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 46 to 50 years old"
		,(((float)age50/users)*100.0),age50);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 51 to 55 years old"
		,(((float)age55/users)*100.0),age55);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 56 to 60 years old"
		,(((float)age60/users)*100.0),age60);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 61 to 65 years old"
		,(((float)age65/users)*100.0),age65);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 66 to 70 years old"
		,(((float)age70/users)*100.0),age70);
	printf("%-25s (%4.1f%%) : %lu\n","Age: 71 and older"
		,(((float)age71/users)*100.0),age71);
	printf("%-25s (%4.1f%%) : %lu\n","Hair: blonde"
		,(((float)hair_blonde/users)*100.0),hair_blonde);
	printf("%-25s (%4.1f%%) : %lu\n","Hair: brown"
		,(((float)hair_brown/users)*100.0),hair_brown);
	printf("%-25s (%4.1f%%) : %lu\n","Hair: black"
		,(((float)hair_black/users)*100.0),hair_black);
	printf("%-25s (%4.1f%%) : %lu\n","Hair: red"
		,(((float)hair_red/users)*100.0),hair_red);
	printf("%-25s (%4.1f%%) : %lu\n","Hair: grey"
		,(((float)hair_grey/users)*100.0),hair_grey);
	printf("%-25s (%4.1f%%) : %lu\n","Hair: other"
		,(((float)hair_other/users)*100.0),hair_other);
	printf("%-25s (%4.1f%%) : %lu\n","Eyes: blue"
		,(((float)eyes_blue/users)*100.0),eyes_blue);
	printf("%-25s (%4.1f%%) : %lu\n","Eyes: brown"
        ,(((float)eyes_brown/users)*100.0),eyes_brown);
	printf("%-25s (%4.1f%%) : %lu\n","Eyes: green"
		,(((float)eyes_green/users)*100.0),eyes_green);
	printf("%-25s (%4.1f%%) : %lu\n","Eyes: hazel"
		,(((float)eyes_hazel/users)*100.0),eyes_hazel);
	printf("%-25s (%4.1f%%) : %lu\n","Eyes: other"
		,(((float)eyes_other/users)*100.0),eyes_other);
	printf("%-25s (%4.1f%%) : %lu\n","Race: white"
		,(((float)race_white/users)*100.0),race_white);
	printf("%-25s (%4.1f%%) : %lu\n","Race: black"
		,(((float)race_black/users)*100.0),race_black);
	printf("%-25s (%4.1f%%) : %lu\n","Race: asian"
		,(((float)race_asian/users)*100.0),race_asian);
	printf("%-25s (%4.1f%%) : %lu\n","Race: amerindian"
		,(((float)race_amerindian/users)*100.0),race_amerindian);
	printf("%-25s (%4.1f%%) : %lu\n","Race: mideastern"
		,(((float)race_mideastern/users)*100.0),race_mideastern);
	printf("%-25s (%4.1f%%) : %lu\n","Race: hispanic"
		,(((float)race_hispanic/users)*100.0),race_hispanic);
	printf("%-25s (%4.1f%%) : %lu\n","Race: other"
		,(((float)race_other/users)*100.0),race_other);
	printf("%-25s (%4.1f%%) : %lu\n","Marital: single"
        ,(((float)marital_single/users)*100.0),marital_single);
	printf("%-25s (%4.1f%%) : %lu\n","Marital: married"
		,(((float)marital_married/users)*100.0),marital_married);
	printf("%-25s (%4.1f%%) : %lu\n","Marital: divorced"
		,(((float)marital_divorced/users)*100.0),marital_divorced);
	printf("%-25s (%4.1f%%) : %lu\n","Marital: widowed"
		,(((float)marital_widowed/users)*100.0),marital_widowed);
	printf("%-25s (%4.1f%%) : %lu\n","Marital: other"
		,(((float)marital_other/users)*100.0),marital_other);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: aries"
		,(((float)zodiac_aries/users)*100.0),zodiac_aries);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: taurus"
		,(((float)zodiac_taurus/users)*100.0),zodiac_taurus);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: gemini"
		,(((float)zodiac_gemini/users)*100.0),zodiac_gemini);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: cancer"
		,(((float)zodiac_cancer/users)*100.0),zodiac_cancer);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: leo"
		,(((float)zodiac_leo/users)*100.0),zodiac_leo);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: virgo"
		,(((float)zodiac_virgo/users)*100.0),zodiac_virgo);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: libra"
		,(((float)zodiac_libra/users)*100.0),zodiac_libra);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: scorpio"
		,(((float)zodiac_scorpio/users)*100.0),zodiac_scorpio);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: sagittarius"
		,(((float)zodiac_sagittarius/users)*100.0),zodiac_sagittarius);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: capricorn"
		,(((float)zodiac_capricorn/users)*100.0),zodiac_capricorn);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: aquarius"
		,(((float)zodiac_aquarius/users)*100.0),zodiac_aquarius);
	printf("%-25s (%4.1f%%) : %lu\n","Zodiac: pisces"
		,(((float)zodiac_pisces/users)*100.0),zodiac_pisces);
}
if(!max_wall)
	return(0);

if((file=open("WALL.DAB",O_RDWR|O_BINARY|O_DENYNONE))==-1
	|| (dab_fp=fdopen(file,"r+b"))==NULL) {
    return(0); 
}

if((file=open("WALL.TMP",O_WRONLY|O_CREAT|O_TRUNC|O_BINARY|O_DENYALL
	,S_IWRITE|S_IREAD))==-1
	|| (tmp_fp=fdopen(file,"r+b"))==NULL) {
	printf("Error opening WALL.TMP\n");
    exit(1); }

fprintf(stderr,"Reading wall data...");
rewind(dab_fp);
while(!feof(dab_fp)) {
	if(!fread(&wall,sizeof(wall_t),1,dab_fp))
        break;
	if((now-wall.imported)/(24UL*60UL*60UL)<=max_wall)
		fwrite(&wall,sizeof(wall_t),1,tmp_fp); }

fprintf(stderr,"\n");
fcloseall();
remove("WALL.DAB");
rename("WALL.TMP","WALL.DAB");

return(0);
}
