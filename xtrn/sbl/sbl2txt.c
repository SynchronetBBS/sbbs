/* SBL2TXT.C */

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
    ,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,gm->tm_year%100
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
for(i=0;i<bbs.total_sysops;i++) {
	if(i) {
		if(bbs.total_sysops>2)
			fprintf(out,", ");
		else
			fputc(' ',out);
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
for(i=0;i<bbs.total_networks;i++) {
	if(i) {
		if(bbs.total_networks>2)
			fprintf(out,", ");
		else
			fputc(' ',out);
		if(!(i%3))
			fprintf(out,"\r\n          ");
		if(i+1==bbs.total_networks)
			fprintf(out,"and "); }
	fprintf(out,"%s [%s]",bbs.network[i],bbs.address[i]); }
fprintf(out,"\r\n");
fprintf(out,"Terminal: ");
for(i=0;i<bbs.total_terminals;i++) {
	if(i) {
		if(bbs.total_terminals>2)
			fprintf(out,", ");
		else
			fputc(' ',out);
		if(i+1==bbs.total_terminals)
			fprintf(out,"and "); }
	fprintf(out,"%s",bbs.terminal[i]); }
fprintf(out,"\r\n\r\n");
for(i=0;i<bbs.total_numbers;i++)
	fprintf(out,"%-30.30s %12.12s %5u %-15.15s "
		"Minimum: %u\r\n"
		,i && !strcmp(bbs.number[i].modem.location,bbs.number[i-1].modem.location)
			? nulstr : bbs.number[i].modem.location
		,bbs.number[i].modem.number
		,bbs.number[i].modem.max_rate
		,bbs.number[i].modem.desc
		,bbs.number[i].modem.min_rate);

fprintf(out,"\r\n");
for(i=0;i<5;i++) {
	if(!bbs.desc[i][0])
		break;
	fprintf(out,"%15s%s\r\n",nulstr,bbs.desc[i]); }

fprintf(out,"\r\n");
fprintf(out,"Entry created on %s by %s\r\n"
	,timestr(&bbs.created),bbs.user);
fprintf(out," Last updated on %s\r\n\r\n",timestr(&bbs.updated));
}


void main(int argc, char **argv)
{
	char	software[16]="";
	char	telnet_port[16];
	int i,in;
	FILE *out;
	bbs_t bbs;

for(i=1;i<argc;i++)
	if(argv[i][0]=='s' && argv[i][1]=='=')
		sprintf(software,"%.15s",argv[i]+2);

if((in=open("SBL.DAB",O_RDONLY|O_BINARY))==-1) {
	printf("error opening SBL.DAB\n");
	return; }

if((out=fopen("SBL.TXT","wb"))==NULL) {
	printf("error opening/creating SBL.TXT\n");
	return; }

while(!eof(in)) {
	read(in,&bbs,sizeof(bbs_t));
	if(!bbs.name[0])
		continue;
	if(software[0] && strnicmp(bbs.software,software,strlen(software)))
		continue;
	// long_bbs_info(out,bbs);
	for(i=0;i<bbs.total_numbers;i++)
		if(bbs.number[i].modem.min_rate==0xffff) {
			if(bbs.number[i].telnet.port && bbs.number[i].telnet.port!=23)
				sprintf(telnet_port,":%u",bbs.number[i].telnet.port);
			else
				telnet_port[0]=0;
			fprintf(out,"%-25.25s  telnet://%s%s\r\n"
					,bbs.name
					,bbs.number[i].telnet.addr
					,telnet_port);
		} else
			fprintf(out,"%-25.25s  %12.12s  %5u  %s\r\n"
				,bbs.name,bbs.number[i].modem.number
				,bbs.number[i].modem.max_rate
				,bbs.number[i].modem.desc);
	}
close(in);
fclose(out);
}
