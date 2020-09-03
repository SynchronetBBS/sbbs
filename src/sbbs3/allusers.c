/* $Id: allusers.c,v 1.7 2018/02/20 11:56:26 rswindell Exp $ */

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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "sbbs.h"

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
"       ALLUSERS /R+W       add 'W' restriction for all users"
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
			break; 
	}
	return(-1);
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
			return(U_FLAGS4); 
	}
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
			return(0);				/* so skip this user */
	}

	for(i=0;i<4;i++)
		if(reqflags[i]) {
			fseek(stream,offset+getflagoff(i+1),SEEK_SET);
			if(!fread(str,8,1,stream))
				return(0);
			str[8]=0;
			truncsp(str);
			if((ahtoul(str)&reqflags[i])!=reqflags[i])
				return(0); 	/* doesn't have 'em all */

		}

	if(reqrest) {
		fseek(stream,offset+U_REST,SEEK_SET);
		if(!fread(str,8,1,stream))
			return(0);
		str[8]=0;
		truncsp(str);
		if((ahtoul(str)&reqrest)!=reqrest)
			return(0); 
	}

	if(reqexempt) {
		fseek(stream,offset+U_REST,SEEK_SET);
		if(!fread(str,8,1,stream))
			return(0);
		str[8]=0;
		truncsp(str);
		if((ahtoul(str)&reqexempt)!=reqexempt)
			return(0); 
	}

	return(1);
}

int main(int argc, char **argv)
{
	char	dir[128],str[128];
	int 	i,j,file,set,sub,mod;
	long	l,f,flags,flagoff,length,offset;
	FILE	*stream;

	printf("\nALLUSERS v2.10 - Bulk User Editor for Synchronet User Database\n");

	if(argc<2) {
		puts(usage);
		exit(1); 
	}
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
					puts(usage);
					exit(1); 
		}

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
						sub=1; 
					}
					for(;argv[i][j];j++)
						if(isalpha(argv[i][j]))
							flags|=FLAG(toupper(argv[i][j]));
					sprintf(str,"%suser.dat",dir);
					if(!fexistcase(str) || (file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
						printf("Error opening %s\n",str);
						exit(1); 
					}
					if((stream=fdopen(file,"w+b"))==NULL) {
						printf("Error opening %s\n",str);
						exit(1); 
					}
					setvbuf(stream,NULL,_IOFBF,2048);
					length=filelength(file);
					printf("\n%s Flags %s Set #%d\n",sub ? "Removing":"Adding"
						,sub ? "from":"to",set);
					for(offset=0;offset<length;offset+=U_LEN) {
						printf("%lu of %lu (%u modified)\r"
							,(offset/U_LEN)+1,length/U_LEN,mod);
						if(lockuser(stream,offset)) {
							printf("Error locking offset %lu\n",offset);
							continue; 
						}
						if(!chkuser(stream,offset)) {
							unlock(fileno(stream),offset,U_LEN);
							continue; 
						}
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
							continue; 
						}
						mod++;
						sprintf(str,"%lx",l);
						while(strlen(str)<8)
							strcat(str,"\3");
						fseek(stream,offset+flagoff,SEEK_SET);
						fwrite(str,8,1,stream);
						unlock(fileno(stream),offset,U_LEN); 
					}
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
						sub=1; 
					}
					for(;argv[i][j];j++)
						if(isalpha(argv[i][j]))
							flags|=FLAG(toupper(argv[i][j]));
					sprintf(str,"%suser.dat",dir);
					if(!fexistcase(str) || (file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
						printf("Error opening %s\n",str);
						exit(1); 
					}
					if((stream=fdopen(file,"w+b"))==NULL) {
						printf("Error opening %s\n",str);
						exit(1); 
					}
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
							continue; 
						}
						if(!chkuser(stream,offset)) {
							unlock(fileno(stream),offset,U_LEN);
							continue; 
						}
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
							continue; 
						}
						mod++;
						sprintf(str,"%lx",l);
						while(strlen(str)<8)
							strcat(str,"\3");
						fseek(stream,offset+flagoff,SEEK_SET);
						fwrite(str,8,1,stream);
						unlock(fileno(stream),offset,U_LEN); 
					}
					fclose(stream);
					printf("\n");
					break;
			   case 'L':    /* Level */
					j=atoi(argv[i]+2);
					if(j>99)
						j=99;
					if(j<0)
						j=0;
					sprintf(str,"%suser.dat",dir);
					if(!fexistcase(str) || (file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
						printf("Error opening %s\n",str);
						exit(1); 
					}
					if((stream=fdopen(file,"w+b"))==NULL) {
						printf("Error opening %s\n",str);
						exit(1); 
					}
					setvbuf(stream,NULL,_IOFBF,2048);
					length=filelength(file);
					printf("\nChanging Levels\n");
					for(offset=0;offset<length;offset+=U_LEN) {
						printf("%lu of %lu (%u modified)\r"
							,(offset/U_LEN)+1,length/U_LEN,mod);
						if(lockuser(stream,offset)) {
							printf("Error locking offset %lu\n",offset);
							continue; 
						}
						if(!chkuser(stream,offset)) {
							unlock(fileno(stream),offset,U_LEN);
							continue; 
						}
						fseek(stream,offset+U_LEVEL,SEEK_SET);
						fread(str,2,1,stream);
						str[2]=0;
						truncsp(str);
						if(atoi(str)==j) {		/* no change */
							unlock(fileno(stream),offset,U_LEN);
							continue; 
						}
						sprintf(str,"%02u",j);
						fseek(stream,offset+U_LEVEL,SEEK_SET);
						fwrite(str,2,1,stream);
						unlock(fileno(stream),offset,U_LEN);
						mod++; 
					}
					fclose(stream);
					printf("\n");
					break;
				default:
					puts(usage);
					exit(1); 
		}
		else {
			strcpy(dir,argv[i]);
			backslash(dir); 
		} 
	}
	return(0);
}
