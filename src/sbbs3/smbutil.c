/* smbutil.c */

/* Synchronet message base (SMB) utility */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#define SMBUTIL_VER "2.33"
char	revision[16];
char	compiler[32];

const char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};


#define NOANALYSIS		(1L<<0)
#define NOCRC			(1L<<1)

#ifdef __WATCOMC__
	#define ffblk find_t
    #define findfirst(x,y,z) _dos_findfirst(x,z,y)
	#define findnext(x) _dos_findnext(x)
#endif

#if defined(_WIN32)
	#include <ctype.h>	/* isdigit() */
	#include <conio.h>	/* getch() */
#endif

/* ANSI */
#include <stdio.h>
#include <time.h>		/* time */
#include <errno.h>		/* errno */
#include <string.h>		/* strrchr */
#include <ctype.h>		/* toupper */

#include "genwrap.h"	/* stricmp */
#include "dirwrap.h"	/* fexist */
#include "conwrap.h"	/* getch */
#include "filewrap.h"
#include "smblib.h"
#include "crc16.h"
#include "crc32.h"
#include "gen_defs.h"	/* MAX_PATH */

#ifdef __WATCOMC__
	#include <dos.h>
#endif

/* gets is dangerous */
#define gets(str)  fgets((str), sizeof(str), stdin)

#define CHSIZE_FP(fp,size)	if(chsize(fileno(fp),size)) printf("chsize failed!\n");

/********************/
/* Global variables */
/********************/

smb_t		smb;
ulong		mode=0L;
ushort		tzone=0;
ushort		xlat=XLAT_NONE;
FILE*		nulfp;
FILE*		errfp;
FILE*		statfp;
BOOL		pause_on_exit=FALSE;
BOOL		pause_on_error=FALSE;
char*		beep="";

/************************/
/* Program usage/syntax */
/************************/

char *usage=
"usage: smbutil [-opts] cmd <filespec.shd>\n"
"\n"
"cmd:\n"
"       l[n] = list msgs starting at number n\n"
"       r[n] = read msgs starting at number n\n"
"       v[n] = view msg headers starting at number n\n"
"       i[f] = import msg from text file f (or use stdin)\n"
"       e[f] = import e-mail from text file f (or use stdin)\n"
"       n[f] = import netmail from text file f (or use stdin)\n"
"       h    = dump hash file\n"
"       s    = display msg base status\n"
"       c    = change msg base status\n"
"       d    = delete all msgs\n"
"       m    = maintain msg base - delete old msgs and msgs over max\n"
"       p[k] = pack msg base (k specifies minimum packable Kbytes)\n"
"opts:\n"
"       c[m] = create message base if it doesn't exist (m=max msgs)\n"
"       a    = always pack msg base (disable compression analysis)\n"
"       i    = ignore dupes (do not store CRCs or search for duplicate hashes)\n"
"       d    = use default values (no prompt) for to, from, and subject\n"
"       l    = LZH-compress message text\n"
"       o    = print errors on stdout (instead of stderr)\n"
"       p    = wait for keypress (pause) on exit\n"
"       !    = wait for keypress (pause) on error\n"
"       b    = beep on error\n"
"       t<s> = set 'to' user name for imported message\n"
"       n<s> = set 'to' netmail address for imported message\n"
"       u<s> = set 'to' user number for imported message\n"
"       f<s> = set 'from' user name for imported message\n"
"       e<s> = set 'from' user number for imported message\n"
"       s<s> = set 'subject' for imported message\n"
"       z[n] = set time zone (n=min +/- from UT or 'EST','EDT','CST',etc)\n"
"       #    = set number of messages to view/list (e.g. -1)\n"
;

void bail(int code)
{

	if(pause_on_exit || (code && pause_on_error)) {
		fprintf(statfp,"\nHit enter to continue...");
		getchar();
	}

	if(code)
		fprintf(statfp,"\nReturning error code: %d\n",code);
	exit(code);
}

/*****************************************************************************/
/* Expands Unix LF to CRLF													 */
/*****************************************************************************/
ulong lf_expand(BYTE* inbuf, BYTE* outbuf)
{
	ulong	i,j;

	for(i=j=0;inbuf[i];i++) {
		if(inbuf[i]=='\n' && (!i || inbuf[i-1]!='\r'))
			outbuf[j++]='\r';
		outbuf[j++]=inbuf[i];
	}
	outbuf[j]=0;
	return(j);
}

/****************************************************************************/
/* Adds a new message to the message base									*/
/****************************************************************************/
void postmsg(char type, char* to, char* to_number, char* to_address, 
			 char* from, char* from_number, char* subject, FILE* fp)
{
	char		str[128];
	char		buf[1024];
	char*		msgtxt=NULL;
	char*		newtxt;
	long		msgtxtlen;
	ushort		net;
	int 		i;
	ushort		agent=AGENT_SMBUTIL;
	smbmsg_t	msg;
	long		dupechk_hashes=SMB_HASH_SOURCE_ALL;

	/* Read message text from stream (file or stdin) */
	msgtxtlen=0;
	while(!feof(fp)) {
		i=fread(buf,1,sizeof(buf),fp);
		if(i<1)
			break;
		if((msgtxt=(char*)realloc(msgtxt,msgtxtlen+i+1))==NULL) {
			fprintf(errfp,"\n%s!realloc(%ld) failure\n",beep,msgtxtlen+i+1);
			bail(1);
		}
		memcpy(msgtxt+msgtxtlen,buf,i);
		msgtxtlen+=i;
	}

	if(msgtxt!=NULL) {

		msgtxt[msgtxtlen]=0;	/* Must be NULL-terminated */

		if((newtxt=(char*)malloc((msgtxtlen*2)+1))==NULL) {
			fprintf(errfp,"\n%s!malloc(%ld) failure\n",beep,(msgtxtlen*2)+1);
			bail(1);
		}

		/* Expand LFs to CRLFs */
		msgtxtlen=lf_expand(msgtxt, newtxt);
		free(msgtxt);
		msgtxt=newtxt;
	}

	memset(&msg,0,sizeof(smbmsg_t));
	msg.hdr.when_written.time=time(NULL);
	msg.hdr.when_written.zone=tzone;
	msg.hdr.when_imported=msg.hdr.when_written;

	if((to==NULL || stricmp(to,"All")==0) && to_address!=NULL)
		to=to_address;
	if(to==NULL) {
		printf("To User Name: ");
		fgets(str,sizeof(str),stdin); 
	} else
		SAFECOPY(str,to);
	truncsp(str);

	if((i=smb_hfield_str(&msg,RECIPIENT,str))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
			,beep,RECIPIENT,i,smb.last_error);
		bail(1); 
	}
	if(type=='E' || type=='N')
		smb.status.attr|=SMB_EMAIL;
	if(smb.status.attr&SMB_EMAIL) {
		if(to_address==NULL) {
			if(to_number==NULL) {
				printf("To User Number: ");
				gets(str);
			} else
				SAFECOPY(str,to_number);
			truncsp(str);
			if((i=smb_hfield_str(&msg,RECIPIENTEXT,str))!=SMB_SUCCESS) {
				fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
					,beep,RECIPIENTEXT,i,smb.last_error);
				bail(1); 
			}
		}
	}

	if(smb.status.attr&SMB_EMAIL && (type=='N' || to_address!=NULL)) {
		if(to_address==NULL) {
			printf("To Address (e.g. user@host): ");
			gets(str);
		} else
			SAFECOPY(str,to_address);
		truncsp(str);
		if(*str) {
			net=smb_netaddr_type(str);
			if((i=smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net))!=SMB_SUCCESS) {
				fprintf(errfp,"\n%s!smb_hfield(0x%02X) returned %d: %s\n"
					,beep,RECIPIENTNETTYPE,i,smb.last_error);
				bail(1); 
			}
			if((i=smb_hfield_str(&msg,RECIPIENTNETADDR,str))!=SMB_SUCCESS) {
				fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
					,beep,RECIPIENTNETADDR,i,smb.last_error);
				bail(1); 
			} 
		} 
	}

	if(from==NULL) {
		printf("From User Name: ");
		gets(str);
	} else
		SAFECOPY(str,from);
	truncsp(str);
	if((i=smb_hfield_str(&msg,SENDER,str))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
			,beep,SENDER,i,smb.last_error);
		bail(1); 
	}
	if(smb.status.attr&SMB_EMAIL) {
		if(from_number==NULL) {
			printf("From User Number: ");
			gets(str);
		} else
			SAFECOPY(str,from_number);
		truncsp(str);
		if((i=smb_hfield_str(&msg,SENDEREXT,str))!=SMB_SUCCESS) {
			fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
				,beep,SENDEREXT,i,smb.last_error);
			bail(1); 
		}
	}
	if((i=smb_hfield(&msg, SENDERAGENT, sizeof(agent), &agent))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_hfield(0x%02X) returned %d: %s\n"
			,beep,SENDERAGENT,i,smb.last_error);
		bail(1);
	}

	if(subject==NULL) {
		printf("Subject: ");
		gets(str);
	} else
		SAFECOPY(str,subject);
	truncsp(str);
	if((i=smb_hfield_str(&msg,SUBJECT,str))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
			,beep,SUBJECT,i,smb.last_error);
		bail(1); 
	}

	safe_snprintf(str,sizeof(str),"SMBUTIL %s-%s r%s %s %s"
		,SMBUTIL_VER
		,PLATFORM_DESC
		,revision
		,__DATE__
		,compiler
		);
	if((i=smb_hfield_str(&msg,FIDOPID,str))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_hfield_str(0x%02X) returned %d: %s\n"
			,beep,FIDOPID,i,smb.last_error);
		bail(1); 
	}

	if(mode&NOCRC || smb.status.max_crcs==0)	/* no CRC checking means no body text dupe checking */
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);

	if((i=smb_addmsg(&smb,&msg,smb.status.attr&SMB_HYPERALLOC
		,dupechk_hashes,xlat,msgtxt,NULL))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_addmsg returned %d: %s\n"
			,beep,i,smb.last_error);
		bail(1); 
	}
	smb_freemsgmem(&msg);

	FREE_AND_NULL(msgtxt);
}

/****************************************************************************/
/* Shows the message base header											*/
/****************************************************************************/
void showstatus(void)
{
	int i;

	i=smb_locksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	i=smb_getstatus(&smb);
	smb_unlocksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_getstatus returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	printf("last_msg        =%lu\n"
		   "total_msgs      =%lu\n"
		   "header_offset   =%lu\n"
		   "max_crcs        =%lu\n"
		   "max_msgs        =%lu\n"
		   "max_age         =%u\n"
		   "attr            =%04Xh\n"
		   ,smb.status.last_msg
		   ,smb.status.total_msgs
		   ,smb.status.header_offset
		   ,smb.status.max_crcs
		   ,smb.status.max_msgs
		   ,smb.status.max_age
		   ,smb.status.attr
		   );
}

/****************************************************************************/
/* Configure message base header											*/
/****************************************************************************/
void config(void)
{
	char max_msgs[128],max_crcs[128],max_age[128],header_offset[128],attr[128];
	int i;

	i=smb_locksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	i=smb_getstatus(&smb);
	smb_unlocksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_getstatus returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	printf("Header offset =%-5lu  New value (CR=No Change): "
		,smb.status.header_offset);
	gets(header_offset);
	printf("Max msgs      =%-5lu  New value (CR=No Change): "
		,smb.status.max_msgs);
	gets(max_msgs);
	printf("Max crcs      =%-5lu  New value (CR=No Change): "
		,smb.status.max_crcs);
	gets(max_crcs);
	printf("Max age       =%-5u  New value (CR=No Change): "
		,smb.status.max_age);
	gets(max_age);
	printf("Attributes    =%-5u  New value (CR=No Change): "
		,smb.status.attr);
	gets(attr);
	i=smb_locksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	i=smb_getstatus(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_getstatus returned %d: %s\n"
			,beep,i,smb.last_error);
		smb_unlocksmbhdr(&smb);
		return; 
	}
	if(isdigit(max_msgs[0]))
		smb.status.max_msgs=atol(max_msgs);
	if(isdigit(max_crcs[0]))
		smb.status.max_crcs=atol(max_crcs);
	if(isdigit(max_age[0]))
		smb.status.max_age=atoi(max_age);
	if(isdigit(header_offset[0]))
		smb.status.header_offset=atol(header_offset);
	if(isdigit(attr[0]))
		smb.status.attr=atoi(attr);
	i=smb_putstatus(&smb);
	smb_unlocksmbhdr(&smb);
	if(i)
		fprintf(errfp,"\n%s!smb_putstatus returned %d: %s\n"
			,beep,i,smb.last_error);
}

/****************************************************************************/
/* Lists messages' to, from, and subject                                    */
/****************************************************************************/
void listmsgs(ulong start, ulong count)
{
	int i;
	ulong l=0;
	smbmsg_t msg;

	if(!start)
		start=1;
	if(!count)
		count=~0;
	fseek(smb.sid_fp,(start-1L)*sizeof(idxrec_t),SEEK_SET);
	while(l<count) {
		if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
			break;
		i=smb_lockmsghdr(&smb,&msg);
		if(i) {
			fprintf(errfp,"\n%s!smb_lockmsghdr returned %d: %s\n"
				,beep,i,smb.last_error);
			break; 
		}
		i=smb_getmsghdr(&smb,&msg);
		smb_unlockmsghdr(&smb,&msg);
		if(i) {
			fprintf(errfp,"\n%s!smb_getmsghdr returned %d: %s\n"
				,beep,i,smb.last_error);
			break; 
		}
		printf("%4lu %-25.25s %-25.25s %.20s\n"
			,msg.hdr.number,msg.from,msg.to,msg.subj);
		smb_freemsgmem(&msg);
		l++; 
	}
}

char *binstr(uchar *buf, ushort length)
{
	static char str[512];
	char tmp[128];
	int i;

	str[0]=0;
	for(i=0;i<length;i++)
		if(buf[i] && (buf[i]<' ' || buf[i]>=0x7f) 
			&& buf[i]!='\r' && buf[i]!='\n' && buf[i]!='\t')
			break;
	if(i==length)		/* not binary */
		return((char*)buf);
	for(i=0;i<length;i++) {
		sprintf(tmp,"%02X ",buf[i]);
		strcat(str,tmp); 
		if(i>=100) {
			strcat(str,"...");
			break;
		}
	}
	return(str);
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char *my_timestr(time_t *intime)
{
    static char str[256];
    char mer[3],hour;
    struct tm *gm;

	gm=localtime(intime);
	if(gm==NULL) {
		strcpy(str,"Invalid Time");
		return(str); 
	}
	if(gm->tm_hour>=12) {
		if(gm->tm_hour==12)
			hour=12;
		else
			hour=gm->tm_hour-12;
		strcpy(mer,"pm"); 
	}
	else {
		if(gm->tm_hour==0)
			hour=12;
		else
			hour=gm->tm_hour;
		strcpy(mer,"am"); 
	}
	sprintf(str,"%s %s %02d %4d %02d:%02d %s"
		,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,1900+gm->tm_year
		,hour,gm->tm_min,mer);
	return(str);
}

/****************************************************************************/
/* Displays message header information										*/
/****************************************************************************/
void viewmsgs(ulong start, ulong count)
{
	int i;
	ulong l=0;
	smbmsg_t msg;

	if(!start)
		start=1;
	if(!count)
		count=~0;
	fseek(smb.sid_fp,(start-1L)*sizeof(idxrec_t),SEEK_SET);
	while(l<count) {
		if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
			break;
		i=smb_lockmsghdr(&smb,&msg);
		if(i) {
			fprintf(errfp,"\n%s!smb_lockmsghdr returned %d: %s\n"
				,beep,i,smb.last_error);
			break; 
		}
		i=smb_getmsghdr(&smb,&msg);
		smb_unlockmsghdr(&smb,&msg);
		if(i) {
			fprintf(errfp,"\n%s!smb_getmsghdr returned %d: %s\n"
				,beep,i,smb.last_error);
			break; 
		}

		printf("--------------------\n");
		printf("%-20.20s %ld\n"		,"index record",ftell(smb.sid_fp)/sizeof(idxrec_t));
		smb_dump_msghdr(stdout,&msg);
		printf("\n");
		smb_freemsgmem(&msg);
		l++; 
	}
}

/****************************************************************************/
/****************************************************************************/
void dump_hashes(void)
{
	char	tmp[128];
	int		retval;
	hash_t	hash;

	if((retval=smb_open_hash(&smb))!=SMB_SUCCESS) {
		fprintf(errfp,"\n%s!smb_open_hash returned %d: %s\n"
			,beep, retval, smb.last_error);
		return;
	}

	while(!smb_feof(smb.hash_fp)) {
		if(smb_fread(&smb,&hash,sizeof(hash),smb.hash_fp)!=sizeof(hash))
			break;
		printf("\n");
		printf("%-10s: %lu\n",		"Number",	hash.number);
		printf("%-10s: %s\n",		"Source",	smb_hashsourcetype(hash.source));
		printf("%-10s: %lu\n",		"Length",	hash.length);
		printf("%-10s: %s\n",		"Time",		my_timestr((time_t*)&hash.time));
		printf("%-10s: %x\n",		"Flags",	hash.flags);
		if(hash.flags&SMB_HASH_CRC16)
			printf("%-10s: %04x\n",	"CRC-16",	hash.crc16);
		if(hash.flags&SMB_HASH_CRC32)
			printf("%-10s: %08lx\n","CRC-32",	hash.crc32);
		if(hash.flags&SMB_HASH_MD5)
			printf("%-10s: %s\n",	"MD5",		MD5_hex(tmp,hash.md5));
	}

	smb_close_hash(&smb);
}

/****************************************************************************/
/* Maintain message base - deletes messages older than max age (in days)	*/
/* or messages that exceed maximum											*/
/****************************************************************************/
void maint(void)
{
	int i;
	ulong l,m,n,f,flagged=0;
	time_t now;
	smbmsg_t msg;
	idxrec_t *idx;

	printf("Maintaining %s\r\n",smb.file);
	now=time(NULL);
	i=smb_locksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	i=smb_getstatus(&smb);
	if(i) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp,"\n%s!smb_getstatus returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	if(!smb.status.total_msgs) {
		smb_unlocksmbhdr(&smb);
		printf("Empty\n");
		return; 
	}
	printf("Loading index...\n");
	if((idx=(idxrec_t *)malloc(sizeof(idxrec_t)*smb.status.total_msgs))
		==NULL) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp,"\n%s!Error allocating %lu bytes of memory\n"
			,beep,sizeof(idxrec_t)*smb.status.total_msgs);
		return; 
	}
	fseek(smb.sid_fp,0L,SEEK_SET);
	for(l=0;l<smb.status.total_msgs;l++) {
		printf("%lu of %lu\r"
			,l+1,smb.status.total_msgs);
		if(!fread(&idx[l],1,sizeof(idxrec_t),smb.sid_fp))
			break; 
	}
	printf("\nDone.\n\n");

	printf("Scanning for pre-flagged messages...\n");
	for(m=0;m<l;m++) {
		printf("\r%2lu%%",m ? (long)(100.0/((float)l/m)) : 0);
		if(idx[m].attr&MSG_DELETE)
			flagged++; 
	}
	printf("\r100%% (%lu pre-flagged for deletion)\n",flagged);

	if(smb.status.max_age) {
		printf("Scanning for messages more than %u days old...\n"
			,smb.status.max_age);
		for(m=f=0;m<l;m++) {
			printf("\r%2lu%%",m ? (long)(100.0/((float)l/m)) : 0);
			if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
				continue;
			if((ulong)now>idx[m].time && (now-idx[m].time)/(24L*60L*60L)
				>smb.status.max_age) {
				f++;
				flagged++;
				idx[m].attr|=MSG_DELETE; 
			} 
		}  /* mark for deletion */
		printf("\r100%% (%lu flagged for deletion)\n",f); 
	}

	printf("Scanning for read messages to be killed...\n");
	for(m=f=0;m<l;m++) {
		printf("\r%2lu%%",m ? (long)(100.0/((float)l/m)) : 0);
		if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
			continue;
		if((idx[m].attr&(MSG_READ|MSG_KILLREAD))==(MSG_READ|MSG_KILLREAD)) {
			f++;
			flagged++;
			idx[m].attr|=MSG_DELETE; 
		} 
	}
	printf("\r100%% (%lu flagged for deletion)\n",f);

	if(smb.status.max_msgs && l-flagged>smb.status.max_msgs) {
		printf("Flagging excess messages for deletion...\n");
		for(m=n=0,f=flagged;l-flagged>smb.status.max_msgs && m<l;m++) {
			if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
				continue;
			printf("%lu of %lu\r",++n,(l-f)-smb.status.max_msgs);
			flagged++;
			idx[m].attr|=MSG_DELETE; 
		}			/* mark for deletion */
		printf("\nDone.\n\n"); 
	}

	if(!flagged) {				/* No messages to delete */
		free(idx);
		smb_unlocksmbhdr(&smb);
		return; 
	}

	if(!(mode&NOANALYSIS)) {

		printf("Freeing allocated header and data blocks for deleted messages...\n");
		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			i=smb_open_da(&smb);
			if(i) {
				smb_unlocksmbhdr(&smb);
				fprintf(errfp,"\n%s!smb_open_da returned %d: %s\n"
					,beep,i,smb.last_error);
				bail(1); 
			}
			i=smb_open_ha(&smb);
			if(i) {
				smb_unlocksmbhdr(&smb);
				fprintf(errfp,"\n%s!smb_open_ha returned %d: %s\n"
					,beep,i,smb.last_error);
				bail(1); 
			} 
		}

		for(m=n=0;m<l;m++) {
			if(idx[m].attr&MSG_DELETE) {
				printf("%lu of %lu\r",++n,flagged);
				msg.idx=idx[m];
				msg.hdr.number=msg.idx.number;
				if((i=smb_getmsgidx(&smb,&msg))!=0) {
					fprintf(errfp,"\n%s!smb_getmsgidx returned %d: %s\n"
						,beep,i,smb.last_error);
					continue; 
				}
				i=smb_lockmsghdr(&smb,&msg);
				if(i) {
					fprintf(errfp,"\n%s!smb_lockmsghdr returned %d: %s\n"
						,beep,i,smb.last_error);
					break; 
				}
				if((i=smb_getmsghdr(&smb,&msg))!=0) {
					smb_unlockmsghdr(&smb,&msg);
					fprintf(errfp,"\n%s!smb_getmsghdr returned %d: %s\n"
						,beep,i,smb.last_error);
					break; 
				}
				msg.hdr.attr|=MSG_DELETE;			/* mark header as deleted */
				if((i=smb_putmsg(&smb,&msg))!=0) {
					smb_freemsgmem(&msg);
					smb_unlockmsghdr(&smb,&msg);
					fprintf(errfp,"\n%s!smb_putmsg returned %d: %s\n"
						,beep,i,smb.last_error);
					break; 
				}
				smb_unlockmsghdr(&smb,&msg);
				if((i=smb_freemsg(&smb,&msg))!=0) {
					smb_freemsgmem(&msg);
					fprintf(errfp,"\n%s!smb_freemsg returned %d: %s\n"
						,beep,i,smb.last_error);
					break; 
				}
				smb_freemsgmem(&msg); 
			} 
		}
		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			smb_close_ha(&smb);
			smb_close_da(&smb); 
		}
		printf("\nDone.\n\n"); 
	}

	printf("Re-writing index...\n");
	rewind(smb.sid_fp);
	CHSIZE_FP(smb.sid_fp,0);
	for(m=n=0;m<l;m++) {
		if(idx[m].attr&MSG_DELETE)
			continue;
		printf("%lu of %lu\r",++n,l-flagged);
		fwrite(&idx[m],sizeof(idxrec_t),1,smb.sid_fp); 
	}
	printf("\nDone.\n\n");
	fflush(smb.sid_fp);

	free(idx);
	smb.status.total_msgs-=flagged;
	smb_putstatus(&smb);
	smb_unlocksmbhdr(&smb);
}


typedef struct {
	ulong old,new;
	} datoffset_t;

/****************************************************************************/
/* Removes all unused blocks from SDT and SHD files 						*/
/****************************************************************************/
void packmsgs(ulong packable)
{
	uchar str[128],buf[SDT_BLOCK_LEN],ch,fname[128],tmpfname[128];
	int i,size;
	ulong l,m,n,datoffsets=0,length,total,now;
	FILE *tmp_sdt,*tmp_shd,*tmp_sid;
	BOOL		error=FALSE;
	smbhdr_t	hdr;
	smbmsg_t	msg;
	datoffset_t *datoffset=NULL;

	now=time(NULL);
	printf("Packing %s\n",smb.file);
	i=smb_locksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	i=smb_getstatus(&smb);
	if(i) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp,"\n%s!smb_getstatus returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}

	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		i=smb_open_ha(&smb);
		if(i) {
			smb_unlocksmbhdr(&smb);
			fprintf(errfp,"\n%s!smb_open_ha returned %d: %s\n"
				,beep,i,smb.last_error);
			return; 
		}
		i=smb_open_da(&smb);
		if(i) {
			smb_unlocksmbhdr(&smb);
			smb_close_ha(&smb);
			fprintf(errfp,"\n%s!smb_open_da returned %d: %s\n"
				,beep,i,smb.last_error);
			return; 
		} 
	}

	if(!smb.status.total_msgs) {
		printf("Empty\n");
		rewind(smb.shd_fp);
		CHSIZE_FP(smb.shd_fp,smb.status.header_offset);
		rewind(smb.sdt_fp);
		CHSIZE_FP(smb.sdt_fp,0);
		rewind(smb.sid_fp);
		CHSIZE_FP(smb.sid_fp,0);
		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			rewind(smb.sha_fp);
			CHSIZE_FP(smb.sha_fp,0);
			rewind(smb.sda_fp);
			CHSIZE_FP(smb.sda_fp,0);
			smb_close_ha(&smb);
			smb_close_da(&smb); 
		}
		smb_unlocksmbhdr(&smb);
		return; 
	}


	if(!(smb.status.attr&SMB_HYPERALLOC) && !(mode&NOANALYSIS)) {
		printf("Analyzing data blocks...\n");

		length=filelength(fileno(smb.sda_fp));

		fseek(smb.sda_fp,0L,SEEK_SET);
		for(l=m=0;l<length;l+=2) {
			printf("\r%2lu%%  ",l ? (long)(100.0/((float)length/l)) : 0);
			i=0;
			if(!fread(&i,2,1,smb.sda_fp))
				break;
			if(!i)
				m++; 
		}

		printf("\rAnalyzing header blocks...\n");

		length=filelength(fileno(smb.sha_fp));

		fseek(smb.sha_fp,0L,SEEK_SET);
		for(l=n=0;l<length;l++) {
			printf("\r%2lu%%  ",l ? (long)(100.0/((float)length/l)) : 0);
			ch=0;
			if(!fread(&ch,1,1,smb.sha_fp))
				break;
			if(!ch)
				n++; 
		}

		if(!m && !n) {
			printf("\rAlready compressed.\n\n");
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return; 
		}

		if(packable && (m*SDT_BLOCK_LEN)+(n*SHD_BLOCK_LEN)<packable*1024L) {
			printf("\rLess than %luk compressible bytes.\n\n",packable);
			smb_close_ha(&smb);
			smb_close_da(&smb);
			smb_unlocksmbhdr(&smb);
			return; 
		}

		printf("\rCompressing %lu data blocks (%lu bytes)\n"
				 "        and %lu header blocks (%lu bytes)\n"
				  ,m,m*SDT_BLOCK_LEN,n,n*SHD_BLOCK_LEN); 
	}

	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		rewind(smb.sha_fp);
		CHSIZE_FP(smb.sha_fp,0);		/* Reset both allocation tables */
		rewind(smb.sda_fp);
		CHSIZE_FP(smb.sda_fp,0); 
	}

	if(smb.status.attr&SMB_HYPERALLOC && !(mode&NOANALYSIS)) {
		printf("Analyzing %s\n",smb.file);

		length=filelength(fileno(smb.shd_fp));
		m=n=0;
		for(l=smb.status.header_offset;l<length;l+=size) {
			printf("\r%2lu%%  ",(long)(100.0/((float)length/l)));
			msg.idx.offset=l;
			if((i=smb_lockmsghdr(&smb,&msg))!=0) {
				printf("\n(%06lX) smb_lockmsghdr returned %d\n",l,i);
				size=SHD_BLOCK_LEN;
				continue; 
			}
			if((i=smb_getmsghdr(&smb,&msg))!=0) {
				smb_unlockmsghdr(&smb,&msg);
				m++;
				size=SHD_BLOCK_LEN;
				continue; 
			}
			smb_unlockmsghdr(&smb,&msg);
			if(msg.hdr.attr&MSG_DELETE) {
				m+=smb_hdrblocks(msg.hdr.length);
				n+=smb_datblocks(smb_getmsgdatlen(&msg)); 
			}
			size=smb_hdrblocks(smb_getmsghdrlen(&msg))*SHD_BLOCK_LEN;
			smb_freemsgmem(&msg); 
		}


		if(!m && !n) {
			printf("\rAlready compressed.\n\n");
			smb_unlocksmbhdr(&smb);
			return; 
		}

		if(packable && (n*SDT_BLOCK_LEN)+(m*SHD_BLOCK_LEN)<packable*1024L) {
			printf("\rLess than %luk compressible bytes.\n\n",packable);
			smb_unlocksmbhdr(&smb);
			return; 
		}

		printf("\rCompressing %lu data blocks (%lu bytes)\n"
				 "        and %lu header blocks (%lu bytes)\n"
				  ,n,n*SDT_BLOCK_LEN,m,m*SHD_BLOCK_LEN); 
	}

	sprintf(fname,"%s.sd$",smb.file);
	tmp_sdt=fopen(fname,"wb");
	sprintf(fname,"%s.sh$",smb.file);
	tmp_shd=fopen(fname,"wb");
	sprintf(fname,"%s.si$",smb.file);
	tmp_sid=fopen(fname,"wb");
	if(!tmp_sdt || !tmp_shd || !tmp_sid) {
		smb_unlocksmbhdr(&smb);
		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			smb_close_ha(&smb);
			smb_close_da(&smb); 
		}
		fprintf(errfp,"\n%s!Error opening temp files\n",beep);
		return; 
	}
	setvbuf(tmp_sdt,NULL,_IOFBF,2*1024);
	setvbuf(tmp_shd,NULL,_IOFBF,2*1024);
	setvbuf(tmp_sid,NULL,_IOFBF,2*1024);
	if(!(smb.status.attr&SMB_HYPERALLOC)
		&& (datoffset=(datoffset_t *)malloc(sizeof(datoffset_t)*smb.status.total_msgs))
		==NULL) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		fclose(tmp_sdt);
		fclose(tmp_shd);
		fclose(tmp_sid);
		fprintf(errfp,"\n%s!Error allocating memory\n",beep);
		return; 
	}
	fseek(smb.shd_fp,0L,SEEK_SET);
	fread(&hdr,1,sizeof(smbhdr_t),smb.shd_fp);
	fwrite(&hdr,1,sizeof(smbhdr_t),tmp_shd);
	fwrite(&(smb.status),1,sizeof(smbstatus_t),tmp_shd);
	for(l=sizeof(smbhdr_t)+sizeof(smbstatus_t);l<smb.status.header_offset;l++) {
		fread(&ch,1,1,smb.shd_fp);			/* copy additional base header records */
		fwrite(&ch,1,1,tmp_shd); 
	}
	fseek(smb.sid_fp,0L,SEEK_SET);
	total=0;
	for(l=0;l<smb.status.total_msgs;l++) {
		printf("%lu of %lu\r",l+1,smb.status.total_msgs);
		if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
			break;
		if(msg.idx.attr&MSG_DELETE) {
			printf("\nDeleted index.\n");
			continue; 
		}
		i=smb_lockmsghdr(&smb,&msg);
		if(i) {
			fprintf(errfp,"\n%s!smb_lockmsghdr returned %d: %s\n"
				,beep,i,smb.last_error);
			continue; 
		}
		i=smb_getmsghdr(&smb,&msg);
		smb_unlockmsghdr(&smb,&msg);
		if(i) {
			fprintf(errfp,"\n%s!smb_getmsghdr returned %d: %s\n"
				,beep,i,smb.last_error);
			continue; 
		}
		if(msg.hdr.attr&MSG_DELETE) {
			printf("\nDeleted header.\n");
			smb_freemsgmem(&msg);
			continue; 
		}
		if(msg.expiration && msg.expiration<=now) {
			printf("\nExpired message.\n");
			smb_freemsgmem(&msg);
			continue; 
		}
		for(m=0;m<datoffsets;m++)
			if(msg.hdr.offset==datoffset[m].old)
				break;
		if(m<datoffsets) {				/* another index pointed to this data */
			printf("duplicate index\n");
			msg.hdr.offset=datoffset[m].new;
			smb_incmsgdat(&smb,datoffset[m].new,smb_getmsgdatlen(&msg),1); 
		} else {

			if(!(smb.status.attr&SMB_HYPERALLOC))
				datoffset[datoffsets].old=msg.hdr.offset;

			fseek(smb.sdt_fp,msg.hdr.offset,SEEK_SET);

			m=smb_getmsgdatlen(&msg);
			if(m>16L*1024L*1024L) {
				fprintf(errfp,"\n%s!Invalid data length (%lu)\n",beep,m);
				continue; 
			}

			if(!(smb.status.attr&SMB_HYPERALLOC)) {
				datoffset[datoffsets].new=msg.hdr.offset
					=smb_fallocdat(&smb,m,1);
				datoffsets++;
				fseek(tmp_sdt,msg.hdr.offset,SEEK_SET); 
			}
			else {
				fseek(tmp_sdt,0L,SEEK_END);
				msg.hdr.offset=ftell(tmp_sdt); 
			}

			/* Actually copy the data */

			n=smb_datblocks(m);
			for(m=0;m<n;m++) {
				fread(buf,1,SDT_BLOCK_LEN,smb.sdt_fp);
				if(!m && *(ushort *)buf!=XLAT_NONE && *(ushort *)buf!=XLAT_LZH) {
					printf("\nUnsupported translation type (%04X)\n"
						,*(ushort *)buf);
					break; 
				}
				fwrite(buf,1,SDT_BLOCK_LEN,tmp_sdt); 
			}
			if(m<n)
				continue; 
		}

		/* Write the new index entry */
		length=smb_getmsghdrlen(&msg);
		if(smb.status.attr&SMB_HYPERALLOC)
			msg.idx.offset=ftell(tmp_shd);
		else
			msg.idx.offset=smb_fallochdr(&smb,length)+smb.status.header_offset;
		msg.idx.number=msg.hdr.number;
		msg.idx.attr=msg.hdr.attr;
		msg.idx.time=msg.hdr.when_imported.time;
		msg.idx.subj=smb_subject_crc(msg.subj);
		if(smb.status.attr&SMB_EMAIL) {
			if(msg.to_ext)
				msg.idx.to=atoi(msg.to_ext);
			else
				msg.idx.to=0;
			if(msg.from_ext)
				msg.idx.from=atoi(msg.from_ext);
			else
				msg.idx.from=0; 
		}
		else {
			SAFECOPY(str,msg.to);
			strlwr(str);
			msg.idx.to=crc16(str,0);
			SAFECOPY(str,msg.from);
			strlwr(str);
			msg.idx.from=crc16(str,0); 
		}
		fwrite(&msg.idx,1,sizeof(idxrec_t),tmp_sid);

		/* Write the new header entry */
		fseek(tmp_shd,msg.idx.offset,SEEK_SET);
		fwrite(&msg.hdr,1,sizeof(msghdr_t),tmp_shd);
		for(n=0;n<msg.hdr.total_dfields;n++)
			fwrite(&msg.dfield[n],1,sizeof(dfield_t),tmp_shd);
		for(n=0;n<msg.total_hfields;n++) {
			fwrite(&msg.hfield[n],1,sizeof(hfield_t),tmp_shd);
			fwrite(msg.hfield_dat[n],1,msg.hfield[n].length,tmp_shd); 
		}
		while(length%SHD_BLOCK_LEN) {	/* pad with NULLs */
			fputc(0,tmp_shd);
			length++; 
		}
		total++;
		smb_freemsgmem(&msg); 
	}

	if(datoffset)
		free(datoffset);
	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		smb_close_ha(&smb);
		smb_close_da(&smb); 
	}

	/* Change *.sh$ into *.shd */
	fclose(smb.shd_fp), smb.shd_fp=NULL;
	fclose(tmp_shd);
	sprintf(fname,"%s.shd",smb.file);
	if(remove(fname)!=0) {
		error=TRUE;
		fprintf(errfp,"\n%s!Error %d removing %s\n",beep,errno,fname);
	}
	sprintf(tmpfname,"%s.sh$",smb.file);
	if(!error && rename(tmpfname,fname)!=0) {
		error=TRUE;
		fprintf(errfp,"\n%s!Error %d renaming %s to %s\n",beep,errno,tmpfname,fname);
	}


	/* Change *.sd$ into *.sdt */
	fclose(smb.sdt_fp), smb.sdt_fp=NULL;
	fclose(tmp_sdt);
	sprintf(fname,"%s.sdt",smb.file);
	if(!error && remove(fname)!=0) {
		error=TRUE;
		fprintf(errfp,"\n%s!Error %d removing %s\n",beep,errno,fname);
	}

	sprintf(tmpfname,"%s.sd$",smb.file);
	if(!error && rename(tmpfname,fname)!=0) {
		error=TRUE;
		fprintf(errfp,"\n%s!Error %d renaming %s to %s\n",beep,errno,tmpfname,fname);
	}

	/* Change *.si$ into *.sid */
	fclose(smb.sid_fp), smb.sid_fp=NULL;
	fclose(tmp_sid);
	sprintf(fname,"%s.sid",smb.file);
	if(!error && remove(fname)!=0) {
		error=TRUE;
		fprintf(errfp,"\n%s!Error %d removing %s\n",beep,errno,fname);
	}

	sprintf(tmpfname,"%s.si$",smb.file);
	if(!error && rename(tmpfname,fname)!=0) {
		error=TRUE;
		fprintf(errfp,"\n%s!Error %d renaming %s to %s\n",beep,errno,tmpfname,fname);
	}

	if((i=smb_unlock(&smb))!=0)
		fprintf(errfp,"\n%s!ERROR %d (%s) unlocking %s\n"
			,beep,i,smb.last_error,smb.file);

	if((i=smb_open(&smb))!=0) {
		fprintf(errfp,"\n%s!Error %d (%s) reopening %s\n"
			,beep,i,smb.last_error,smb.file);
		return; 
	}
	if((i=smb_locksmbhdr(&smb))!=0)
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
	smb.status.total_msgs=total;
	if((i=smb_putstatus(&smb))!=0)
		fprintf(errfp,"\n%s!smb_putstatus returned %d: %s\n"
			,beep,i,smb.last_error);
	smb_unlocksmbhdr(&smb);
	printf("\nDone.\n\n");
}

void delmsgs(void)
{
	int i;

	printf("Deleting %s\n",smb.file);

	i=smb_locksmbhdr(&smb);
	if(i) {
		fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	i=smb_getstatus(&smb);
	if(i) {
		smb_unlocksmbhdr(&smb);
		fprintf(errfp,"\n%s!smb_getstatus returned %d: %s\n"
			,beep,i,smb.last_error);
		return; 
	}
	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		i=smb_open_da(&smb);
		if(i) {
			smb_unlocksmbhdr(&smb);
			fprintf(errfp,"\n%s!smb_open_da returned %d: %s\n"
				,beep,i,smb.last_error);
			bail(1); 
		}
		i=smb_open_ha(&smb);
		if(i) {
			smb_unlocksmbhdr(&smb);
			fprintf(errfp,"\n%s!smb_open_ha returned %d: %s\n"
				,beep,i,smb.last_error);
			bail(1); 
		} 
		/* Reset both allocation tables */
		CHSIZE_FP(smb.sha_fp,0);
		CHSIZE_FP(smb.sda_fp,0); 
		smb_close_ha(&smb);
		smb_close_da(&smb); 
	}

	CHSIZE_FP(smb.sid_fp,0);
	CHSIZE_FP(smb.shd_fp,smb.status.header_offset);
	CHSIZE_FP(smb.sdt_fp,0);

	/* re-write status header */
	smb.status.total_msgs=0;
	if((i=smb_putstatus(&smb))!=0)
		fprintf(errfp,"\n%s!smb_putstatus returned %d: %s\n"
			,beep,i,smb.last_error);
	smb_unlocksmbhdr(&smb);
	printf("\nDone.\n\n");
}

/****************************************************************************/
/* Read messages in message base											*/
/****************************************************************************/
void readmsgs(ulong start)
{
	char	*inbuf;
	int 	i,done=0,domsg=1;
	smbmsg_t msg;

	if(start)
		msg.offset=start-1;
	else
		msg.offset=0;
	while(!done) {
		if(domsg) {
			fseek(smb.sid_fp,msg.offset*sizeof(idxrec_t),SEEK_SET);
			if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
				break;
			i=smb_lockmsghdr(&smb,&msg);
			if(i) {
				fprintf(errfp,"\n%s!smb_lockmsghdr returned %d: %s\n"
					,beep,i,smb.last_error);
				break; 
			}
			i=smb_getmsghdr(&smb,&msg);
			if(i) {
				fprintf(errfp,"\n%s!smb_getmsghdr returned %d: %s\n"
					,beep,i,smb.last_error);
				break; 
			}

			printf("\n%lu (%lu)\n",msg.hdr.number,msg.offset+1);
			printf("Subj : %s\n",msg.subj);
			printf("To   : %s",msg.to);
			if(msg.to_net.type)
				printf(" (%s)",smb_netaddr(&msg.to_net));
			printf("\nFrom : %s",msg.from);
			if(msg.from_net.type)
				printf(" (%s)",smb_netaddr(&msg.from_net));
			printf("\nDate : %.24s %s"
				,my_timestr((time_t*)&msg.hdr.when_written.time)
				,smb_zonestr(msg.hdr.when_written.zone,NULL));

			printf("\n\n");

			if((inbuf=smb_getmsgtxt(&smb,&msg,GETMSGTXT_ALL))!=NULL) {
				printf("%s",inbuf);
				free(inbuf); 
			}

			i=smb_unlockmsghdr(&smb,&msg);
			if(i) {
				fprintf(errfp,"\n%s!smb_unlockmsghdr returned %d: %s\n"
					,beep,i,smb.last_error);
				break; 
			}
			smb_freemsgmem(&msg); 
		}
		domsg=1;
		printf("\nReading %s (?=Menu): ",smb.file);
		switch(toupper(getch())) {
			case '?':
				printf("\n"
					   "\n"
					   "(R)e-read current message\n"
					   "(L)ist messages\n"
					   "(T)en more titles\n"
					   "(V)iew message headers\n"
					   "(Q)uit\n"
					   "(+/-) Forward/Backward\n"
					   "\n");
				domsg=0;
				break;
			case 'Q':
				printf("Quit\n");
				done=1;
				break;
			case 'R':
				printf("Re-read\n");
				break;
			case '-':
				printf("Backwards\n");
				if(msg.offset)
					msg.offset--;
				break;
			case 'T':
				printf("Ten titles\n");
				listmsgs(msg.offset+2,10);
				msg.offset+=10;
				domsg=0;
				break;
			case 'L':
				printf("List messages\n");
				listmsgs(1,-1);
				domsg=0;
				break;
			case 'V':
				printf("View message headers\n");
				viewmsgs(1,-1);
				domsg=0;
				break;
			case CR:
			case '+':
				printf("Next\n");
				msg.offset++;
				break; 
		} 
	}
}

short str2tzone(const char* str)
{
	char tmp[32];
	short zone;

	if(isdigit(*str) || *str=='-' || *str=='+') { /* [+|-]HHMM format */
		if(*str=='+') str++;
		sprintf(tmp,"%.*s",*str=='-'? 3:2,str);
		zone=atoi(tmp)*60;
		str+=(*str=='-') ? 3:2;
		if(zone<0)
			zone-=atoi(str);
		else
			zone+=atoi(str);
		return zone;
	}
	if(!stricmp(str,"EST") || !stricmp(str,"Eastern Standard Time"))
		return (short)EST;
	if(!stricmp(str,"EDT") || !stricmp(str,"Eastern Daylight Time"))
		return (short)EDT;
	if(!stricmp(str,"CST") || !stricmp(str,"Central Standard Time"))
		return (short)CST;
	if(!stricmp(str,"CDT") || !stricmp(str,"Central Daylight Time"))
		return (short)CDT;
	if(!stricmp(str,"MST") || !stricmp(str,"Mountain Standard Time"))
		return (short)MST;
	if(!stricmp(str,"MDT") || !stricmp(str,"Mountain Daylight Time"))
		return (short)MDT;
	if(!stricmp(str,"PST") || !stricmp(str,"Pacific Standard Time"))
		return (short)PST;
	if(!stricmp(str,"PDT") || !stricmp(str,"Pacific Daylight Time"))
		return (short)PDT;

	return 0;	/* UTC */
}

/***************/
/* Entry point */
/***************/
int main(int argc, char **argv)
{
	char	cmd[128]="",*p;
	char	path[MAX_PATH+1];
	char*	to=NULL;
	char*	to_number=NULL;
	char*	to_address=NULL;
	char*	from=NULL;
	char*	from_number=NULL;
	char*	subj=NULL;
	FILE*	fp;
	int		i,j,x,y;
	long	count=0;
	BOOL	create=FALSE;
	time_t	now;
	struct	tm* tm;

	setvbuf(stdout,0,_IONBF,0);

	errfp=stderr;
	if((nulfp=fopen(_PATH_DEVNULL,"w+"))==NULL) {
		perror(_PATH_DEVNULL);
		bail(-1);
	}
	if(isatty(fileno(stderr)))
		statfp=stderr;
	else	/* if redirected, don't send status messages to stderr */
		statfp=nulfp;

	sscanf("$Revision$", "%*s %s", revision);

	DESCRIBE_COMPILER(compiler);

	smb.file[0]=0;
	fprintf(statfp,"\nSMBUTIL v%s-%s (rev %s) SMBLIB %s - Synchronet Message Base "\
		"Utility\n\n"
		,SMBUTIL_VER
		,PLATFORM_DESC
		,revision
		,smb_lib_ver()
		);

	/* Automatically detect the system time zone (if possible) */
	tzset();
	now=time(NULL);
	if((tm=localtime(&now))!=NULL) {
		if(tm->tm_isdst<=0)
			tzone=str2tzone(tzname[0]);
		else
			tzone=str2tzone(tzname[1]);
	}
	
	for(x=1;x<argc && x>0;x++) {
		if(
#ifndef __unix__
			argv[x][0]=='/' ||		/* for backwards compatibilty */
#endif
			argv[x][0]=='-') {
			if(isdigit(argv[x][1])) {
				count=strtol(argv[x]+1,NULL,10);
				continue;
			}
			for(j=1;argv[x][j];j++)
				switch(toupper(argv[x][j])) {
					case 'A':
						mode|=NOANALYSIS;
						break;
					case 'I':
						mode|=NOCRC;
						break;
					case 'D':
						to="All";
						to_number="1";
						from="Sysop";
						from_number="1";
						subj="Announcement";
						break;
					case 'Z':
						tzone=str2tzone(argv[x]+j+1);
						j=strlen(argv[x])-1;
						break;
					case 'C':
						create=TRUE;
						break;
					case 'T':
						to=argv[x]+j+1;
						j=strlen(argv[x])-1;
						break;
					case 'U':
						to_number=argv[x]+j+1;
						j=strlen(argv[x])-1;
						break;
					case 'N':
						to_address=argv[x]+j+1;
						j=strlen(argv[x])-1;
						break;
					case 'F':
						from=argv[x]+j+1;
						j=strlen(argv[x])-1;
						break;
					case 'E':
						from_number=argv[x]+j+1;
						j=strlen(argv[x])-1;
						break;
					case 'S':
						subj=argv[x]+j+1;
						j=strlen(argv[x])-1;
						break;
					case 'O':
						errfp=stdout;
						break;
					case 'L':
						xlat=XLAT_LZH;
						break;
					case 'P':
						pause_on_exit=TRUE;
						break;
					case '!':
						pause_on_error=TRUE;
						break;
					case 'B':
						beep="\a";
						break;
					default:
						printf("\nUnknown opt '%c'\n",argv[x][j]);
					case '?':
						printf("%s",usage);
						bail(1);
						break; 
			} 
		}
		else {
			if(!cmd[0])
				SAFECOPY(cmd,argv[x]);
			else {
				SAFECOPY(smb.file,argv[x]);
				if((p=getfext(smb.file))!=NULL && stricmp(p,".shd")==0)
					*p=0;	/* Chop off .shd extension, if supplied on command-line */

				sprintf(path,"%s.shd",smb.file);
				if(!fexistcase(path) && !create) {
					fprintf(errfp,"\n%s doesn't exist (use -c to create)\n",path);
					bail(1);
				}
				smb.retry_time=30;
				fprintf(statfp,"Opening %s\r\n",smb.file);
				if((i=smb_open(&smb))!=0) {
					fprintf(errfp,"\n%s!Error %d (%s) opening %s message base\n"
						,beep,i,smb.last_error,smb.file);
					bail(1); 
				}
				if(!filelength(fileno(smb.shd_fp))) {
					if(!create) {
						printf("Empty\n");
						smb_close(&smb);
						continue; 
					}
					smb.status.max_msgs=strtoul(cmd+1,NULL,0);
					smb.status.max_crcs=count;
					if((i=smb_create(&smb))!=0) {
						smb_close(&smb);
						printf("!Error %d (%s) creating %s\n",i,smb.last_error,smb.file);
						continue; 
					} 
				}
				for(y=0;cmd[y];y++)
					switch(toupper(cmd[y])) {
						case 'I':
						case 'E':
						case 'N':
							if(cmd[1]!=0) {
								if((fp=fopen(cmd+1,"r"))==NULL) {
									fprintf(errfp,"\n%s!Error %d opening %s\n"
										,beep,errno,cmd+1);
									bail(1);
								}
							} else
								fp=stdin;
							i=smb_locksmbhdr(&smb);
							if(i) {
								fprintf(errfp,"\n%s!smb_locksmbhdr returned %d: %s\n"
									,beep,i,smb.last_error);
								return(1); 
							}
							postmsg((char)toupper(cmd[y]),to,to_number,to_address,from,from_number,subj,fp);
							fclose(fp);
							y=strlen(cmd)-1;
							break;
						case 'S':
							showstatus();
							break;
						case 'C':
							config();
							break;
						case 'L':
							listmsgs(atol(cmd+1),count);
							y=strlen(cmd)-1;
							break;
						case 'P':
						case 'D':
							if((i=smb_lock(&smb))!=0) {
								fprintf(errfp,"\n%s!smb_lock returned %d: %s\n"
									,beep,i,smb.last_error);
								return(i);
							}
							switch(toupper(cmd[y])) {
								case 'P':
									packmsgs(atol(cmd+y+1));
									break;
								case 'D':
									delmsgs();
									break;
							}
							smb_unlock(&smb);
							y=strlen(cmd)-1;
							break;
						case 'R':
							readmsgs(atol(cmd+1));
							y=strlen(cmd)-1;
							break;
						case 'V':
							viewmsgs(atol(cmd+1),count);
							y=strlen(cmd)-1;
							break;
						case 'H':
							dump_hashes();
							break;
						case 'M':
							maint();
							break;
						default:
							printf("%s",usage);
							break; 
				}
				smb_close(&smb); 
			} 
		} 
	}
	if(!cmd[0])
		printf("%s",usage);

	bail(0);

	return(-1);	/* never gets here */
}
