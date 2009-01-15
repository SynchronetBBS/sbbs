/* pktdump.c */

/* $Id$ */

#include "fidodefs.h"
#include "sbbsdefs.h"	/* faddr_t */

FILE* nulfp;
FILE* bodyfp;

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(faddr_t* addr, char* outstr)
{
	static char str[64];
    char point[25];

	if(addr==NULL)
		return("0:0/0");
	sprintf(str,"%hu:%hu/%hu",addr->zone,addr->net,addr->node);
	if(addr->point) {
		sprintf(point,".%hu",addr->point);
		strcat(str,point); }
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
	int			ch,lastch;
	char		buf[128];
	char		origdomn[16]="";
	char		destdomn[16]="";
	char		to[FIDO_NAME_LEN];
	char		from[FIDO_NAME_LEN];
	char		subj[FIDO_SUBJ_LEN];
	long		offset;
	faddr_t		orig;
	faddr_t		dest;
	fpkthdr_t	pkthdr;
	fpkdmsg_t	pkdmsg;

	if(fread(&pkthdr,sizeof(BYTE),sizeof(pkthdr),fp)!=sizeof(pkthdr)) {
		fprintf(stderr,"%s !Error reading pkthdr (%u bytes)\n"
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

	orig.zone=pkthdr.origzone;
	orig.net=pkthdr.orignet;
	orig.node=pkthdr.orignode;
	orig.point=0;

	dest.zone=pkthdr.destzone;
	dest.net=pkthdr.destnet;
	dest.node=pkthdr.destnode;
	dest.point=0;				/* No point info in the 2.0 hdr! */

	printf("%s Packet Type ", fname);

	if(pkthdr.fill.two_plus.cword==_rotr(pkthdr.fill.two_plus.cwcopy,8)  /* 2+ Packet Header */
		&& pkthdr.fill.two_plus.cword&1) {
		fprintf(stdout,"2+");
		dest.point=pkthdr.fill.two_plus.destpoint;
		if(orig.net==-1)	/* see FSC-0048 for details */
			orig.net=pkthdr.fill.two_plus.auxnet;
	} else if(pkthdr.baud==2) {					/* Type 2.2 Packet Header */
		fprintf(stdout,"2.2");
		dest.point=pkthdr.month; 
		sprintf(origdomn,"@%s",pkthdr.fill.two_two.origdomn);
		sprintf(destdomn,"@%s",pkthdr.fill.two_two.destdomn);
	} else
		fprintf(stdout,"2.0");

	printf(" from %s%s",faddrtoa(&orig,NULL),origdomn);
	printf(" to %s%s\n"	,faddrtoa(&dest,NULL),destdomn);

	if(pkthdr.password[0])
		fprintf(stdout,"Password: '%.*s'\n",sizeof(pkthdr.password),pkthdr.password);

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
		printf("sizeof(fpkthdr_t)=%d, expected: %d\n",sizeof(fpkthdr_t),FIDO_PACKET_HDR_LEN);
		return(-1);
	}
	if(sizeof(fpkdmsg_t)!=FIDO_PACKED_MSG_HDR_LEN) {
		printf("sizeof(fpkdmsg_t)=%d, expected: %d\n",sizeof(fpkdmsg_t),FIDO_PACKED_MSG_HDR_LEN);
		return(-1);
	}
	if(sizeof(fmsghdr_t)!=FIDO_STORED_MSG_HDR_LEN) {
		printf("sizeof(fmsghdr_t)=%d, expected: %d\n",sizeof(fmsghdr_t),FIDO_STORED_MSG_HDR_LEN);
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