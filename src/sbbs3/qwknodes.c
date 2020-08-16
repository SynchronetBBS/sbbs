/* $Id: qwknodes.c,v 1.24 2020/01/03 20:34:55 rswindell Exp $ */

/* Synchronet QWKnet node list or route.dat file generator */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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
#include "nopen.h"
#include "crc16.h"
#include "crc32.h"
#include "conwrap.h"	/* kbhit */

unsigned _stklen=10000;
smb_t		smb;
scfg_t		cfg;

void stripctrla(char *str)
{
	char out[256];
	int i,j;

	for(i=j=0;str[i] && j<sizeof(out)-1;i++) {
		if(str[i]==CTRL_A && str[i+1]!=0)
			i++;
		else
			out[j++]=str[i]; 
	}
	out[j]=0;
	strcpy(str,out);
}

int lputs(char* str)
{
    char tmp[256];
    int i,j,k;

	j=strlen(str);
	for(i=k=0;i<j;i++)      /* remove CRs */
		if(str[i]==CR && str[i+1]==LF)
			continue;
		else
			tmp[k++]=str[i];
	tmp[k]=0;
	return(fputs(tmp,stderr));
}
/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(const char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsnprintf(sbuf,sizeof(sbuf),fmat,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	lputs(sbuf);
	return(chcount);
}
void bail(int code)
{
	exit(code);
}

char *loadmsgtail(smbmsg_t msg)
{
	char	*buf=NULL;
	uint16_t	xlat;
	int 	i;
	long	l=0,length;

	for(i=0;i<msg.hdr.total_dfields;i++) {
		if(msg.dfield[i].type!=TEXT_TAIL)
			continue;
		fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
			,SEEK_SET);
		fread(&xlat,2,1,smb.sdt_fp);
		if(xlat!=XLAT_NONE) 		/* no translations supported */
			continue;
		length=msg.dfield[i].length-2;
		if((buf=realloc(buf,l+msg.dfield[i].length+1))==NULL)
			return(buf);
		l+=fread(buf+l,1,length,smb.sdt_fp);
		buf[l]=0; 
	}
	return(buf);
}


void gettag(smbmsg_t msg, char *tag)
{
	char *buf,*p;

	tag[0]=0;
	buf=loadmsgtail(msg);
	if(buf==NULL)
		return;
	truncsp(buf);
	stripctrla(buf);
	p=strrchr(buf,LF);
	if(!p) p=buf;
	else p++;
	if(!strnicmp(p," þ Synchronet þ ",16))
		p+=16;
	if(!strnicmp(p," * Synchronet * ",16))
		p+=16;
	while(*p && *p<=' ') p++;
	strcpy(tag,p);
	free(buf);
}


#define FEED   (1<<0)
#define LOCAL  (1<<1)
#define APPEND (1<<2)
#define TAGS   (1<<3)

#define ROUTE  (1<<1)
#define NODES  (1<<2)
#define USERS  (1<<3)

char *usage="\nusage: qwknodes [-opts] cmds"
			"\n"
			"\n cmds: r  =  create route.dat"
			"\n       u  =  create users.dat"
			"\n       n  =  create nodes.dat"
			"\n"
			"\n opts: f  =  format addresses for nodes that feed from this system"
			"\n       a  =  append existing output files"
			"\n       t  =  include tag lines in nodes.dat"
			"\n       l  =  include local users in users.dat"
			"\n       m# =  maximum message age set to # days";

int main(int argc, char **argv)
{
	char		str[256],tmp[128],tag[256],addr[256],*p;
	int 		i,j,mode=0,cmd=0,o_mode,max_age=0;
	ushort		smm,sbl;
	ulong		*crc=NULL,curcrc,total_crcs=0,l;
	FILE		*route,*users,*nodes;
	time_t		now;
	smbmsg_t	msg;
	char		revision[16];

	sscanf("$Revision: 1.24 $", "%*s %s", revision);

	fprintf(stderr,"\nSynchronet QWKnet Node/Route/User List Generator v%s-%s\n"
		,revision, PLATFORM_DESC);

	for(i=1;i<argc;i++)
		for(j=0;argv[i][j];j++)
			switch(toupper(argv[i][j])) {
				case '/':
				case '-':
					while(argv[i][++j])
						switch(toupper(argv[i][j])) {
							case 'F':
								mode|=FEED;
								break;
							case 'L':
								mode|=LOCAL;
								break;
							case 'A':
								mode|=APPEND;
								break;
							case 'T':
								mode|=TAGS;
								break;
							case 'M':
								j++;
								max_age=atoi(argv[i]+j);
								while(isdigit(argv[i][j+1])) j++;
								break;
							default:
								puts(usage);
								return(1); 
					}
					j--;
					break;
				case 'R':
					cmd|=ROUTE;
					break;
				case 'U':
					cmd|=USERS;
					break;
				case 'N':
					cmd|=NODES;
					break;
				default:
					puts(usage);
					return(1); 
		}

	if(!cmd) {
		puts(usage);
		return(1); 
	}

	if(mode&APPEND)
		o_mode=O_WRONLY|O_CREAT|O_APPEND;
	else
		o_mode=O_WRONLY|O_CREAT|O_TRUNC;

	if(cmd&NODES)
		if((nodes=fnopen(&i,"nodes.dat",o_mode))==NULL) {
			printf("\7\nError opening nodes.dat\n");
			return(1); 
		}

	if(cmd&USERS)
		if((users=fnopen(&i,"users.dat",o_mode))==NULL) {
			printf("\7\nError opening users.dat\n");
			return(1); 
		}

	if(cmd&ROUTE)
		if((route=fnopen(&i,"route.dat",o_mode))==NULL) {
			printf("\7\nError opening route.dat\n");
			return(1); 
		}

	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir, get_ctrl_dir());

	if(!load_cfg(&cfg, NULL, TRUE, str)) {
		printf("\7\n%s\n",str);
	}

	now=time(NULL);
	smm=crc16("smm",0);
	sbl=crc16("sbl",0);
	fprintf(stderr,"\n\n");
	for(i=0;i<cfg.total_subs;i++) {
		if(!(cfg.sub[i]->misc&SUB_QNET))
			continue;
		fprintf(stderr,"%-*s  %s\n"
			,LEN_GSNAME,cfg.grp[cfg.sub[i]->grp]->sname,cfg.sub[i]->lname);
		sprintf(smb.file,"%s%s",cfg.sub[i]->data_dir,cfg.sub[i]->code);
		smb.retry_time=30;
		smb.subnum=i;
		if((j=smb_open(&smb))!=0) {
			printf("smb_open returned %d\n",j);
			continue; 
		}
		if((j=smb_locksmbhdr(&smb))!=0) {
			printf("smb_locksmbhdr returned %d\n",j);
			smb_close(&smb);
			continue; 
		}
		if((j=smb_getstatus(&smb))!=0) {
			printf("smb_getstatus returned %d\n",j);
			smb_close(&smb);
			continue; 
		}
		smb_unlocksmbhdr(&smb);
		msg.offset=smb.status.total_msgs;
		if(!msg.offset) {
			smb_close(&smb);
			printf("Empty.\n");
			continue; 
		}
		while(!kbhit() && !ferror(smb.sid_fp) && msg.offset) {
			msg.offset--;
			fseek(smb.sid_fp,msg.offset*sizeof(idxrec_t),SEEK_SET);
			if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
				break;
			fprintf(stderr,"%-5u\r",msg.offset+1);
			if(msg.idx.to==smm || msg.idx.to==sbl)
				continue;
			if(max_age && now-msg.idx.time>((ulong)max_age*24UL*60UL*60UL))
				continue;
			if((j=smb_lockmsghdr(&smb,&msg))!=0) {
				printf("smb_lockmsghdr returned %d\n",j);
				break; 
			}
			if((j=smb_getmsghdr(&smb,&msg))!=0) {
				printf("smb_getmsghdr returned %d\n",j);
				break; 
			}
			smb_unlockmsghdr(&smb,&msg);
			if((mode&LOCAL && msg.from_net.type==NET_NONE)
				|| (msg.from_net.type==NET_QWK && msg.from_net.addr!=NULL)) {
				if(msg.from_net.type!=NET_QWK)
					msg.from_net.addr="";
				if(cmd&USERS) {
					sprintf(str,"%s%s",(char *)msg.from_net.addr,(char *)msg.from);
					curcrc=crc32(str,0); 
				}
				else
					curcrc=crc32(msg.from_net.addr,0);
				for(l=0;l<total_crcs;l++)
					if(curcrc==crc[l])
						break;
				if(l==total_crcs) {
					total_crcs++;
					if((crc=(ulong *)realloc(crc
						,sizeof(ulong)*total_crcs))==NULL) {
						printf("Error allocating %lu bytes\n"
							,sizeof(ulong)*total_crcs);
						break; 
					}
					crc[l]=curcrc;
					if(cmd&ROUTE && msg.from_net.type==NET_QWK) {
						strcpy(addr,msg.from_net.addr);
						if(mode&FEED) {
							p=strrchr(addr,'/');
							if(!p)
								p=addr;
							else
								*(p++)=0;
							sprintf(str,"%s %s:%s%c%s"
								,unixtodstr(&cfg,msg.hdr.when_written.time,tmp)
								,p,cfg.sys_id,p==addr ? 0 : '/'
								,addr);
							fprintf(route,"%s\r\n",str); 
						}
						else {
							p=strrchr(addr,'/');
							if(p) {
								*(p++)=0;
							fprintf(route,"%s %s:%.*s\r\n"
								,unixtodstr(&cfg,msg.hdr.when_written.time,str)
								,p
								,(uint)(p-addr)
								,addr); 
							} 
						} 
					}
					if(cmd&USERS) {
						if(msg.from_net.type!=NET_QWK)
							strcpy(str,cfg.sys_id);
						else if(mode&FEED)
							sprintf(str,"%s/%s",cfg.sys_id,(char *)msg.from_net.addr);
						else
							strcpy(str,msg.from_net.addr);
						p=strrchr(str,'/');
						if(p)
							fprintf(users,"%-25.25s  %-8.8s  %s  (%s)\r\n"
								,msg.from,p+1
								,unixtodstr(&cfg,msg.hdr.when_written.time,tmp)
								,str);
						else
							fprintf(users,"%-25.25s  %-8.8s  %s\r\n"
								,msg.from,str
								,unixtodstr(&cfg,msg.hdr.when_written.time,tmp)); 
					}
					if(cmd&NODES && msg.from_net.type==NET_QWK) {
						if(mode&TAGS)
							gettag(msg,tag);
						if(mode&FEED)
							sprintf(str,"%s/%s",cfg.sys_id,(char *)msg.from_net.addr);
						else
							strcpy(str,msg.from_net.addr);
						p=strrchr(str,'/');
						if(p) {
							if(mode&TAGS)
								fprintf(nodes,"%-8.8s  %s\r\n"
									,p+1
									,tag);
							else
								fprintf(nodes,"%-8.8s  %s  (%s)\r\n"
									,p+1
									,unixtodstr(&cfg,msg.hdr.when_written.time,tmp)
									,str); 
						}
						else
							fprintf(nodes,"%-8.8s  %s\r\n"
								,str
								,mode&TAGS
								? tag
								: unixtodstr(&cfg,msg.hdr.when_written.time,tmp)); 
					}
				} 
			}
			smb_freemsgmem(&msg); 
		}

		smb_close(&smb);
		if(kbhit()) {
			getch();
			fprintf(stderr,"Key pressed.\n");
			break; 
		} 
	}
	fprintf(stderr,"Done.\n");

	return(0);
}
