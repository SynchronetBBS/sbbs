/* SBBSLIST.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Converts Synchronet BBS List (SBL.DAB) to text file */

#include "xsdk.h"
#include "sbldefs.h"

char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};
char *nulstr="";
char tmp[256];
struct date date;
struct time curtime;

extern int daylight=0;
extern long timezone=0L;

typedef struct {

	char	str[13];
	short	offset;

    } sortstr_t;


int sortstr_cmp(sortstr_t **str1, sortstr_t **str2)
{
return(stricmp((*str1)->str,(*str2)->str));
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()										*/
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


void long_bbs_info(FILE *out, bbs_t bbs)
{
	int i;

fprintf(out,"BBS Name: %s since %s\r\n"
	,bbs.name,unixtodstr(bbs.birth,tmp));
fprintf(out,"Operator: ");
for(i=0;i<bbs.total_sysops && i<MAX_SYSOPS;i++) {
	if(i) {
		if(bbs.total_sysops>2)
			fprintf(out,", ");
		else
			fputc(SP,out);
		if(!(i%4))
			fprintf(out,"\r\n          ");
		if(i+1==bbs.total_sysops)
			fprintf(out,"and "); }
	fprintf(out,"%s",bbs.sysop[i]); }
fprintf(out,"\r\n");
fprintf(out,"Software: %-15.15s Nodes: %-5u "
	"Users: %-5u Doors: %u\r\n"
	,bbs.software,bbs.nodes,bbs.users,bbs.xtrns);
fprintf(out,"Download: %lu files in %u directories of "
	"%luMB total space\r\n"
	,bbs.files,bbs.dirs,bbs.megs);
fprintf(out,"Messages: %lu messages in %u sub-boards\r\n"
	,bbs.msgs,bbs.subs);
fprintf(out,"Networks: ");
for(i=0;i<bbs.total_networks && i<MAX_NETS;i++) {
	if(i) {
		if(bbs.total_networks>2)
			fprintf(out,", ");
		else
			fputc(SP,out);
		if(!(i%3))
			fprintf(out,"\r\n          ");
		if(i+1==bbs.total_networks)
			fprintf(out,"and "); }
	fprintf(out,"%s [%s]",bbs.network[i],bbs.address[i]); }
fprintf(out,"\r\n");
fprintf(out,"Terminal: ");
for(i=0;i<bbs.total_terminals && i<MAX_TERMS;i++) {
	if(i) {
		if(bbs.total_terminals>2)
			fprintf(out,", ");
		else
			fputc(SP,out);
		if(i+1==bbs.total_terminals)
			fprintf(out,"and "); }
	fprintf(out,"%s",bbs.terminal[i]); }
fprintf(out,"\r\n\r\n");
for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++)
	fprintf(out,"%-30.30s %12.12s %5u %-15.15s "
		"Minimum: %u\r\n"
		,i && !strcmp(bbs.number[i].location,bbs.number[i-1].location)
			? nulstr : bbs.number[i].location
		,bbs.number[i].number
		,bbs.number[i].max_rate,bbs.number[i].modem
		,bbs.number[i].min_rate);

fprintf(out,"\r\n");
for(i=0;i<5;i++) {
	if(!bbs.desc[i][0])
		break;
	fprintf(out,"%15s%s\r\n",nulstr,bbs.desc[i]); }

fprintf(out,"\r\n");
fprintf(out,"Entry created on %s by %s\r\n"
	,timestr(&bbs.created),bbs.user);
if(bbs.updated && bbs.userupdated[0])
	fprintf(out," Last updated on %s by %s\r\n"
		,timestr(&bbs.updated),bbs.userupdated);
if(bbs.verified && bbs.userverified[0])
	fprintf(out,"Last verified on %s by %s\r\n"
        ,timestr(&bbs.verified),bbs.userverified);
}

int main(int argc, char **argv)
{
	char str[128];
	int i,j,file,ff;
	FILE *in,*shrt,*lng;
	bbs_t bbs;
	sortstr_t **sortstr=NULL;

if((i=open("SBL.DAB",O_RDONLY|O_BINARY|O_DENYNONE))==-1) {
	printf("error opening SBL.DAB\n");
	return(1); }

if((in=fdopen(i,"rb"))==NULL) {
	printf("error opening SBL.DAB\n");
	return(1); }

if((shrt=fopen("SBBS.LST","wb"))==NULL) {
	printf("error opening/creating SBBS.LST\n");
	return(1); }

if((lng=fopen("SBBS_DET.LST","wb"))==NULL) {
	printf("error opening/creating SBBS_DET.LST\n");
	return(1); }

fprintf(shrt,"Synchronet BBS List exported from Vertrauen on %s\r\n"
			 "======================================================="
			 "\r\n\r\n"
	,unixtodstr(time(NULL),str));

fprintf(lng,"Detailed Synchronet BBS List exported from Vertrauen on %s\r\n"
			"================================================================"
			"\r\n\r\n"
	,unixtodstr(time(NULL),str));

printf("Sorting...");
fseek(in,0L,SEEK_SET);
i=j=0;
while(1) {
	if(!fread(&bbs,sizeof(bbs_t),1,in))
		break;
	j++;
	printf("%4u\b\b\b\b",j);
	if(!bbs.name[0] || strnicmp(bbs.software,"SYNCHRONET",10))
		continue;
	i++;
	strcpy(str,bbs.number[0].number);
	if((sortstr=(sortstr_t **)farrealloc(sortstr
		,sizeof(sortstr_t *)*i))==NULL) {
		printf("\r\n\7Memory allocation error\r\n");
		return(1); }
	if((sortstr[i-1]=(sortstr_t *)farmalloc(sizeof(sortstr_t)
		))==NULL) {
		printf("\r\n\7Memory allocation error\r\n");
		return(1); }
	strcpy(sortstr[i-1]->str,str);
	sortstr[i-1]->offset=j-1; }

qsort((void *)sortstr,i,sizeof(sortstr[0])
	,(int(*)(const void *, const void *))sortstr_cmp);

printf("\nCreating index...");
sprintf(str,"SBBSSORT.NDX");
if((file=open(str,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,S_IWRITE|S_IREAD))==-1) {
	printf("\n\7Error creating %s\n",str);
	return(1); }
for(j=0;j<i;j++)
	write(file,&sortstr[j]->offset,2);
printf("\n");

lseek(file,0L,SEEK_SET);
ff=0;
while(1) {
	if(read(file,&i,2)!=2)
		break;
	fseek(in,(long)i*sizeof(bbs_t),SEEK_SET);
	if(!fread(&bbs,sizeof(bbs_t),1,in))
		break;
	long_bbs_info(lng,bbs);
	if(ff)
		fprintf(lng,"\x0c\r\n");
	else
		fprintf(lng,"\r\n---------------------------------------------"
			"----------------------------------\r\n\r\n");
	ff=!ff;
	for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++)
		fprintf(shrt,"%-25.25s  %-30.30s  %12.12s  %u\r\n"
			,i ? "" : bbs.name,bbs.number[i].location,bbs.number[i].number
			,bbs.number[i].max_rate);
	}
}
