/* MLABELS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Make mailing labels from Synchronet BBS user database					*/
/* Digital Dynamics - 03/16/93 v1.00										*/
/* Digital Dynamics - 03/03/94 v2.00										*/
/* Digital Dynamics - 04/13/95 v2.10										*/
/* Digital Dynamics - 07/17/95 v2.20										*/

/* For compilation under Borland/Turbo C(++)								*/

/* Set tabstops to 4 for viewing/printing									*/

/* This program and source code are public domain. Modified versions may	*/
/* note be distributed without consent from Digital Dynamics.				*/

#include <io.h>
#include <share.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys\stat.h>
#include "sbbsdefs.h"           /* Synchronet typedefs and macros header */

#define DOUBLE_COLUMN	(1<<0)		/* Print double column labels */
#define ATTN_ALIAS		(1<<1)		/* Print ATTN: Alias/Real Name */

char *nulstr="";
int  min=0,max=99;
long reqflags[4]={0},reqrest=0,reqexempt=0;

char *usage=
"\nusage: mlabels <data\\user path> [[-require] [...]] [/options] <outfile>\n"
"\nwhere require is one of:\n"
"       L#                  set minimum level to # (default=0)\n"
"       M#                  set maximum level to # (default=99)\n"
"       F#<flags>           set required flags from flag set #\n"
"       E<flags>            set required exemptions\n"
"       R<flags>            set required restrictions\n"
"\nwhere options is one or more of:\n"
"       D                   double column labels\n"
"       A                   include ATTN: alias/name on label\n"
"\nexample:\n"
"\nMLABELS \\SBBS\\DATA\\USER -L50 /D PRN\n";


/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                       */
/****************************************************************************/
ulong ahtoul(char *str)
{
	ulong l,val=0;

while((l=(*str++)|0x20)!=0x20)
	val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && (uchar)str[c-1]<=SP) c--;
str[c]=0;
}

/****************************************************************************/
/* Returns bytes offset into user record for flag set # 'set'               */
/****************************************************************************/
long getflagoff(int set)
{
switch(set) {
	default:
		return(U_FLAGS1);
	case 2:
		return(U_FLAGS2);
	case 3:
		return(U_FLAGS3);
	case 4:
		return(U_FLAGS4); }
}

/****************************************************************************/
/* Checks a user record against the requirements set on the command line	*/
/* Returns 1 if the user meets the requirements (or no requirements were	*/
/* specified) or 0 if the user does not meet any of the requirements.		*/
/****************************************************************************/
int chkuser(FILE *stream, long offset)
{
	char str[128];
	int i;

if(min || max!=99) {			/* Check security level */
	fseek(stream,offset+U_LEVEL,SEEK_SET);
	fread(str,2,1,stream);
	str[2]=0;
	i=atoi(str);
	if(i<min || i>max)			/* not within range */
		return(0); }			/* so skip this user */

for(i=0;i<4;i++)
	if(reqflags[i]) {
		fseek(stream,offset+getflagoff(i+1),SEEK_SET);
		fread(str,8,1,stream);
		str[8]=0;
		truncsp(str);
		if((ahtoul(str)&reqflags[i])!=reqflags[i])
			return(0); }	/* doesn't have 'em all */

if(reqrest) {
	fseek(stream,offset+U_REST,SEEK_SET);
	fread(str,8,1,stream);
	str[8]=0;
	truncsp(str);
	if((ahtoul(str)&reqrest)!=reqrest)
		return(0); }

if(reqexempt) {
	fseek(stream,offset+U_REST,SEEK_SET);
	fread(str,8,1,stream);
	str[8]=0;
	truncsp(str);
	if((ahtoul(str)&reqexempt)!=reqexempt)
        return(0); }

return(1);
}

/****************************************************************************/
/* Attempts to lock a user record, retries for up to 10 seconds 			*/
/* Returns 0 on success, -1 on failure										*/
/****************************************************************************/
int lockuser(FILE *stream, ulong offset)
{
	time_t start;

if(lock(fileno(stream),offset,U_LEN)==0)
	return(0);
start=time(NULL);
while(1) {
	if(lock(fileno(stream),offset,U_LEN)==0)
		return(0);
	if(time(NULL)-start>=10L)
		break; }
return(-1);
}

/***************/
/* Entry point *
/***************/
int main(int argc, char **argv)
{
	char	str[256],buf1[U_LEN],buf2[U_LEN],infile[128]="",outfile[128]=""
			,mode=0;	/* optional modes bits */
	int 	i,j,k,file,printed=0;
	long	l,length,offset;
	FILE	*in,*out;

printf("\nSynchronet Mailing Labels v2.10\n");
for(i=1;i<argc;i++) {
	if(argv[i][0]=='-')
		switch(toupper(argv[i][1])) {
			case 'L':                       /* Set minimum sec level */
				min=atoi(argv[i]+2);
				break;
			case 'M':                       /* Set maximum sec level */
				max=atoi(argv[i]+2);
				break;
			case 'F':                       /* Set required flags */
				j=3;
				k=1;
				if(isdigit(argv[i][2]))
					k=argv[i][2]&0xf;
				else
					j=2;
				for(;argv[i][j];j++)
                    if(isalpha(argv[i][j]))
						reqflags[k-1]|=FLAG(toupper(argv[i][j]));
				break;
			case 'R':                       /* Set required restrictions */
				for(j=2;argv[i][j];j++)
                    if(isalpha(argv[i][j]))
						reqrest|=FLAG(toupper(argv[i][j]));
				break;
			case 'E':                       /* Set required exemptions */
				for(j=2;argv[i][j];j++)
                    if(isalpha(argv[i][j]))
						reqexempt|=FLAG(toupper(argv[i][j]));
                break;
			default:						/* Unrecognized include */
				printf(usage);
                exit(1); }

	else if(argv[i][0]=='/') {
		k=strlen(argv[i]);
		for(j=1;j<k;j++)
			switch(toupper(argv[i][j])) {
				case 'D':   /* Double column labels */
					mode|=DOUBLE_COLUMN;
					break;
				case 'A':   /* Attention Alias/Real Name */
					mode|=ATTN_ALIAS;
					break;
				default:
					printf("\nUnknown option\n");
				case '?':
					printf(usage);
					exit(1); } }
	else if(infile[0])				/* in filename already given */
		strcpy(outfile,argv[i]);
	else
		strcpy(infile,argv[i]); }

if(!infile[0] || !outfile[0]) {
	printf("\nFilename not specified\n");
	printf(usage);
	exit(1); }

if(infile[strlen(infile)-1]!='\\' && infile[strlen(infile)-1]!=':')
	strcat(infile,"\\");
strcat(infile,"USER.DAT");
if((file=sopen(infile,O_RDONLY|O_BINARY,SH_DENYNO))==-1) {
	printf("\nError opening %s\n",infile);
	exit(1); }
if((in=fdopen(file,"rb"))==NULL) {
	printf("\nError opening %s\n",infile);
	exit(1); }
setvbuf(in,NULL,_IOFBF,2048);
length=filelength(file);

if((file=open(outfile,O_WRONLY|O_TRUNC|O_CREAT|O_BINARY
	,S_IWRITE|S_IREAD))==-1) {
	printf("\nError opening/creating %s\n",outfile);
	exit(1); }
if((out=fdopen(file,"wb"))==NULL) {
	printf("\nError opening %s\n",outfile);
	exit(1); }
setvbuf(out,NULL,_IOFBF,2048);

printf("\n");
for(offset=0;offset<length;offset+=U_LEN) {
	printf("%lu of %lu (%u labels)\r"
		,(offset/U_LEN)+1,length/U_LEN,printed);
	if(lockuser(in,offset)) {
		printf("Error locking offset %lu\n",offset);
		continue; }

	if(!chkuser(in,offset)) {
		unlock(fileno(in),offset,U_LEN);
		continue; }

	fseek(in,offset,SEEK_SET);
	if(!fread(buf1,U_LEN,1,in)) {
		printf("Couldn't read %lu bytes at %lu\n",U_LEN,offset);
		break; }
	unlock(fileno(in),offset,U_LEN);
	for(i=0;i<U_LEN;i++) {				/* Convert ETX (3) to NULL (0) */
		if(buf1[i]==ETX)
			buf1[i]=NULL; }

	buf1[U_MISC+8]=0;
	l=ahtoul(buf1+U_MISC);
	if(l&(DELETED|INACTIVE))			 /* skip if deleted or inactive */
		continue;

	while(mode&DOUBLE_COLUMN) { 		 /* double wide - right column */
		offset+=U_LEN;
		printf("%lu of %lu (%u labels)\r"
			,(offset/U_LEN)+1,length/U_LEN,printed);
		if(lockuser(in,offset)) {
			printf("Error locking offset %lu\n",offset);
			continue; }
		fseek(in,offset,SEEK_SET);
		if(!fread(buf2,U_LEN,1,in)) {
			mode&=~DOUBLE_COLUMN;
			unlock(fileno(in),offset,U_LEN); }
		else {
			if(!chkuser(in,offset)) {
				unlock(fileno(in),offset,U_LEN);
				continue; }
			unlock(fileno(in),offset,U_LEN);
			for(i=0;i<U_LEN;i++) {		/* Convert ETX (3) to NULL (0) */
				if(buf2[i]==ETX)
					buf2[i]=NULL; }
			buf2[U_MISC+8]=0;
			l=ahtoul(buf2+U_MISC);
			if(l&(DELETED|INACTIVE))	/* skip if deleted or inactive */
				continue;
			else
				break; } }

	if(mode&DOUBLE_COLUMN) {			/* print two columns */
		fprintf(out,"    %-*.*s%*s%.*s\r\n"
			,LEN_NAME,LEN_NAME
			,buf1+U_NAME
			,41-LEN_NAME,nulstr
			,LEN_NAME
			,buf2+U_NAME);

		fprintf(out,"    %-*.*s%*s%.*s\r\n"
			,LEN_ADDRESS,LEN_ADDRESS
			,buf1+U_ADDRESS
			,41-LEN_ADDRESS,nulstr
			,LEN_ADDRESS
			,buf2+U_ADDRESS);

		sprintf(str,"%.*s %.*s"
			,LEN_LOCATION,buf1+U_LOCATION
			,LEN_ZIPCODE,buf1+U_ZIPCODE);

		fprintf(out,"    %-41s%.*s %.*s\r\n"
			,str
			,LEN_LOCATION,buf2+U_LOCATION
			,LEN_ZIPCODE,buf2+U_ZIPCODE);

		sprintf(str,"ATTN: %.*s",LEN_ALIAS,buf1+U_ALIAS);

        if(mode&ATTN_ALIAS)
			fprintf(out,"    %-41sATTN: %.*s\r\n\r\n\r\n"
                ,str,LEN_ALIAS,buf2+U_ALIAS);
        else
			fprintf(out,"\r\n\r\n\r\n");

		printed+=2; }

	else {								/* single column labels */
		fprintf(out,"    %.*s\r\n"
			,LEN_NAME
			,buf1+U_NAME);

		fprintf(out,"    %.*s\r\n"
			,LEN_ADDRESS
			,buf1+U_ADDRESS);

		fprintf(out,"    %.*s %.*s\r\n"
			,LEN_LOCATION
			,buf1+U_LOCATION
			,LEN_ZIPCODE
			,buf1+U_ZIPCODE);

		if(mode&ATTN_ALIAS)
			fprintf(out,"    ATTN: %.*s\r\n\r\n\r\n"
				,LEN_ALIAS,buf1+U_ALIAS);
        else
			fprintf(out,"\r\n\r\n\r\n");

		printed++; } }
printf("\nDone.\n");
fclose(in);
fclose(out);
return(0);
}

/* end of mlabels.c */
