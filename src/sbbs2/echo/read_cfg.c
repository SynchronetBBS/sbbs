/* READ_CFG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Portions written by Allen Christiansen 1994-1996 						*/

#include <dos.h>
#include <mem.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include <fcntl.h>
#include <share.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <sys\stat.h>

#include "crc32.h"
#include "sbbsdefs.h"
#include "sbbsecho.h"

#ifdef __WATCOMC__
    #define O_DENYNONE SH_DENYNO
#endif

extern uchar node_swap;
extern long misc;
extern config_t cfg;

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.   All files are opened in BINARY mode.            */
/****************************************************************************/
int nopen(char *str, int access)
{
    char logstr[256];
    int file,share,count=0;

if(access&O_DENYNONE) {
	access&=~O_DENYNONE;
	share=SH_DENYNO; }
else if(access==O_RDONLY) share=SH_DENYWR;
else share=SH_DENYRW;
while(((file=sopen(str,O_BINARY|access,share,S_IWRITE))==-1)
    && errno==EACCES && count++<LOOP_NOPEN);
if(file==-1 && errno==EACCES)
    printf("\7\nNOPEN: ACCESS DENIED\n\7");
return(file);
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.                                                               */
/****************************************************************************/
FILE *fnopen(int *file, char *str, int access)
{
    char mode[128];
    FILE *stream;

if(access&O_WRONLY) {
	access&=~O_WRONLY;
	access|=O_RDWR; }	 /* fdopen can't open WRONLY */

if(((*file)=nopen(str,access))==-1)
    return(NULL);

if(access&O_APPEND) {
	if(access&(O_RDONLY|O_RDWR))
        strcpy(mode,"a+");
    else
        strcpy(mode,"a"); }
else {
	if(access&(O_WRONLY|O_RDWR))
        strcpy(mode,"r+");
    else
        strcpy(mode,"r"); }
stream=fdopen((*file),mode);
if(stream==NULL) {
	printf("\7\nFDOPEN(%s) FAILED\n",mode);
    close(*file);
    return(NULL); }
setvbuf(stream,NULL,_IOFBF,16*1024);
return(stream);
}
/******************************************************************************
 Here we take a string and put a terminator in place of the first TAB or SPACE
******************************************************************************/
char *cleanstr(char *instr)
{
	int i;

for(i=0;instr[i];i++)
	if((uchar)instr[i]<=SP)
		break;
instr[i]=0;
return(instr);
}
/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(char *instr)
{
	char *p,str[51];
    faddr_t addr;

sprintf(str,"%.50s",instr);
cleanstr(str);
if(!stricmp(str,"ALL")) {
	addr.zone=addr.net=addr.node=addr.point=0xffff;
	return(addr); }
addr.zone=addr.net=addr.node=addr.point=0;
if((p=strchr(str,':'))!=NULL) {
	if(!strnicmp(str,"ALL:",4))
		addr.zone=0xffff;
	else
		addr.zone=atoi(str);
	p++;
	if(!strnicmp(p,"ALL",3))
		addr.net=0xffff;
	else
		addr.net=atoi(p); }
else {
#ifdef SCFG
    if(total_faddrs)
        addr.zone=faddr[0].zone;
    else
#endif
        addr.zone=1;
    addr.net=atoi(str); }
if(!addr.zone)              /* no such thing as zone 0 */
    addr.zone=1;
if((p=strchr(str,'/'))!=NULL) {
	p++;
	if(!strnicmp(p,"ALL",3))
		addr.node=0xffff;
	else
		addr.node=atoi(p); }
else {
	if(!addr.net) {
#ifdef SCFG
		if(total_faddrs)
			addr.net=faddr[0].net;
		else
#endif
			addr.net=1; }
	addr.node=atoi(str); }
if((p=strchr(str,'.'))!=NULL) {
	p++;
	if(!strnicmp(p,"ALL",3))
		addr.point=0xffff;
	else
		addr.point=atoi(p); }
return(addr);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(faddr_t addr)
{
    static char str[25];
	char tmp[25];

if(addr.zone==0xffff)
	strcpy(str,"ALL");
else {
	sprintf(str,"%u:",addr.zone);
	if(addr.net==0xffff)
		strcat(str,"ALL");
	else {
		sprintf(tmp,"%u/",addr.net);
		strcat(str,tmp);
		if(addr.node==0xffff)
			strcat(str,"ALL");
		else {
			sprintf(tmp,"%u",addr.node);
			strcat(str,tmp);
			if(addr.point==0xffff)
				strcat(str,".ALL");
			else if(addr.point) {
				sprintf(tmp,".%u",addr.point);
				strcat(str,tmp); } } } }
return(str);
}
/******************************************************************************
 This function returns the number of the node in the SBBSECHO.CFG file which
 matches the address passed to it (or cfg.nodecfgs if no match).
 ******************************************************************************/
int matchnode(faddr_t addr, int exact)
{
	int i;

if(exact!=2) {
	for(i=0;i<cfg.nodecfgs;i++) 				/* Look for exact match */
		if(!memcmp(&cfg.nodecfg[i].faddr,&addr,sizeof(faddr_t)))
			break;
	if(exact || i<cfg.nodecfgs)
		return(i); }

for(i=0;i<cfg.nodecfgs;i++) 					/* Look for point match */
	if(cfg.nodecfg[i].faddr.point==0xffff
		&& addr.zone==cfg.nodecfg[i].faddr.zone
		&& addr.net==cfg.nodecfg[i].faddr.net
		&& addr.node==cfg.nodecfg[i].faddr.node)
		break;
if(i<cfg.nodecfgs)
    return(i);

for(i=0;i<cfg.nodecfgs;i++) 					/* Look for node match */
	if(cfg.nodecfg[i].faddr.node==0xffff
		&& addr.zone==cfg.nodecfg[i].faddr.zone
		&& addr.net==cfg.nodecfg[i].faddr.net)
		break;
if(i<cfg.nodecfgs)
    return(i);

for(i=0;i<cfg.nodecfgs;i++) 					/* Look for net match */
	if(cfg.nodecfg[i].faddr.net==0xffff
		&& addr.zone==cfg.nodecfg[i].faddr.zone)
		break;
if(i<cfg.nodecfgs)
	return(i);

for(i=0;i<cfg.nodecfgs;i++) 					/* Look for total wild */
	if(cfg.nodecfg[i].faddr.zone==0xffff)
        break;
return(i);
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

void read_cfg()
{
	uchar str[1025],tmp[512],*p,*tp;
	short attr;
	int i,j,file;
	FILE *stream;
	faddr_t addr,route_addr;


/****** READ IN SBBSECHO.CFG FILE *******/

printf("\n\nReading %s\n",cfg.cfgfile);
if((stream=fnopen(&file,cfg.cfgfile,O_RDONLY))==NULL) {
    printf("Unable to open %s for read.\n",cfg.cfgfile);
    exit(1); }

cfg.maxpktsize=DFLT_PKT_SIZE;
cfg.maxbdlsize=DFLT_BDL_SIZE;
cfg.badecho=-1;
cfg.log=LOG_DEFAULTS;

while(1) {
    if(!fgets(str,256,stream))
        break;
    truncsp(str);
    p=str;
    while(*p && *p<=SP) p++;
    if(*p==';')
        continue;
    sprintf(tmp,"%-.25s",p);
    tp=strchr(tmp,SP);
    if(tp)
        *tp=0;                              /* Chop off at space */
    strupr(tmp);                            /* Convert code to uppercase */
    while(*p>SP) p++;                       /* Skip code */
    while(*p && *p<=SP) p++;                /* Skip white space */

    if(!strcmp(tmp,"PACKER")) {             /* Archive Definition */
		if((cfg.arcdef=(arcdef_t *)REALLOC(cfg.arcdef
            ,sizeof(arcdef_t)*(cfg.arcdefs+1)))==NULL) {
			printf("\nError allocating %u bytes of memory for arcdef #%u.\n"
                ,sizeof(arcdef_t)*(cfg.arcdefs+1),cfg.arcdefs+1);
            exit(1); }
        sprintf(cfg.arcdef[cfg.arcdefs].name,"%-.25s",p);
        tp=cfg.arcdef[cfg.arcdefs].name;
        while(*tp && *tp>SP) tp++;
        *tp=0;
        while(*p && *p>SP) p++;
        while(*p && *p<=SP) p++;
        cfg.arcdef[cfg.arcdefs].byteloc=atoi(p);
        while(*p && *p>SP) p++;
        while(*p && *p<=SP) p++;
        sprintf(cfg.arcdef[cfg.arcdefs].hexid,"%-.25s",p);
        tp=cfg.arcdef[cfg.arcdefs].hexid;
        while(*tp && *tp>SP) tp++;
        *tp=0;
        while(fgets(str,256,stream) && strnicmp(str,"END",3)) {
            p=str;
            while(*p && *p<=SP) p++;
            if(!strnicmp(p,"PACK ",5)) {
                p+=5;
                while(*p && *p<=SP) p++;
                sprintf(cfg.arcdef[cfg.arcdefs].pack,"%-.80s",p);
                truncsp(cfg.arcdef[cfg.arcdefs].pack);
                continue; }
            if(!strnicmp(p,"UNPACK ",7)) {
                p+=7;
                while(*p && *p<=SP) p++;
                sprintf(cfg.arcdef[cfg.arcdefs].unpack,"%-.80s",p);
                truncsp(cfg.arcdef[cfg.arcdefs].unpack); } }
        ++cfg.arcdefs;
        continue; }

	if(!strcmp(tmp,"REGNUM"))
		continue;

	if(!strcmp(tmp,"NOTIFY")) {
		cfg.notify=atoi(cleanstr(p));
		continue; }

	if(!strcmp(tmp,"LOG")) {
		cleanstr(p);
		if(!stricmp(p,"ALL"))
			cfg.log=0xffffffffUL;
		else if(!stricmp(p,"DEFAULT"))
			cfg.log=LOG_DEFAULTS;
		else if(!stricmp(p,"NONE"))
			cfg.log=0L;
		else
			cfg.log=strtol(cleanstr(p),0,16);
		continue; }

    if(!strcmp(tmp,"NOSWAP")) {
        node_swap=0;
        continue; }

	if(!strcmp(tmp,"SECURE_ECHOMAIL")) {
		misc|=SECURE;
		continue; }

    if(!strcmp(tmp,"CHECKMEM")) {
        misc|=CHECKMEM;
        continue; }

    if(!strcmp(tmp,"STORE_SEENBY")) {
        misc|=STORE_SEENBY;
        continue; }

    if(!strcmp(tmp,"STORE_PATH")) {
        misc|=STORE_PATH;
        continue; }

    if(!strcmp(tmp,"STORE_KLUDGE")) {
        misc|=STORE_KLUDGE;
        continue; }

    if(!strcmp(tmp,"FUZZY_ZONE")) {
        misc|=FUZZY_ZONE;
        continue; }

	if(!strcmp(tmp,"FAST_OPEN")) {
		continue; }

    if(!strcmp(tmp,"FLO_MAILER")) {
        misc|=FLO_MAILER;
        continue; }

	if(!strcmp(tmp,"ELIST_ONLY")) {
		misc|=ELIST_ONLY;
		continue; }

	if(!strcmp(tmp,"KILL_EMPTY")) {
		misc|=KILL_EMPTY_MAIL;
		continue; }

    if(!strcmp(tmp,"AREAFILE")) {
        sprintf(cfg.areafile,"%-.80s",cleanstr(p));
        continue; }

    if(!strcmp(tmp,"LOGFILE")) {
        sprintf(cfg.logfile,"%-.80s",cleanstr(p));
        continue; }

    if(!strcmp(tmp,"INBOUND")) {            /* Inbound directory */
        sprintf(cfg.inbound,"%-.80s",cleanstr(p));
        if(cfg.inbound[strlen(cfg.inbound)-1]!='\\')
            strcat(cfg.inbound,"\\");
        continue; }

    if(!strcmp(tmp,"SECURE_INBOUND")) {     /* Secure Inbound directory */
        sprintf(cfg.secure,"%-.80s",cleanstr(p));
        if(cfg.secure[strlen(cfg.secure)-1]!='\\')
            strcat(cfg.secure,"\\");
        continue; }

    if(!strcmp(tmp,"OUTBOUND")) {           /* Outbound directory */
        sprintf(cfg.outbound,"%-.80s",cleanstr(p));
        if(cfg.outbound[strlen(cfg.outbound)-1]!='\\')
            strcat(cfg.outbound,"\\");
        continue; }

    if(!strcmp(tmp,"ARCSIZE")) {            /* Maximum bundle size */
        cfg.maxbdlsize=atol(p);
        continue; }

    if(!strcmp(tmp,"PKTSIZE")) {            /* Maximum packet size */
        cfg.maxpktsize=atol(p);
        continue; }

    if(!strcmp(tmp,"USEPACKER")) {          /* Which packer to use */
		if(!*p)
			continue;
		strcpy(str,p);
		p=str;
		while(*p && *p>SP) p++;
		if(!*p)
			continue;
		*p=0;
		p++;
		for(i=0;i<cfg.arcdefs;i++)
			if(!strnicmp(cfg.arcdef[i].name,str
				,strlen(cfg.arcdef[i].name)))
				break;
		if(i==cfg.arcdefs)				/* i = number of arcdef til done */
			i=0xffff;					/* Uncompressed type if not found */
		while(*p) {
			while(*p && *p<=SP) p++;
			if(!*p)
				break;
			addr=atofaddr(p);
			while(*p && *p>SP) p++;
			j=matchnode(addr,1);
			if(j==cfg.nodecfgs) {
				cfg.nodecfgs++;
				if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
					,sizeof(nodecfg_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for nodecfg #%u.\n"
						,j+1);
					exit(1); }
				memset(&cfg.nodecfg[j],0,sizeof(nodecfg_t));
				cfg.nodecfg[j].faddr=addr; }
			cfg.nodecfg[j].arctype=i; } }

    if(!strcmp(tmp,"PKTPWD")) {         /* Packet Password */
        if(!*p)
            continue;
        addr=atofaddr(p);
        while(*p && *p>SP) p++;         /* Skip address */
        while(*p && *p<=SP) p++;        /* Find beginning of password */
		j=matchnode(addr,1);
        if(j==cfg.nodecfgs) {
            cfg.nodecfgs++;
            if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
                ,sizeof(nodecfg_t)*(j+1)))==NULL) {
				printf("\nError allocating memory for nodecfg #%u.\n"
                    ,j+1);
                exit(1); }
            memset(&cfg.nodecfg[j],0,sizeof(nodecfg_t));
            cfg.nodecfg[j].faddr=addr; }
        sprintf(cfg.nodecfg[j].pktpwd,"%.8s",p); }

	if(!strcmp(tmp,"PKTTYPE")) {            /* Packet Type to Use */
		if(!*p)
			continue;
		strcpy(str,p);
		p=str;
		while(*p && *p>SP) p++;
		*p=0;
		p++;
		while(*p) {
			while(*p && *p<=SP) p++;
			if(!*p)
				break;
			addr=atofaddr(p);
			while(*p && *p>SP) p++;
			j=matchnode(addr,1);
			if(j==cfg.nodecfgs) {
				cfg.nodecfgs++;
				if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
					,sizeof(nodecfg_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for nodecfg #%u.\n"
						,j+1);
					exit(1); }
				memset(&cfg.nodecfg[j],0,sizeof(nodecfg_t));
				cfg.nodecfg[j].faddr=addr; }
			if(!strcmp(str,"2+"))
				cfg.nodecfg[j].pkt_type=PKT_TWO_PLUS;
			else if(!strcmp(str,"2.2"))
				cfg.nodecfg[j].pkt_type=PKT_TWO_TWO;
			else if(!strcmp(str,"2"))
				cfg.nodecfg[j].pkt_type=PKT_TWO; } }

	if(!strcmp(tmp,"SEND_NOTIFY")) {    /* Nodes to send notify lists to */
		while(*p) {
            while(*p && *p<=SP) p++;
            if(!*p)
                break;
            addr=atofaddr(p);
            while(*p && *p>SP) p++;
			j=matchnode(addr,1);
            if(j==cfg.nodecfgs) {
                cfg.nodecfgs++;
                if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
                    ,sizeof(nodecfg_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for nodecfg #%u.\n"
                        ,j+1);
                    exit(1); }
                memset(&cfg.nodecfg[j],0,sizeof(nodecfg_t));
                cfg.nodecfg[j].faddr=addr; }
			cfg.nodecfg[j].attr|=SEND_NOTIFY; } }

    if(!strcmp(tmp,"PASSIVE")
        || !strcmp(tmp,"HOLD")
        || !strcmp(tmp,"CRASH")
        || !strcmp(tmp,"DIRECT")) {         /* Set node attributes */
        if(!strcmp(tmp,"PASSIVE"))
            attr=ATTR_PASSIVE;
        else if(!strcmp(tmp,"CRASH"))
            attr=ATTR_CRASH;
        else if(!strcmp(tmp,"HOLD"))
            attr=ATTR_HOLD;
        else if(!strcmp(tmp,"DIRECT"))
            attr=ATTR_DIRECT;
        while(*p) {
            while(*p && *p<=SP) p++;
            if(!*p)
                break;
            addr=atofaddr(p);
            while(*p && *p>SP) p++;
			j=matchnode(addr,1);
            if(j==cfg.nodecfgs) {
                cfg.nodecfgs++;
                if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
                    ,sizeof(nodecfg_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for nodecfg #%u.\n"
                        ,j+1);
                    exit(1); }
                memset(&cfg.nodecfg[j],0,sizeof(nodecfg_t));
                cfg.nodecfg[j].faddr=addr; }
            cfg.nodecfg[j].attr|=attr; } }

	if(!strcmp(tmp,"ROUTE_TO")) {
		while(*p && *p<=SP) p++;
		if(*p) {
			route_addr=atofaddr(p);
			while(*p && *p>SP) p++; }
        while(*p) {
            while(*p && *p<=SP) p++;
            if(!*p)
                break;
            addr=atofaddr(p);
            while(*p && *p>SP) p++;
			j=matchnode(addr,1);
            if(j==cfg.nodecfgs) {
                cfg.nodecfgs++;
                if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
                    ,sizeof(nodecfg_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for nodecfg #%u.\n"
                        ,j+1);
                    exit(1); }
                memset(&cfg.nodecfg[j],0,sizeof(nodecfg_t));
                cfg.nodecfg[j].faddr=addr; }
			cfg.nodecfg[j].route=route_addr; } }

    if(!strcmp(tmp,"AREAFIX")) {            /* Areafix stuff here */
		if(!*p)
			continue;
		addr=atofaddr(p);
		i=matchnode(addr,1);
		if(i==cfg.nodecfgs) {
			cfg.nodecfgs++;
			if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
				,sizeof(nodecfg_t)*(i+1)))==NULL) {
				printf("\nError allocating memory for nodecfg #%u.\n"
					,i+1);
				exit(1); }
			memset(&cfg.nodecfg[i],0,sizeof(nodecfg_t));
			cfg.nodecfg[i].faddr=addr; }
		cfg.nodecfg[i].flag=NULL;
		while(*p && *p>SP) p++; 		/* Get to the end of the address */
		while(*p && *p<=SP) p++;		/* Skip over whitespace chars */
		tp=p;
		while(*p && *p>SP) p++; 		/* Find end of password 	*/
		*p=0;							/* and terminate the string */
		++p;
		sprintf(cfg.nodecfg[i].password,"%-.25s",tp);
		while(*p && *p<=SP) p++;		/* Search for more chars */
		if(!*p) 						/* Nothing else there */
			continue;
		while(*p) {
			tp=p;
			while(*p && *p>SP) p++; 	/* Find end of this flag */
			*p=0;						/* and terminate it 	 */
			++p;
			for(j=0;j<cfg.nodecfg[i].numflags;j++)
				if(!strnicmp(cfg.nodecfg[i].flag[j].flag,tp
					,strlen(cfg.nodecfg[i].flag[j].flag)))
					break;
			if(j==cfg.nodecfg[i].numflags) {
				if((cfg.nodecfg[i].flag=
					(flag_t *)REALLOC(cfg.nodecfg[i].flag
					,sizeof(flag_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for nodecfg #%u "
						"flag #%u.\n",cfg.nodecfgs,j+1);
					exit(1); }
				cfg.nodecfg[i].numflags++;
				sprintf(cfg.nodecfg[i].flag[j].flag,"%.4s",tp); }
			while(*p && *p<=SP) p++; } }

    if(!strcmp(tmp,"ECHOLIST")) {           /* Echolists go here */
		if((cfg.listcfg=(echolist_t *)REALLOC(cfg.listcfg
			,sizeof(echolist_t)*(cfg.listcfgs+1)))==NULL) {
			printf("\nError allocating memory for echolist cfg #%u.\n"
				,cfg.listcfgs+1);
			exit(1); }
		memset(&cfg.listcfg[cfg.listcfgs],0,sizeof(echolist_t));
		++cfg.listcfgs;
		/* Need to forward requests? */
		if(!strnicmp(p,"FORWARD ",8) || !strnicmp(p,"HUB ",4)) {
			if(!strnicmp(p,"HUB ",4))
				cfg.listcfg[cfg.listcfgs-1].misc|=NOFWD;
            while(*p && *p>SP) p++;
            while(*p && *p<=SP) p++;
            if(*p)
				cfg.listcfg[cfg.listcfgs-1].forward=atofaddr(p);
            while(*p && *p>SP) p++;
			while(*p && *p<=SP) p++;
			if(*p && !(cfg.listcfg[cfg.listcfgs-1].misc&NOFWD)) {
				tp=p;
				while(*p && *p>SP) p++;
				*p=0;
				++p;
				while(*p && *p<=SP) p++;
				sprintf(cfg.listcfg[cfg.listcfgs-1].password,"%.71s",tp); } }
		else
			cfg.listcfg[cfg.listcfgs-1].misc|=NOFWD;
		if(!*p)
			continue;
		tp=p;
		while(*p && *p>SP) p++;
		*p=0;
		p++;

		sprintf(cfg.listcfg[cfg.listcfgs-1].listpath,"%-.128s",tp);
		cfg.listcfg[cfg.listcfgs-1].numflags=0;
		cfg.listcfg[cfg.listcfgs-1].flag=NULL;
		while(*p && *p<=SP) p++;		/* Skip over whitespace chars */
		while(*p) {
			tp=p;
			while(*p && *p>SP) p++; 	/* Find end of this flag */
			*p=0;						/* and terminate it 	 */
			++p;
			for(j=0;j<cfg.listcfg[cfg.listcfgs-1].numflags;j++)
				if(!strnicmp(cfg.listcfg[cfg.listcfgs-1].flag[j].flag,tp
					,strlen(cfg.listcfg[cfg.listcfgs-1].flag[j].flag)))
					break;
			if(j==cfg.listcfg[cfg.listcfgs-1].numflags) {
				if((cfg.listcfg[cfg.listcfgs-1].flag=
					(flag_t *)REALLOC(cfg.listcfg[cfg.listcfgs-1].flag
					,sizeof(flag_t)*(j+1)))==NULL) {
					printf("\nError allocating memory for listcfg #%u "
						"flag #%u.\n",cfg.listcfgs,j+1);
					exit(1); }
				cfg.listcfg[cfg.listcfgs-1].numflags++;
				sprintf(cfg.listcfg[cfg.listcfgs-1].flag[j].flag,"%.4s",tp); }
			while(*p && *p<=SP) p++; } }

//    printf("Unrecognized line in SBBSECHO.CFG file.\n");
}
fclose(stream);
printf("\n");
}

