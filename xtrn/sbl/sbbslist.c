/* SBBSLIST.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Converts Synchronet BBS List (SBL.DAB) to text file */

#include "xsdk.h"
#include "telnet.h"
#include "sbldefs.h"

char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};
char *nulstr="";
char tmp[256];

extern int daylight=0;
extern long timezone=0L;

#undef  ERROR_VALUE
#define ERROR_VALUE			(GetLastError()-WSABASEERR)

#define SORT	TRUE
#define VERIFY	TRUE

typedef struct {

	time_t	date;
	ulong	count;
	ulong	attempts;
	short	offset;

    } sort_t;


int sort_cmp(sort_t **str1, sort_t **str2)
{
	int diff;
	
	/* sort descending by date */
	diff=((*str2)->date&0xffff0000)-((*str1)->date&0xffff0000);

	if(diff)
		return(diff);

	/* sort descending by verfication counter */
	diff=(((*str2)->count)-((*str1)->count));

	if(diff)
		return(diff);

	/* sort ascending by verification attempts */
	return(((*str1)->attempts)-((*str2)->attempts));
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
void truncsp(uchar *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
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

	if(gm==NULL) {
		sprintf(str,"Invalid time: %08lX",*intime);
		return(str);
	}

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
	struct tm* tm;

	if(!unix)
		strcpy(str,"00/00/00");
	else {
		tm=gmtime(&unix);
		if(tm==NULL)
			strcpy(str,"00/00/00");
		else {
			if(tm->tm_mon>11) {	  /* DOS leap year bug */
				tm->tm_mon=0;
				tm->tm_year++; }
			if(tm->tm_mday>31)
				tm->tm_mday=1;
			sprintf(str,"%02u/%02u/%02u",tm->tm_mon+1,tm->tm_mday
				,tm->tm_year%100); }
		}
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
for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++) {
	fprintf(out,"%-30.30s "
	   ,i && !strcmp(bbs.number[i].modem.location,bbs.number[i-1].modem.location)
			? nulstr : bbs.number[i].modem.location);
	if(bbs.number[i].modem.min_rate==0xffff)
		fprintf(out,"%s:%d\r\n"
			,bbs.number[i].telnet.addr
			,bbs.number[i].telnet.port);
	else
		fprintf(out,"%12.12s %5u %-15.15s "
			"Minimum: %u\r\n"
			,bbs.number[i].modem.number
			,bbs.number[i].modem.max_rate,bbs.number[i].modem.desc
			,bbs.number[i].modem.min_rate);
}
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

u_long resolve_ip(char *addr)
{
	HOSTENT*	host;

	if(isdigit(addr[0]))
		return(inet_addr(addr));
	if ((host=gethostbyname(addr))==NULL) {
//		printf("!ERROR resolving hostname: %s\n",addr);
		return(0);
	}
	return(*((ulong*)host->h_addr_list[0]));
}

int telnet_negotiate(SOCKET sock, uchar* buf, int rd, int max_rd)
{
	int		i;
	int		rsplen;
	uchar	rsp[512];

	return(rd);

	do {
		rsplen=0;
		for(i=0; i<rd && rsplen<sizeof(rsp)-2;i++) {
			if(buf[i]!=TELNET_IAC)
				continue;
			i++;
			printf("telnet cmd: %02X %02X\n"
				,buf[i],buf[i+1]);
			if(buf[i]==TELNET_DO) {
				rsp[rsplen++]=TELNET_IAC;
				rsp[rsplen++]=TELNET_WILL;
				rsp[rsplen++]=buf[i+1];
			}
			if(buf[i]==TELNET_DONT) {
				rsp[rsplen++]=TELNET_IAC;
				rsp[rsplen++]=TELNET_WONT;
				rsp[rsplen++]=buf[i+1];
			}
			i++;
		}
		if(!rsplen)
			break;
		printf("telnet rsp: ");
		for(i=0; i<rsplen; i++) 
			printf("%02X  ",rsp[i]);	
		printf("\n");
		send(sock,rsp,rsplen,0);
		Sleep(3000);
		rd=recv(sock,buf,max_rd,0);

	} while(rd>0);

	return(rd);

}

int sockreadline(SOCKET socket, char* buf, int len, int timeout)
{
	char	ch;
	int		i,rd=0;
	time_t	start;
	fd_set	socket_set;
	struct timeval	tv;
	
	start=time(NULL);
	while(rd<len-1) {

		tv.tv_sec=0;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(socket,&socket_set);

		i=select(socket+1,&socket_set,NULL,NULL,&tv);

		if(i<1) {
			if(i==0) {
				if((time(NULL)-start)>=timeout) 
					return(0);
				mswait(1);
				continue;
			}
			return(0);
		}
		i=recv(socket, &ch, 1, 0);
		if(i<1) 
			return(0);
		if(ch=='\n' && rd>=1) {
			break;
		}	
		buf[rd++]=ch;
	}
	buf[rd-1]=0;
	
	return(rd);
}


BOOL check_imsg_support(ulong ip_addr)
{
	char	buf[128];
	BOOL	success=FALSE;
	int		rd;
	SOCKET	sock;
	SOCKADDR_IN	addr;

	printf("\r\nFinger: ");
	if((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_IP)) == INVALID_SOCKET) {
		printf("!Error %d opening socket\n",ERROR_VALUE);
		return(FALSE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;

	if(bind(sock, (struct sockaddr *) &addr, sizeof (addr))!=0) {
		printf("!bind error %d\n",ERROR_VALUE);
		closesocket(sock);
		return(FALSE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.S_un.S_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(79);	/* finger */

	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr))!=0)  {
		printf("!connect error %d\n",ERROR_VALUE);
		closesocket(sock);
		return(FALSE);
	}

	/* Send query */
	strcpy(buf,"?ver\r\n");
	send(sock,buf,strlen(buf),0);

	/* Get response */
	while((rd=sockreadline(sock,buf,sizeof(buf),30))>0) {
		printf("%s\n",buf);
		if(strstr(buf,"Synchronet")) {
			success=TRUE;
			break;
		}
	}

	closesocket(sock);

	if(success==FALSE)
		return(FALSE);

	/* Check SMTP server */
	printf("SMTP: ");
	success=FALSE;

	if((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_IP)) == INVALID_SOCKET) {
		printf("!Error %d opening socket\n",ERROR_VALUE);
		return(FALSE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;

	if(bind(sock, (struct sockaddr *) &addr, sizeof (addr))!=0) {
		printf("!bind error %d\n",ERROR_VALUE);
		closesocket(sock);
		return(FALSE);
	}

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.S_un.S_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(25);	/* SMTP */

	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr))!=0)  {
		printf("!connect error %d\n",ERROR_VALUE);
		closesocket(sock);
		return(FALSE);
	}

	/* Get response */
	while((rd=sockreadline(sock,buf,sizeof(buf),10))>0) {
		printf("%s\r\n",buf);
		if(strstr(buf,"Synchronet")) {
			success=TRUE;
			break;
		}
	}

	closesocket(sock);

	return(success);
}

int main(int argc, char **argv)
{
	char	str[128],name[128],*location,nodes[32],*sysop;
	char	sysop_email[128];
	char	buf[256];
	char	verify_result[128];
	char	version[128];
	char*	p;
	char*	sp;
	char*	tp;
	char*	for_os;
	char*	fontstr="<FONT FACE=\"Arial\" SIZE=\"-1\">";
	char	telnet_addr_buf[128];
	char*	telnet_addr;
	char	telnet_portstr[32];
	ushort	telnet_port;
	BOOL	fingered;
	BOOL	verified;
	ushort	index;
	int		i,j,file,ff,rd;
	long	l;
	ulong	ip_addr;
	ulong	total_systems;
	ulong	total_attempts=0;
	ulong	total_verified=0;
	FILE	*in,*shrt,*lng,*html;
	FILE*	ibbs;
	ulong	ip_list[1000];
	ulong	ip_total=0;
	ulong	ip;
	bbs_t	bbs;
	sort_t **sort=NULL;
	time_t	now;

	/* socket stuff */
	SOCKET	sock;
	SOCKADDR_IN	addr;
	int		status;             /* Status Code */
	WSADATA WSAData;

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))!=0) {
	    printf("!WinSock startup ERROR %d\n", status);
		return(1);
	}

	if(_putenv("TZ=UCT0"))
		printf("!putenv() FAILED");
	tzset();

	now=time(NULL);

	if((i=_sopen("SBL.DAB",O_RDWR|O_BINARY,SH_DENYNO,S_IREAD|S_IWRITE))==-1) {
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

	if((html=fopen("sbbslist.html","w"))==NULL) {
		printf("error opening/creating sbbslist.html\n");
		return(1); }

	if((ibbs=fopen("sbbsimsg.lst","w"))==NULL) {
		printf("error opening/creating sbbsimsg.lst\n");
		return(1); }

	fprintf(shrt,"Synchronet BBS List exported from Vertrauen on %s\r\n"
				 "======================================================="
				 "\r\n\r\n"
		,unixtodstr(time(NULL),str));

	fprintf(lng,"Detailed Synchronet BBS List exported from Vertrauen on %s\r\n"
				"================================================================"
				"\r\n\r\n"
		,unixtodstr(time(NULL),str));

	fprintf(html,"<HTML><HEAD><TITLE>Synchronet BBS List</TITLE></HEAD>\n");
	fprintf(html,"<BODY><FONT FACE=\"Arial\" SIZE=\"-1\">\n");

	printf("Sorting...");
	fseek(in,0L,SEEK_SET);
	i=j=0;
	while(1) {
		if(!fread(&bbs,sizeof(bbs_t),1,in))
			break;
		j++;
		printf("%4u\b\b\b\b",j);
		if(!bbs.name[0] || strnicmp(bbs.software,"SYNCHRONET",10)) {
//			printf("%s\n",bbs.software);
			continue;
		}
		i++;
		if((sort=(sort_t **)REALLOC(sort
			,sizeof(sort_t *)*i))==NULL) {
			printf("\r\n\7Memory allocation error\r\n");
			return(1); }
		if((sort[i-1]=(sort_t *)LMALLOC(sizeof(sort_t)
			))==NULL) {
			printf("\r\n\7Memory allocation error\r\n");
			return(1); }
		sort[i-1]->date=bbs.verified;
		sort[i-1]->count=bbs.verification_count;
		sort[i-1]->attempts=bbs.verification_attempts;
		sort[i-1]->offset=j-1; 
	}
	total_systems=i;

#if SORT
	qsort((void *)sort,total_systems,sizeof(sort[0])
		,(int(*)(const void *, const void *))sort_cmp);
#endif
	printf(" Done.\n");
		
	printf("Creating index...");
	sprintf(str,"SBBSSORT.NDX");
	if((file=open(str,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,S_IWRITE|S_IREAD))==-1) {
		printf("\n\7Error creating %s\n",str);
		return(1); }
	for(j=0;j<(int)total_systems;j++)
		write(file,&sort[j]->offset,2);
	lseek(file,0L,SEEK_SET);
	printf(" Done.\n");


	printf("Creating lists...\n");

	fprintf(html,"<CENTER>");
	fprintf(html,"<H1><I><A HREF=http://www.synchro.net>Synchronet"
		"</A> BBS List</H1></I>\n");
	
	fprintf(html,"(%d systems) exported from "
		"<B><A HREF=http://vert.synchro.net>Vertrauen</A></B> on %s\n"
		,total_systems ,timestr(&now));

	fprintf(html,"<P></CENTER>\n");

	fprintf(html,"<TABLE WIDTH=\"100%%\">\n");
	fprintf(html,"<COLGROUP ALIGN=LEFT><COLGROUP ALIGN=LEFT>"
		"<COLGROUP ALIGN=CENTER><COLGROUP ALIGN=CENTER>"
		"<COLGROUP ALIGN=RIGHT><COLGROUP ALIGN=CENTER>\n");
	fprintf(html,"<TR BGCOLOR=\"#000000\">\n");
	fprintf(html,"<TH><FONT FACE=\"Arial\" SIZE=\"-1\" COLOR=\"#FFFFFF\">BBS Name\n");
	fprintf(html,"<TH><FONT FACE=\"Arial\" SIZE=\"-1\" COLOR=\"#FFFFFF\">Sysop\n");
	fprintf(html,"<TH><FONT FACE=\"Arial\" SIZE=\"-1\" COLOR=\"#FFFFFF\">Location\n");
	fprintf(html,"<TH><FONT FACE=\"Arial\" SIZE=\"-1\" COLOR=\"#FFFFFF\">Nodes\n");
	fprintf(html,"<TH><FONT FACE=\"Arial\" SIZE=\"-1\" COLOR=\"#FFFFFF\">Modem/Telnet Address\n");
	fprintf(html,"<TH><FONT FACE=\"Arial\" SIZE=\"-1\" COLOR=\"#FFFFFF\">Verification Results\n");
	ff=0;
	while(1) {
		if(read(file,&index,2)!=2)
			break;
		fseek(in,(long)index*sizeof(bbs_t),SEEK_SET);
		if(!fread(&bbs,sizeof(bbs_t),1,in))
			break;
		long_bbs_info(lng,bbs);
		if(ff)
			fprintf(lng,"\x0c\r\n");
		else
			fprintf(lng,"\r\n---------------------------------------------"
				"----------------------------------\r\n\r\n");
		ff=!ff;
		verified=FALSE;
		fingered=FALSE;
		total_attempts++;
		for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++) {
			if(!i) {
				if(bbs.sysop_email[0]) {
					sprintf(sysop_email,"<A HREF=mailto:%s>%s</A>",bbs.sysop_email,bbs.sysop[0]);
					sysop=sysop_email;
				} else
					sysop=bbs.sysop[0];
				sprintf(nodes,"%u",bbs.nodes);
			} else {
				sysop="";
				nodes[0]=0;
			}
			if(i && !stricmp(bbs.number[i].modem.number,bbs.number[i-1].modem.number))
				continue;	// duplicate
			if(i && !stricmp(bbs.number[i].modem.location,bbs.number[i-1].modem.location))
				location="";
			else
				location=bbs.number[i].modem.location;
			fprintf(shrt,"%-25.25s %-25.25s %s\r\n"
				,i ? "" : bbs.name, location
				,bbs.number[i].modem.number);
			if(!i) {
				fprintf(html,"<A NAME=\"%s.index\">",bbs.name);
				fprintf(html,"<TR BGCOLOR=\"#EEEEEE\">");
			} else
				fprintf(html,"<TR>");
				sprintf(name,"<A HREF=\"#%s\">%s</A>",bbs.name,bbs.name);

			if(bbs.number[i].modem.min_rate==0xffff /* && bbs.name[0]=='E' */) {

				telnet_port=bbs.number[i].telnet.port;
				if(telnet_port==0)
					telnet_port=23;
				strcpy(telnet_addr_buf,bbs.number[i].telnet.addr);
				telnet_addr=telnet_addr_buf;

				if(!strnicmp(telnet_addr,"TELNET:",7))
					telnet_addr+=7;
				if(!strnicmp(telnet_addr,"//",2))
					telnet_addr+=2;
				p=strchr(telnet_addr,':');
				if(p!=NULL) {
					*p=0;
					telnet_port=atoi(p+1);
				}

#if !VERIFY	/* set to 1 for no-verification */
				verified=TRUE;
				strcpy(verify_result,"<B>v3.00x for Win32</B>");
#else
				printf("Verifying %d/%d %s:%d "
					,total_attempts,total_systems,telnet_addr,telnet_port);

				ip_addr=resolve_ip(telnet_addr);
				if(!ip_addr) 
					strcpy(verify_result,"bad hostname");
				else {

					if((sock = socket(AF_INET,SOCK_STREAM,IPPROTO_IP)) == INVALID_SOCKET) {
						printf("\n\7Error %d opening socket",ERROR_VALUE);
						return(1);
					}

					memset(&addr,0,sizeof(addr));
					addr.sin_family = AF_INET;

					if(bind(sock, (struct sockaddr *) &addr, sizeof (addr))!=0) {
						closesocket(sock);
						printf("!ERROR %d binding to socket %d",ERROR_VALUE, sock);
						return(1);
					}

					memset(&addr,0,sizeof(addr));
					addr.sin_addr.S_un.S_addr = ip_addr;
					addr.sin_family = AF_INET;
					addr.sin_port   = htons(telnet_port);

					if(connect(sock, (struct sockaddr *)&addr, sizeof(addr))!=0) 
						sprintf(verify_result,"no connect (%ld)"
							,ERROR_VALUE);
					else {
						l=1;
						ioctlsocket(sock, FIONBIO, &l);
						Sleep(3000);

						buf[0]=0;
						rd=recv(sock,buf,sizeof(buf)-1,0);
						if((rd=telnet_negotiate(sock,buf,rd,sizeof(buf)-1))<1)
							sprintf(verify_result,"no data (%ld)"
								,rd==SOCKET_ERROR ? ERROR_VALUE : rd);
						else {
							
							buf[rd]=0;
							sp=buf+(rd-1);
							while(*sp && sp!=buf) sp--;	// Skip garbage (with null)
							if(*sp==0)
								sp++;
							p=strstr(sp,"Synchronet");
							if(p!=NULL) {
								verified=TRUE;
								for_os=strstr(sp," for ");
								if(for_os==NULL) 
									for_os="";
								p=strstr(sp,"Version ");
								if(p==NULL)
									version[0]=0;
								else {
									p+=8;	/* skip "version" */
									tp=strchr(p,'\r');
									if(tp!=NULL) *tp=0;
									truncsp(p);
									for_os[12]=0;
									tp=strchr(for_os+5,' ');
									if(tp!=NULL) *tp=0;
									truncsp(for_os);
									sprintf(version,"v%s%s",p,for_os);
								}
								sprintf(verify_result,"<B>%s</B>"
									,version[0] ? version : "Verified");
							} else {
								printf("rd=%d buf='%s' sp='%s'\n",rd,buf,sp);
								sprintf(verify_result,"non-Synchronet");
							}
							/* Check Finger */
							for(ip=0;ip<ip_total;ip++)
								if(ip_addr==ip_list[ip])
									break;
							if(ip>=ip_total && !fingered) {	/* not already checked */
								ip_list[ip_total++]=ip_addr;
								if(check_imsg_support(ip_addr)) {
									fingered=TRUE;
									printf("[IM]");
									fprintf(ibbs,"%-63s %s\n"
										,telnet_addr, inet_ntoa(addr.sin_addr));
								}
							}
						}
					}

					closesocket(sock);
				}

				printf("%s\n",verify_result);
				bbs.verification_attempts++;
#endif

				if(telnet_port==23)
					telnet_portstr[0]=0;
				else
					sprintf(telnet_portstr,":%d",telnet_port);

				fprintf(html,"<TD><B>%s%s</B><TD>%s%s<TD>%s%s<TD>%s%s<TD%s>"
					"<A HREF=telnet://%s%s>%s%s%s%s%s</A><TD%s>%s%s\n"
					,fontstr,i ? "":name
					,fontstr,sysop
					,fontstr,location
					,fontstr,nodes
					,i ? " BGCOLOR=\"#EEEEEE\"":""
					,telnet_addr, telnet_portstr
						,fontstr
						,verified ? "<B>":"", telnet_addr, telnet_portstr
						, verified ? "</B>":""
					,i ? " BGCOLOR=\"#EEEEEE\"":""
					,fontstr,verify_result);
			} else
				fprintf(html,"<TD><B>%s%s</B><TD>%s%s<TD>%s%s<TD>%s%s<TD%s>%s%s"
					"<TD%s>%s%s\n"
					,fontstr,i ? "":name
					,fontstr,sysop
					,fontstr,location
					,fontstr,nodes
					,i ? " BGCOLOR=\"#EEEEEE\"":""
					,fontstr,bbs.number[i].modem.number
					,i ? " BGCOLOR=\"#EEEEEE\"":""
					,fontstr,"N/A");
		} /* for(numbers) */
#if VERIFY
		if(verified) {
			total_verified++;
			bbs.verified=time(NULL);
			bbs.verification_count++;
			strcpy(bbs.userverified,"SBBS List Verifier");
		}
		fseek(in,(long)index*sizeof(bbs_t),SEEK_SET);
		fwrite(&bbs,sizeof(bbs_t),1,in);
#endif
	}
	fprintf(html,"</TABLE>\n");

	now=time(NULL);
	fprintf(html,"<CENTER>\n");
	fprintf(html,"%d systems verified from "
		"<B><A HREF=http://vert.synchro.net>Vertrauen</A></B> on %s\n"
		,total_verified ,timestr(&now));
	fprintf(html,"<H1><I>Detailed Synchronet BBS List</I></H1>\n");
	fprintf(html,"</CENTER>\n");

	/* Generate Detailed List */
	lseek(file,0L,SEEK_SET);
	while(1) {
		if(read(file,&index,2)!=2)
			break;
		fseek(in,(long)index*sizeof(bbs_t),SEEK_SET);
		if(!fread(&bbs,sizeof(bbs_t),1,in))
			break;

		fprintf(html,"<P><A NAME=\"%s\">\n",bbs.name);

		fprintf(html,"<H2><A HREF=\"#%s.index\">%s</A></H2>",bbs.name,bbs.name);
		fprintf(html,"<FONT FACE=\"Arial\" SIZE=\"-1\">\n");

		fprintf(html,"Online since: %s<BR>\n",unixtodstr(bbs.birth,tmp));

		if(bbs.sysop_email[0]) {
			sprintf(sysop_email,"<A HREF=mailto:%s>%s</A>",bbs.sysop_email,bbs.sysop[0]);
			sysop=sysop_email;
		} else
			sysop=bbs.sysop[0];

		fprintf(html,"Operator: %s", sysop);

		for(i=1;i<bbs.total_sysops && i<MAX_SYSOPS;i++) {
			if(bbs.total_sysops>2)
				fprintf(html,", ");
			else
				fputc(SP,html);
			if(i+1==bbs.total_sysops)
				fprintf(html,"and "); 
			fprintf(html,"%s",bbs.sysop[i]); 
		}
		fprintf(html,"<BR>\n");

		if(bbs.web_url[0]) 
			fprintf(html,"Web-site: <A HREF=http://%s>%s</A><BR>\n",bbs.web_url,bbs.web_url);

		fprintf(html,"Nodes: %u, "
			"Users: %u, Doors: %u<BR>\n"
			,bbs.nodes,bbs.users,bbs.xtrns);
		fprintf(html,"Download: %lu files in %u directories of "
			"%luMB total space<BR>\n"
			,bbs.files,bbs.dirs,bbs.megs);
		fprintf(html,"Messages: %lu messages in %u sub-boards<BR>\n"
			,bbs.msgs,bbs.subs);

		if(bbs.total_networks) {
			fprintf(html,"Networks: ");
			for(i=0;i<bbs.total_networks && i<MAX_NETS;i++) {
				if(i) {
					if(bbs.total_networks>2)
						fprintf(html,", ");
					else
						fputc(SP,html);
					if(!(i%2))
						fprintf(html,"<BR>");
					if(i+1==bbs.total_networks)
						fprintf(html,"and "); 
				}
				fprintf(html,"%s [%s]",bbs.network[i],bbs.address[i]); }
			fprintf(html,"<BR>\n");
		}

		if(bbs.total_terminals) {
			fprintf(html,"Terminal: ");
			for(i=0;i<bbs.total_terminals && i<MAX_TERMS;i++) {
				if(i) {
					if(bbs.total_terminals>2)
						fprintf(html,", ");
					else
						fputc(SP,html);
					if(i+1==bbs.total_terminals)
						fprintf(html,"and "); }
				fprintf(html,"%s",bbs.terminal[i]); }
			fprintf(html,"<BR>\n");
		}

		fprintf(html,"<BR>\n");
		for(i=0;i<bbs.total_numbers && i<MAX_NUMBERS;i++) {
			if(bbs.number[i].modem.min_rate==0xffff) {

				telnet_port=bbs.number[i].telnet.port;
				if(telnet_port==0)
					telnet_port=23;
				strcpy(telnet_addr_buf,bbs.number[i].telnet.addr);
				telnet_addr=telnet_addr_buf;

				if(!strnicmp(telnet_addr,"TELNET:",7))
					telnet_addr+=7;
				if(!strnicmp(telnet_addr,"//",2))
					telnet_addr+=2;
				p=strchr(telnet_addr,':');
				if(p!=NULL) {
					*p=0;
					telnet_port=atoi(p+1);
				}

				if(telnet_port==23)
					telnet_portstr[0]=0;
				else
					sprintf(telnet_portstr,":%d",telnet_port);

				fprintf(html,"<A HREF=telnet://%s%s>telnet://%s%s</A>"
					,telnet_addr
					,telnet_portstr
					,telnet_addr
					,telnet_portstr);
			} else
				fprintf(html,"%s %u %s "
					"Minimum: %u"
					,bbs.number[i].modem.number
					,bbs.number[i].modem.max_rate,bbs.number[i].modem.desc
					,bbs.number[i].modem.min_rate);

			fprintf(html," %s<BR>\n"
			   ,i && !strcmp(bbs.number[i].modem.location,bbs.number[i-1].modem.location)
					? nulstr : bbs.number[i].modem.location);
		}
		fprintf(html,"<BR>\n");

		fprintf(html,"<BLOCKQUOTE>\n");
		for(i=0;i<5;i++) {
			if(!bbs.desc[i][0])
				break;
			fprintf(html,"%s<BR>\n",bbs.desc[i]); 
		}
		fprintf(html,"</BLOCKQUOTE>\n");

		fprintf(html,"<PRE>\n");
		fprintf(html,"Entry created on %s by %s\n"
			,timestr(&bbs.created),bbs.user);
		if(bbs.updated && bbs.userupdated[0])
			fprintf(html," Last updated on %s by %s\n"
				,timestr(&bbs.updated),bbs.userupdated);
		if(bbs.verified && bbs.userverified[0])
			fprintf(html,"Last verified on %s by %s\n"
				,timestr(&bbs.verified),bbs.userverified);

		fprintf(html,"</PRE></P>\n");
	}
	fprintf(html,"<CENTER><H1>End</H1></CENTER>\n");

	fprintf(html,"<P>If you are a sysop of a <B>Synchronet BBS</B> and you would "
		"like to add your system to this list, please do one of the following:\n");
	fprintf(html,"<UL>\n");
	fprintf(html,"<LI>Install <I>Synchronet BBS List <B>v2.00+</B></I> on your BBS and "
		"link it into the <B>SYNCDATA</B> message conference (on <B>DOVE-Net</B> or <B>FidoNet</B>)\n");
	fprintf(html,"<LI><B>OR</B> log on to <A HREF=telnet://vert.synchro.net>Vertrauen</A> and "
		"manually add your system into the online BBS List database.\n");
	fprintf(html,"</UL>\n");
	fprintf(html,"</BODY></HTML>\n");
	printf(" Done.\n");
	return(0);
}
