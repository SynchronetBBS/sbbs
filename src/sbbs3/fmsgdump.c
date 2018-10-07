/* $Id$ */
// vi: tabstop=4

#include "gen_defs.h"
#include "fidodefs.h"
#include "xpendian.h"	/* swap */
#include "dirwrap.h"	/* _PATH_DEVNULL */
#include <stdio.h>
#include <string.h>

FILE* nulfp;
FILE* bodyfp;

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

int msgdump(FILE* fp, const char* fname)
{
	int			ch;
	long		end;
	fmsghdr_t	hdr;

	if(fread(&hdr,sizeof(hdr),1,fp) != 1) {
		fprintf(stderr,"%s !Error reading msg hdr (%" XP_PRIsize_t "u bytes)\n"
			,fname,sizeof(hdr));
		return(__COUNTER__);
	}

	fseek(fp,-1L,SEEK_END);
	ch = fgetc(fp);
	end = ftell(fp);
	if(ch != FIDO_STORED_MSG_TERMINATOR) {
		fprintf(stderr,"%s !Message missing terminator (%02X instead of %02X)\n"
			,fname, (uchar)ch, FIDO_STORED_MSG_TERMINATOR);
//		return(-2);
	}

	if(hdr.from[sizeof(hdr.from)-1] != 0)
		fprintf(stderr,"%s Unterminated 'from' field\n", fname);
	if(hdr.to[sizeof(hdr.to)-1] != 0)
		fprintf(stderr,"%s Unterminated 'to' field\n", fname);
	if(hdr.subj[sizeof(hdr.subj)-1] != 0)
		fprintf(stderr,"%s Unterminated 'subj' field\n", fname);
	if(hdr.time[sizeof(hdr.time)-1] != 0)
		fprintf(stderr,"%s Untermianted 'time' field\n", fname);


	printf("Subj: %.*s\n", (int)sizeof(hdr.subj)-1, hdr.subj);
	printf("Attr: %04hX\n", hdr.attr);
	printf("To  : %.*s (%u.%u/%u.%u)\n", (int)sizeof(hdr.to)-1, hdr.to
		,hdr.destzone, hdr.destnet, hdr.destnode, hdr.destpoint);
	printf("From: %.*s (%u.%u/%u.%u)\n", (int)sizeof(hdr.from)-1, hdr.from
		,hdr.origzone, hdr.orignet, hdr.orignode, hdr.origpoint);
	printf("Time: %.*s\n", (int)sizeof(hdr.time)-1, hdr.time);

	if(end <= sizeof(hdr)+1) {
		fprintf(stderr, "!No body text\n");
		return(__COUNTER__);
	}

	char* body = calloc((end - sizeof(hdr)) + 1, 1);
	fseek(fp, sizeof(hdr), SEEK_SET);
	fread(body, end - sizeof(hdr), 1, fp);
	char* tp;
	REPLACE_CHARS(body, '\r', '\n', tp);
	REPLACE_CHARS(body, '\1', '@', tp);	
	fprintf(bodyfp,"\n-start of message text-\n");
	fprintf(bodyfp,body);
	fprintf(bodyfp,"\n-end of message text-\n");

	printf("\n");
	return(0);
}

char* usage = "usage: fmsgdump [-body] <file1.msg> [file2.msg] [...]\n";

int main(int argc, char** argv)
{
	FILE*	fp;
	int		i;
	char	revision[16];

	sscanf("$Revision$", "%*s %s", revision);

	fprintf(stderr,"fmsgdump rev %s - Dump FidoNet Stored Messages\n\n"
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
		msgdump(fp, argv[i]);
		fclose(fp);
	}

	return(0);
}
