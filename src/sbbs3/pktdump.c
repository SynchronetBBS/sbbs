/* pktdump.c */

/* $Id: pktdump.c,v 1.17 2020/04/28 05:41:30 rswindell Exp $ */

#include "fidodefs.h"
#include "xpendian.h"	/* swap */
#include "dirwrap.h"	/* _PATH_DEVNULL */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

FILE* nulfp;
FILE* bodyfp;
FILE* ctrlfp;

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

bool freadstr(FILE* fp, char* str, size_t maxlen)
{
	int		ch;
	size_t	len=0;

	memset(str, 0, maxlen);
	while((ch=fgetc(fp))!=EOF && len<maxlen) {
		str[len++]=ch;
		if(ch==0)
			break;
	}

	return str[maxlen-1] == 0;
}

const char* fmsgattr_str(uint16_t attr)
{
	static char str[64] = "";

#define FIDO_ATTR_CHECK(a, f) if(a&FIDO_##f)	sprintf(str + strlen(str), "%s%s", str[0] == 0 ? "" : ", ", #f);
	FIDO_ATTR_CHECK(attr, PRIVATE);
	FIDO_ATTR_CHECK(attr, CRASH);
	FIDO_ATTR_CHECK(attr, RECV);
	FIDO_ATTR_CHECK(attr, SENT);
	FIDO_ATTR_CHECK(attr, FILE);
	FIDO_ATTR_CHECK(attr, INTRANS);
	FIDO_ATTR_CHECK(attr, ORPHAN);
	FIDO_ATTR_CHECK(attr, KILLSENT);
	FIDO_ATTR_CHECK(attr, LOCAL);
	FIDO_ATTR_CHECK(attr, HOLD);
	FIDO_ATTR_CHECK(attr, FREQ);
	FIDO_ATTR_CHECK(attr, RRREQ);
	FIDO_ATTR_CHECK(attr, RR);
	FIDO_ATTR_CHECK(attr, AUDIT);
	FIDO_ATTR_CHECK(attr, FUPREQ);
	return str;
}

int pktdump(FILE* fp, const char* fname, FILE* good, FILE* bad)
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

	printf(" from %s", faddrtoa(&orig,NULL));
	printf(" to %s\n", faddrtoa(&dest,NULL));

	if(pkthdr.type2.password[0])
		fprintf(stdout,"Password: '%.*s'\n",(int)sizeof(pkthdr.type2.password),pkthdr.type2.password);

	if(good != NULL)
		fwrite(&pkthdr, sizeof(pkthdr), 1, good);
	if(bad != NULL)
		fwrite(&pkthdr, sizeof(pkthdr), 1, bad);
	fseek(fp,sizeof(pkthdr),SEEK_SET);

	/* Read/Display packed messages */
	while(!feof(fp)) {

		offset=ftell(fp);

		if(fread(&pkdmsg.type, 1, sizeof(pkdmsg.type), fp) != sizeof(pkdmsg.type))
			break;
		if(pkdmsg.type == FIDO_PACKET_TERMINATOR)
			continue;
		if(pkdmsg.type != 2) {
			printf("%s %06lX Corrupted Message Header (type: %04hX)\n"
				,fname
				,offset
				,pkdmsg.type);
			continue;
		}

		bool corrupted = false;
		fseek(fp, offset, SEEK_SET);
		/* Read fixed-length header fields (or final NULL byte) */
		if(fread(&pkdmsg,sizeof(BYTE),sizeof(pkdmsg),fp)!=sizeof(pkdmsg))
			break;
		if(pkdmsg.time[0] == '\0' || pkdmsg.time[sizeof(pkdmsg.time) - 1] != '\0') {
			printf("%s %06lX Corrupted Message Header (DateTime)\n"
				,fname
				,offset);
			corrupted = true;
		}

		/* Read variable-length header fields */
		freadstr(fp,to,sizeof(to));
		freadstr(fp,from,sizeof(from));
		freadstr(fp,subj,sizeof(subj));
		if(!corrupted && (from[0] == '\0'
			|| to[sizeof(to) - 1] != '\0'
			|| from[sizeof(from) - 1] != '\0'
			|| subj[sizeof(subj) - 1] != '\0'
			)) {
			printf("%s %06lX Corrupted Message Header (variable-length fields)\n"
				,fname
				,offset);
			corrupted = true;
		}

		if(corrupted) { // Seek to the end of the message body (hopefully)
			if(bad != NULL) {
				fwrite(&pkdmsg, sizeof(pkdmsg), 1, bad);
				for(int i = 0, ch = EOF; i < sizeof(to) && ch != '\0'; i++)
					fputc(ch = to[i], bad);
				for(int i = 0, ch = EOF; i < sizeof(from) && ch != '\0'; i++)
					fputc(ch = from[i], bad);
				for(int i = 0, ch = EOF; i < sizeof(subj) && ch != '\0'; i++)
					fputc(ch = subj[i], bad);
			}
			while((ch = fgetc(fp)) != EOF && ch != 0) {
				if(bad != NULL)
					fputc(ch, bad);
			}
			if(bad != NULL)
				fputc('\0', bad);
			continue;
		}

		/* Display fixed-length fields */
		printf("%s %06lX Packed Message Type: %d from %u/%u to %u/%u\n"
			,fname
			,offset
			,pkdmsg.type
			,pkdmsg.orignet, pkdmsg.orignode
			,pkdmsg.destnet, pkdmsg.destnode);
		printf("Attribute: 0x%04X (%s)\n",pkdmsg.attr, fmsgattr_str(pkdmsg.attr));
		printf("Date/Time: %s\n",pkdmsg.time);
	
		/* Display variable-length fields */
		printf("%-4s : %s\n","To",to);
		printf("%-4s : %s\n","From",from);
		printf("%-4s : %s\n","Subj",subj);

		if(good != NULL) {
			fwrite(&pkdmsg, sizeof(pkdmsg), 1, good);
			fwrite(to, strlen(to) + 1, 1, good);
			fwrite(from, strlen(from) + 1, 1, good);
			fwrite(subj, strlen(subj) + 1, 1, good);
		}

		fprintf(bodyfp,"\n-start of message text-\n");

		size_t count = 0;
		while((ch=fgetc(fp))!=EOF && ch!=0) {
			count++;
			if((count == 1 || lastch == '\r') && ch == 1) {
				fputc('@', ctrlfp);
				while((ch = fgetc(fp)) != EOF && ch != 0 && ch != '\r')
					fputc(ch, ctrlfp);
				fputc('\n', ctrlfp);
				if(ch == 0)
					break;
				continue;
			}
			if(lastch=='\r' && ch!='\n')
				fputc('\n',bodyfp);
			fputc(lastch=ch,bodyfp);
			if(good != NULL)
				fputc(ch, good);
		}
		if(good != NULL)
			fputc('\0', good);

		fprintf(bodyfp,"\n-end of message text-\n");
	}
	if(good != NULL) { // Final terminating NULL bytes
		fputc('\0', good);
		fputc('\0', good);
	}
	if(bad != NULL) { // Final terminating NULL bytes
		fputc('\0', bad);
		fputc('\0', bad);
	}

	return(0);
}

char* usage = "usage: pktdump [-body | -ctrl] [-recover | -split] <file1.pkt> [file2.pkt] [...]\n";

int main(int argc, char** argv)
{
	FILE*	fp;
	bool	split = false;
	bool	recover = false;
	int		i;
	char	revision[16];

	sscanf("$Revision: 1.17 $", "%*s %s", revision);

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
	ctrlfp=nulfp;

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
					bodyfp=stdout;
					/* fall-through */
				case 'c':
					ctrlfp = stdout;
					break;
				case 'r':
					recover=true;
					break;
				case 's':
					split=true;
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
		FILE* good = NULL;
		FILE* bad = NULL;
		char good_fname[MAX_PATH + 1] = "";
		char bad_fname[MAX_PATH + 1] = "";
		if(recover || split) {
			SAFEPRINTF(good_fname, "%s.good", argv[i]);
			if((good = fopen(good_fname, "wb")) == NULL) {
				perror(argv[i]);
				return EXIT_FAILURE;
			}
		}
		if(split) {
			SAFEPRINTF(bad_fname, "%s.bad", argv[i]);
			if((bad = fopen(bad_fname, "wb")) == NULL) {
				perror(argv[i]);
				return EXIT_FAILURE;
			}
		}
		pktdump(fp, argv[i], good, bad);
		if(good != NULL) {
			long length = ftell(good);
			fclose(good);
			good = NULL;
			if(length <= sizeof(fpkthdr_t) + sizeof(uint16_t) // no messages
				|| length == ftell(fp))
				remove(good_fname);
		}
		if(bad != NULL) {
			long length = ftell(bad);
			fclose(bad);
			bad = NULL;
			if(length <= sizeof(fpkthdr_t) + sizeof(uint16_t) // no messages
				|| length == ftell(fp))
				remove(bad_fname);
		}
		fclose(fp);
	}

	return(0);
}
