/* adduser.c */

/* Program to add a user to a Synchronet user database */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

scfg_t scfg;

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...)
{
	va_list argptr;
	char sbuf[512];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsprintf(sbuf,fmat,argptr);
	va_end(argptr);
	printf("%s",sbuf);
	return(chcount);
}


char *usage="\nusage: adduser name [-param value] [...]\n"
	"\nparams:\n"
	"\t-P\tPassword\n"
	"\t-R\tReal name\n"
	"\t-H\tChat handle\n"
	"\t-G\tGender (M or F)\n"
	"\t-B\tBirth date (in MM/DD/YY or DD/MM/YY format)\n"
	"\t-T\tTelephone number\n"
	"\t-N\tNetmail (Internet e-mail) address\n"
	"\t-A\tStreet address\n"
	"\t-L\tLocation (city, state)\n"
	"\t-Z\tZip/Postal code\n"
	"\t-S\tSecurity level\n"
	"\t-E\tExpiration days\n"
	"\t-C\tComment\n"
	"\nNOTE: multi-word user name and param values must be enclosed in \"quotes\"\n"
	;

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	char*	p;
	char	error[512];
	char	revision[16];
	int		i;
	time_t	now;
	user_t	user;

	sscanf("$Revision$" + 11, "%s", revision);

	fprintf(stderr,"\nADDUSER v%s-%s - Adds User to Synchronet User Database\n"
		,revision
		,PLATFORM_DESC
		);

	if(argc<2) {
		printf("%s",usage);
		return(1); 
	}

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\nSBBSCTRL environment variable not set.\n");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); 
	}

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);
	SAFECOPY(scfg.ctrl_dir,p);

	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(stderr,"!ERROR changing directory to: %s", scfg.ctrl_dir);

	printf("\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",error);
		exit(1);
	}

	if(!(scfg.sys_misc&SM_LOCAL_TZ))
		putenv("TZ=UTC0");

	now=time(NULL);

	memset(&user,0,sizeof(user));

	/****************/
	/* Set Defaults */
	/****************/

	/* security */
	user.level=scfg.new_level;
	user.flags1=scfg.new_flags1;
	user.flags2=scfg.new_flags2;
	user.flags3=scfg.new_flags3;
	user.flags4=scfg.new_flags4;
	user.rest=scfg.new_rest;
	user.exempt=scfg.new_exempt; 

	user.cdt=scfg.new_cdt;
	user.min=scfg.new_min;
	user.freecdt=scfg.level_freecdtperday[user.level];

	if(scfg.total_fcomps)
		strcpy(user.tmpext,scfg.fcomp[0]->ext);
	else
		strcpy(user.tmpext,"ZIP");
	for(i=0;i<scfg.total_xedits;i++)
		if(!stricmp(scfg.xedit[i]->code,scfg.new_xedit))
			break;
	if(i<scfg.total_xedits)
		user.xedit=i+1;

	user.shell=scfg.new_shell;
	user.misc=(scfg.new_misc&~(DELETED|INACTIVE|QUIET|NETMAIL));
	user.qwk=QWK_DEFAULT;
	user.firston=now;
	user.laston=now;	/* must set this or user may be purged prematurely */
	user.pwmod=now;
	user.sex=' ';
	user.prot=scfg.new_prot;

	if(scfg.new_expire)
		user.expire=now+((long)scfg.new_expire*24L*60L*60L); 

	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-') {
			switch(toupper(argv[i++][1])) {
			case 'A':
				SAFECOPY(user.address,argv[i]);
				break;
			case 'B':
				SAFECOPY(user.birth,argv[i]);
				break;
			case 'L':
				SAFECOPY(user.location,argv[i]);
				break;
			case 'C':
				SAFECOPY(user.comment,argv[i]);
				break;
			case 'E':
				user.expire=now+((long)atoi(argv[i])*24L*60L*60L); 
				break;
			case 'G':
				user.sex=toupper(argv[i][0]);
				break;
			case 'H':
				SAFECOPY(user.handle,argv[i]);
				break;
			case 'N':
				SAFECOPY(user.netmail,argv[i]);
				break;
			case 'P':
				SAFECOPY(user.pass,argv[i]);
				strupr(user.pass);
				break;
			case 'R':
				SAFECOPY(user.name,argv[i]);
				break;
			case 'S':
				user.level=atoi(argv[i]);
				break;
			case 'T':
				SAFECOPY(user.phone,argv[i]);
				break;
			case 'Z':
				SAFECOPY(user.zipcode,argv[i]);
				break;
			default:
				printf("%s",usage);
				return(1); 
			}
		}
		else
			SAFECOPY(user.alias,argv[i]);
	}

	if(user.alias[0]==0) {
		printf("%s",usage);
		return(1);
	}

	if(user.handle[0]==0)
		SAFECOPY(user.handle,user.alias);
	if(user.name[0]==0)
		SAFECOPY(user.name,user.alias);

	if((i=newuserdat(&scfg, &user))!=0) {
		fprintf(stderr,"!ERROR %d adding new user record\n",i);
		return(i);
	}

	printf("User record #%d (%s) created successfully.\n",user.number,user.alias);

	return(0);
}

