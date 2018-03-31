/* pktdump.c */

/* $Id$ */

#include "fidodefs.h"
#include "xpendian.h"	/* swap */
#include "dirwrap.h"	/* _PATH_DEVNULL */
#include <stdio.h>
#include <string.h>

FILE* nulfp;
FILE* bodyfp;

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(struct fidoaddr* addr, char* outstr)
{
	static char str[64];

	if(addr==NULL)
		return("0:0/0");
	sprintf(str,"%hu:%hu/%hu",addr->zone,addr->net,addr->node);
	if(addr->point)
		sprintf(str + strlen(str), ".%hu", addr->point);
	if(addr->domain[0])
		sprintf(str + strlen(str), "@%s", addr->domain);
	if(outstr==NULL)
		return(str);
	strcpy(outstr,str);
	return(outstr);
}

char* freadstr(FILE* fp, char* str, size_t maxlen)
{
	int		ch;
	size_t	len=0;

	while((ch=fgetc(fp))!=EOF && len<maxlen) {
		str[len++]=ch;
		if(ch==0)
			break;
	}

	str[maxlen-1]=0;

	return(str);
}
	

int pktdump(FILE* fp, const char* fname)
{
	int			ch,lastch=0;
	char		buf[128];
	char		to[FIDO_NAME_LEN];
	char		from[FIDO_NAME_LEN];
	char		subj[FIDO_SUBJ_LEN];
	long		offset;
	struct fidoaddr	orig = {0};
	struct fidoaddr	dest = {0};
	fpkthdr_t	pkthdr;
	fpkdmsg_t	pkdmsg;

	if(fread(&pkthdr,sizeof(pkthdr),1,fp) != 1) {
		fprintf(stderr,"%s !Error reading pkthdr (%" XP_PRIsize_t "u bytes)\n"
			,fname,sizeof(pkthdr));
		return(-1);
	}

	fseek(fp,-2L,SEEK_END);
	fread(buf,sizeof(BYTE),sizeof(buf),fp);
	if(memcmp(buf,"\x00\x00",2)) {
		fprintf(stderr,"%s !Packet missing terminating nulls: %02X %02X\n"
			,fname,buf[0],buf[1]);
//		return(-2);
	}

	printf("%s Packet Type ", fname);

	if(pkthdr.type2.pkttype != 2) {
		fprintf(stderr, "%u (unsupported packet type)\n", pkthdr.type2.pkttype);
		return -3;
	}

	orig.zone=pkthdr.type2.origzone;
	orig.net=pkthdr.type2.orignet;
	orig.node=pkthdr.type2.orignode;
	orig.point=0;

	dest.zone=pkthdr.type2.destzone;
	dest.net=pkthdr.type2.destnet;
	dest.node=pkthdr.type2.destnode;
	dest.point=0;				/* No point info in the 2.0 hdr! */

	if(pkthdr.type2plus.cword==BYTE_SWAP_16(pkthdr.type2plus.cwcopy)  /* 2+ Packet Header */
		&& pkthdr.type2plus.cword&1) {
		fprintf(stdout,"2+ (prod: %02X%02X, rev: %u.%u)"
			,pkthdr.type2plus.prodcodeHi	,pkthdr.type2plus.prodcodeLo
			,pkthdr.type2plus.prodrevMajor	,pkthdr.type2plus.prodrevMinor);
		dest.point=pkthdr.type2plus.destpoint;
		if(pkthdr.type2plus.origpoint!=0 && orig.net==0xffff) {	/* see FSC-0048 for details */
			orig.net=pkthdr.type2plus.auxnet;
			orig.point=pkthdr.type2plus.origpoint;
		}
		if(pkthdr.type2plus.origzone != orig.zone)
			printf("!Warning: origination zone mismatch in type 2+ packet header (%u != %u)\n"
				,pkthdr.type2plus.origzone, orig.zone);
		if(pkthdr.type2plus.destzone != dest.zone)
			printf("!Warning: destination zone mismatch in type 2+ packet header (%u != %u)\n"
				,pkthdr.type2plus.destzone, dest.zone);
	} else if(pkthdr.type2_2.subversion==2) {					/* Type 2.2 Packet Header (FSC-45) */
		fprintf(stdout,"2.2 (prod: %02X, rev: %u)", pkthdr.type2_2.prodcode, pkthdr.type2_2.prodrev);
		dest.point=pkthdr.type2_2.destpoint; 
		memcpy(orig.domain, pkthdr.type2_2.origdomn, sizeof(pkthdr.type2_2.origdomn));
		memcpy(dest.domain, pkthdr.type2_2.destdomn, sizeof(pkthdr.type2_2.destdomn));
	} else
		fprintf(stdout,"2.0 (prod: %02X, serial: %u)", pkthdr.type2.prodcode, pkthdr.type2.sernum);

	printf(" from %s",faddrtoa(&orig,NULL));
	printf(" to %s\n"	,faddrtoa(&dest,NULL));

	if(pkthdr.type2.password[0])
		fprintf(stdout,"Password: '%.*s'\n",sizeof(pkthdr.type2.password),pkthdr.type2.password);

	fseek(fp,sizeof(pkthdr),SEEK_SET);

	/* Read/Display packed messages */
	while(!feof(fp)) {

		printf("%08lX\n",offset=ftell(fp));

		/* Read fixed-length header fields */
		if(fread(&pkdmsg,sizeof(BYTE),sizeof(pkdmsg),fp)!=sizeof(pkdmsg))
			break;
		/* Display fixed-length fields */
		printf("%s %06lX Packed Message Type: %d from %u/%u to %u/%u\n"
			,fname
			,offset
			,pkdmsg.type
			,pkdmsg.orignet, pkdmsg.orignode
			,pkdmsg.destnet, pkdmsg.destnode);
		printf("Attribute: %04X\n",pkdmsg.attr);
		printf("Date/Time: %s\n",pkdmsg.time);

		/* Read variable-length header fields */
		freadstr(fp,to,sizeof(to));
		freadstr(fp,from,sizeof(from));
		freadstr(fp,subj,sizeof(subj));
	
		/* Display variable-length fields */
		printf("%-4s : %s\n","To",to);
		printf("%-4s : %s\n","From",from);
		printf("%-4s : %s\n","Subj",subj);

		fprintf(bodyfp,"\n-start of message text-\n");

		while((ch=fgetc(fp))!=EOF && ch!=0) {
			if(lastch=='\r' && ch!='\n')
				fputc('\n',bodyfp);
			fputc(lastch=ch,bodyfp);
		}

		fprintf(bodyfp,"\n-end of message text-\n");
	}

	return(0);
}

char* usage = "usage: pktdump [-body] <file1.pkt> [file2.pkt] [...]\n";

int main(int argc, char** argv)
{
	FILE*	fp;
	int		i;
	char	revision[16];

	sscanf("$Revision$", "%*s %s", revision);

	fprintf(stderr,"pktdump rev %s - Dump FidoNet Packets\n\n"
		,revision
		);

	if(argc<2) {
		fprintf(stderr,"%s",usage);
		return -1;
	}

	if((nulfp=fopen(_PATH_DEVNULL,"w+"))==NULL) {
		perror(_PATH_DEVNULL);
		return -1;
	}
	bodyfp=nulfp;

	if(sizeof(fpkthdr_t)!=FIDO_PACKET_HDR_LEN) {
		printf("sizeof(fpkthdr_t)=%" XP_PRIsize_t "u, expected: %d\n",sizeof(fpkthdr_t),FIDO_PACKET_HDR_LEN);
		return(-1);
	}
	if(sizeof(fpkdmsg_t)!=FIDO_PACKED_MSG_HDR_LEN) {
		printf("sizeof(fpkdmsg_t)=%" XP_PRIsize_t "u, expected: %d\n",sizeof(fpkdmsg_t),FIDO_PACKED_MSG_HDR_LEN);
		return(-1);
	}
	if(sizeof(fmsghdr_t)!=FIDO_STORED_MSG_HDR_LEN) {
		printf("sizeof(fmsghdr_t)=%" XP_PRIsize_t "u, expected: %d\n",sizeof(fmsghdr_t),FIDO_STORED_MSG_HDR_LEN);
		return(-1);
	}

	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-') {
			switch(tolower(argv[i][1])) {
				case 'b':
					bodyfp=stdout;;
					break;
				default:
					printf("%s",usage);
					return(0);
			}
			continue;
		}
		fprintf(stdout,"Opening %s\n",argv[i]);
		if((fp=fopen(argv[i],"rb"))==NULL) {
			perror(argv[i]);
			continue;
		}
		pktdump(fp, argv[i]);
		fclose(fp);
	}

	return(0);
}
