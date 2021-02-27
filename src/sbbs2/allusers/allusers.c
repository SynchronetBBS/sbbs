/* ALLUSERS.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/****************************************************************************/
/* Makes global changes to Synchronet user database (USER.DAT)				*/
/* Compatible with Version 2.1 of Synchronet BBS Software					*/
/****************************************************************************/

#include <io.h>
#include <share.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "sbbsdefs.h"

int  min=0,max=99;
long reqflags[4]={0},reqrest=0,reqexempt=0;

char *usage=
"\nusage: allusers [data\\user path] [[-require] [...]] "
	"/modify [[/modify] [...]]\n"
"\nwhere require is one of:\n"
"       L#                  set minimum level to # (default=0)\n"
"       M#                  set maximum level to # (default=99)\n"
"       F#<flags>           set required flags from flag set #\n"
"       E<flags>            set required exemptions\n"
"       R<flags>            set required restrictions\n"
"\nwhere modify is one of:\n"
"       L#                  change security level to #\n"
"       F#[+|-]<flags>      add or remove flags from flag set #\n"
"       E[+|-]<flags>       add or remove exemption flags\n"
"       R[+|-]<flags>       add or remove restriction flags\n"
"\nExamples:\n"
"       ALLUSERS -L30 /FA   add 'A' to flag set #1 for all level 30+ users\n"
"       ALLUSERS /F3-G      remove 'G' from flag set #3 for all users\n"
"       ALLUSERS -F2B /E-P  remove 'P' exemption for all users with FLAG '2B'\n"
"       ALLUSERS /R+W       add 'W' restriction for all users\n"
;

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

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong								*/
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
	if(!fread(str,2,1,stream))
		return(0);
	str[2]=0;
	i=atoi(str);
	if(i<min || i>max)			/* not within range */
		return(0); }			/* so skip this user */

for(i=0;i<4;i++)
	if(reqflags[i]) {
		fseek(stream,offset+getflagoff(i+1),SEEK_SET);
		if(!fread(str,8,1,stream))
			return(0);
		str[8]=0;
		truncsp(str);
		if((ahtoul(str)&reqflags[i])!=reqflags[i])
			return(0); }	/* doesn't have 'em all */

if(reqrest) {
	fseek(stream,offset+U_REST,SEEK_SET);
	if(!fread(str,8,1,stream))
		return(0);
	str[8]=0;
	truncsp(str);
	if((ahtoul(str)&reqrest)!=reqrest)
		return(0); }

if(reqexempt) {
	fseek(stream,offset+U_REST,SEEK_SET);
	if(!fread(str,8,1,stream))
		return(0);
	str[8]=0;
	truncsp(str);
	if((ahtoul(str)&reqexempt)!=reqexempt)
        return(0); }

return(1);
}

int main(int argc, char **argv)
{
	char	dir[128],str[128];
	int 	i,j,k,file,set,sub,mod;
	long	l,f,flags,flagoff,length,offset;
	FILE	*stream;

printf("\nALLUSERS v2.10 - Bulk User Editor for Synchronet User Database\n");

if(argc<2) {
	printf(usage);
	exit(1); }
dir[0]=0;
for(i=1;i<argc;i++) {
	flags=flagoff=sub=mod=0;
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
				set=1;
				if(isdigit(argv[i][2]))
					set=argv[i][2]&0xf;
				else
					j=2;
				for(;argv[i][j];j++)
                    if(isalpha(argv[i][j]))
						reqflags[set-1]|=FLAG(toupper(argv[i][j]));
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

	else if(argv[i][0]=='/')
		switch(toupper(argv[i][1])) {
			case 'F':   /* flags */
				j=3;
				set=1;
				if(isdigit(argv[i][2]))
					set=argv[i][2]&0xf;
				else
					j=2;
				if(argv[i][j]=='+')
					j++;
				else if(argv[i][j]=='-') {
					j++;
					sub=1; }
				for(;argv[i][j];j++)
                    if(isalpha(argv[i][j]))
                        flags|=FLAG(toupper(argv[i][j]));
				sprintf(str,"%sUSER.DAT",dir);
				if((file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
					printf("Error opening %s\n",str);
					exit(1); }
				if((stream=fdopen(file,"w+b"))==NULL) {
					printf("Error opening %s\n",str);
					exit(1); }
				setvbuf(stream,NULL,_IOFBF,2048);
				length=filelength(file);
				printf("\n%s Flags %s Set #%d\n",sub ? "Removing":"Adding"
					,sub ? "from":"to",set);
				for(offset=0;offset<length;offset+=U_LEN) {
					printf("%lu of %lu (%u modified)\r"
						,(offset/U_LEN)+1,length/U_LEN,mod);
					if(lockuser(stream,offset)) {
						printf("Error locking offset %lu\n",offset);
						continue; }
					if(!chkuser(stream,offset)) {
						unlock(fileno(stream),offset,U_LEN);
						continue; }
					flagoff=getflagoff(set);
					fseek(stream,offset+flagoff,SEEK_SET);
					fread(str,8,1,stream);
					str[8]=0;
					truncsp(str);
					l=f=ahtoul(str);
					if(sub)
						l&=~flags;
					else
						l|=flags;
					if(l==f) {	/* no change */
						unlock(fileno(stream),offset,U_LEN);
						continue; }
					mod++;
					sprintf(str,"%lx",l);
					while(strlen(str)<8)
						strcat(str,"\3");
					fseek(stream,offset+flagoff,SEEK_SET);
					fwrite(str,8,1,stream);
					unlock(fileno(stream),offset,U_LEN); }
				fclose(stream);
				printf("\n");
				break;
		   case 'E':    /* Exemptions */
				flagoff=U_EXEMPT;
		   case 'R':    /* Restrictions */
				if(!flagoff)
					flagoff=U_REST;
				j=2;
				if(argv[i][j]=='+')
					j++;
				else if(argv[i][j]=='-') {
					j++;
					sub=1; }
				for(;argv[i][j];j++)
                    if(isalpha(argv[i][j]))
                        flags|=FLAG(toupper(argv[i][j]));
				sprintf(str,"%sUSER.DAT",dir);
				if((file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
					printf("Error opening %s\n",str);
					exit(1); }
				if((stream=fdopen(file,"w+b"))==NULL) {
					printf("Error opening %s\n",str);
					exit(1); }
				setvbuf(stream,NULL,_IOFBF,2048);
				length=filelength(file);
				printf("\n%s %s\n"
					,sub ? "Removing":"Adding"
					,flagoff==U_REST ? "Restrictions":"Exemptions");
				for(offset=0;offset<length;offset+=U_LEN) {
					printf("%lu of %lu (%u modified)\r"
						,(offset/U_LEN)+1,length/U_LEN,mod);
					if(lockuser(stream,offset)) {
						printf("Error locking offset %lu\n",offset);
                        continue; }
					if(!chkuser(stream,offset)) {
						unlock(fileno(stream),offset,U_LEN);
						continue; }
					fseek(stream,offset+flagoff,SEEK_SET);
					fread(str,8,1,stream);
					str[8]=0;
					truncsp(str);
					l=f=ahtoul(str);
					if(sub)
						l&=~flags;
					else
						l|=flags;
					if(l==f) {	/* no change */
						unlock(fileno(stream),offset,U_LEN);
						continue; }
					mod++;
					sprintf(str,"%lx",l);
					while(strlen(str)<8)
						strcat(str,"\3");
					fseek(stream,offset+flagoff,SEEK_SET);
					fwrite(str,8,1,stream);
					unlock(fileno(stream),offset,U_LEN); }
				fclose(stream);
				printf("\n");
                break;
		   case 'L':    /* Level */
				j=atoi(argv[i]+2);
				if(j>99)
					j=99;
				if(j<0)
					j=0;
				sprintf(str,"%sUSER.DAT",dir);
				if((file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
					printf("Error opening %s\n",str);
					exit(1); }
				if((stream=fdopen(file,"w+b"))==NULL) {
					printf("Error opening %s\n",str);
					exit(1); }
				setvbuf(stream,NULL,_IOFBF,2048);
				length=filelength(file);
				printf("\nChanging Levels\n");
				for(offset=0;offset<length;offset+=U_LEN) {
					printf("%lu of %lu (%u modified)\r"
						,(offset/U_LEN)+1,length/U_LEN,mod);
					if(lockuser(stream,offset)) {
						printf("Error locking offset %lu\n",offset);
                        continue; }
					if(!chkuser(stream,offset)) {
						unlock(fileno(stream),offset,U_LEN);
						continue; }
					fseek(stream,offset+U_LEVEL,SEEK_SET);
					fread(str,2,1,stream);
					str[2]=0;
					truncsp(str);
					if(atoi(str)==j) {		/* no change */
						unlock(fileno(stream),offset,U_LEN);
						continue; }
					sprintf(str,"%02u",j);
					fseek(stream,offset+U_LEVEL,SEEK_SET);
					fwrite(str,2,1,stream);
					unlock(fileno(stream),offset,U_LEN);
					mod++; }
				fclose(stream);
				printf("\n");
                break;
			default:
				printf(usage);
				exit(1); }
	else {
		strcpy(dir,argv[i]);
		if(dir[strlen(dir)-1]!='\\' && dir[strlen(dir)-1]!=':')
			strcat(dir,"\\"); } }
return(0);
}
