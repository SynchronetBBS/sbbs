/* Upgrade Synchronet files from v3 to v4 */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "sbbs4defs.h"

scfg_t scfg;
BOOL overwrite_existing_files=TRUE;

BOOL overwrite(const char* path)
{
	char	str[128];

	if(!overwrite_existing_files && fexist(path)) {
		printf("\n%s already exists, overwrite? ",path);
		fgets(str,sizeof(str),stdin);
		if(toupper(*str)!='Y')
			return(FALSE);
	}

	return(TRUE);
}


BOOL upgrade_users(void)
{
	char	outpath[MAX_PATH+1];
	char	rec[USER_REC_LEN+1];
	FILE*	out;
	int		i,total;
	int		ret;
	size_t	len;
	user_t	user;

	printf("Upgrading user database...     ");

	sprintf(outpath,"%suser/user.tab",scfg.data_dir);
	if(!overwrite(outpath))
		return(TRUE);
	if((out=fopen(outpath,"wb"))==NULL) {
		perror(outpath);
		return(FALSE);
	}

	total=lastuser(&scfg);
	for(i=1;i<=total;i++) {
		printf("\b\b\b\b\b%5u",i);
		memset(&user,0,sizeof(user));
		user.number=i;
		if((ret=getuserdat(&scfg,&user))!=0) {
			printf("\nError %d reading user.dat\n",ret);
			continue;
		}
		/******************************************/
		/* personal info */
		len=sprintf(rec,"%s\t%s\t%s\t%s\t%s\t%s\t"
			,user.alias
			,user.name
			,user.handle
			,user.note
			,user.comp
			,user.comment
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* very personal info */
		len+=sprintf(rec+len,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%c\t%s\t"
			,user.netmail
			,user.address
			,user.location
			,user.zipcode
			,user.pass
			,user.phone
			,user.birth
			,user.sex
			,user.modem
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* date/times */
		len+=sprintf(rec+len,"%lx\t%lx\t%lx\t%lx\t%lx\t%lx\t"
			,user.laston
			,user.firston
			,user.expire
			,user.pwmod
			,user.ns_time
			,user.logontime
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* counting stats */
		len+=sprintf(rec+len,"%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t"
			,user.logons
			,user.ltoday
			,user.timeon
			,user.textra
			,user.ttoday
			,user.tlast
			,user.posts
			,user.emails
			,user.fbacks
			,user.etoday
			,user.ptoday
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* file xfer stats, credits, minutes */
		len+=sprintf(rec+len,"%u\t%u\t%u\t%u\t%u\t%u\t%u\t"
			,user.ulb
			,user.uls
			,user.dlb
			,user.dls
			,user.cdt
			,user.freecdt
			,user.min
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* security */
		len+=sprintf(rec+len,"%u\t%lx\t%lx\t%lx\t%lx\t%lx\t%lx\t"
			,user.level
			,user.flags1
			,user.flags2
			,user.flags3
			,user.flags4
			,user.exempt
			,user.rest
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* settings (bit-fields) */
		len+=sprintf(rec+len,"%lx\t%lx\t%lx\t"
			,user.misc
			,user.qwk
			,user.chat
			);
		/* some unused records for future expansion */
		len+=sprintf(rec+len,"\t\t\t\t");	

		/******************************************/
		/* settings (strings and numbers) */
		len+=sprintf(rec+len,"%u\t%c\t%s\t%s\t%s\t%s\t%s\t%s\t"
			,user.rows
			,user.prot
			,user.xedit ? scfg.xedit[user.xedit]->code : ""
			,scfg.shell[user.shell]->code
			,user.tmpext
			,user.cursub
			,user.curdir
			,user.curxtrn
			);
		printf("reclen=%u\n",len);
		fprintf(out,"%s%*s\r\n",rec,USER_REC_LEN-len,"");
	}
	fclose(out);

	printf("\n%u user records converted\n",total);

	return(TRUE);
}

char *usage="\nusage: v4upgrade [ctrl_dir]\n";

int main(int argc, char** argv)
{
	char	error[512];
	char	revision[16];
	char*	p;
	int		first_arg=1;

	sscanf("$Revision$", "%*s %s", revision);

	fprintf(stderr,"\nV4upgrade v%s-%s - Upgrade Synchronet files from v3 to v4\n"
		,revision
		,PLATFORM_DESC
		);

	if(argc>1 && strcspn(argv[first_arg],"/\\")!=strlen(argv[first_arg]))
		p=argv[first_arg++];
	else
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

	if(!upgrade_users())
		return(1);

    return(0);
}
