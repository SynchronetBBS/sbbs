/* scfgmsg.c */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "scfg.h"

char* DLLCALL strip_slash(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i];i++)
		if(str[i]!='/' && str[i]!='\\')
			tmp[j++]=str[i];
	tmp[j]=0;
	strcpy(str,tmp);
	return(str);
}


char *utos(char *str)
{
	static char out[128];
	int i;

for(i=0;str[i];i++)
	if(str[i]=='_')
		out[i]=SP;
	else
		out[i]=str[i];
out[i]=0;
return(out);
}

char *stou(char *str)
{
	static char out[128];
	int i;

for(i=0;str[i];i++)
	if(str[i]==SP)
		out[i]='_';
	else
		out[i]=str[i];
out[i]=0;
return(out);
}

void clearptrs(int subnum)
{
	char str[256];
	ushort idx,ch;
	int file,i,gi;
	long l=0L;
	glob_t g;

    uifc.pop("Clearing Pointers...");
    sprintf(str,"%suser/ptrs/*.ixb",cfg.data_dir);

	glob(str,0,NULL,&g);
   	for(gi=0;gi<g.gl_pathc;gi++) {

        if(flength(g.gl_pathv[gi])>=((long)cfg.sub[subnum]->ptridx+1L)*10L) {
            if((file=nopen(g.gl_pathv[gi],O_WRONLY))==-1) {
                errormsg(WHERE,ERR_OPEN,g.gl_pathv[gi],O_WRONLY);
                bail(1);
            }
            while(filelength(file)<(long)(cfg.sub[subnum]->ptridx)*10) {
                lseek(file,0L,SEEK_END);
                idx=tell(file)/10;
                for(i=0;i<cfg.total_subs;i++)
                    if(cfg.sub[i]->ptridx==idx)
                        break;
                write(file,&l,4);
                write(file,&l,4);
                ch=0xff;			/* default to scan ON for unknown sub */
                if(i<cfg.total_subs) {
                    if(!(cfg.sub[i]->misc&SUB_NSDEF))
                        ch&=~5;
                    if(!(cfg.sub[i]->misc&SUB_SSDEF))
                        ch&=~2; }
                write(file,&ch,2); }
            lseek(file,((long)cfg.sub[subnum]->ptridx)*10L,SEEK_SET);
            write(file,&l,4);	/* date set to null */
            write(file,&l,4);	/* date set to null */
            ch=0xff;
            if(!(cfg.sub[subnum]->misc&SUB_NSDEF))
                ch&=~5;
            if(!(cfg.sub[subnum]->misc&SUB_SSDEF))
                ch&=~2;
            write(file,&ch,2);
            close(file); }
        }
	globfree(&g);
    uifc.pop(0);
}

void msgs_cfg()
{
	static int dflt,msgs_dflt,bar;
	char str[256],str2[256],done=0,*p;
    char tmp[128];
	int j,k,q,s;
	int i,file,ptridx,n;
	long ported;
	sub_t tmpsub;
	static grp_t savgrp;
	FILE *stream;

while(1) {
	for(i=0;i<cfg.total_grps && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",cfg.grp[i]->lname);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_CHE;
	if(cfg.total_grps)
		j|=WIN_DEL|WIN_DELACT|WIN_GET;
	if(cfg.total_grps<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savgrp.sname[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Message Groups:

This is a listing of message groups for your BBS. Message groups are
used to logically separate your message sub-boards into groups. Every
sub-board belongs to a message group. You must have at least one message
group and one sub-board configured.

One popular use for message groups is to separate local sub-boards and
networked sub-boards. One might have a Local message group that contains
non-networked sub-boards of various topics and also have a FidoNet
message group that contains sub-boards that are echoed across FidoNet.
Some sysops separate sub-boards into more specific areas such as Main,
Technical, or Adult. If you have many sub-boards that have a common
subject denominator, you may want to have a separate message group for
those sub-boards for a more organized message structure.
*/
	i=uifc.list(j,0,0,45,&msgs_dflt,&bar,"Message Groups",opt);
	uifc.savnum=0;
	if(i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
		   continue;
		if(!j) {
			write_msgs_cfg(&cfg,backup_level);
            rerun_nodes();
        }
		return;
    }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Group Long Name:

This is a description of the message group which is displayed when a
user of the system uses the /* command from the main menu.
*/
		strcpy(str,"Main");
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Group Long Name",str,LEN_GLNAME
			,K_EDIT)<1)
			continue;
		SETHELP(WHERE);
/*
Group Short Name:

This is a short description of the message group which is used for the
main menu and reading message prompts.
*/
		sprintf(str2,"%.*s",LEN_GSNAME,str);
		if(uifc.input(WIN_MID,0,0,"Group Short Name",str2,LEN_GSNAME,K_EDIT)<1)
			continue;
		if((cfg.grp=(grp_t **)REALLOC(cfg.grp,sizeof(grp_t *)*(cfg.total_grps+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_grps+1);
			cfg.total_grps=0;
			bail(1);
            continue; }

		if(cfg.total_grps) {	/* was cfg.total_subs (?) */
            for(j=cfg.total_grps;j>i;j--)   /* insert above */
                cfg.grp[j]=cfg.grp[j-1];
            for(j=0;j<cfg.total_subs;j++)   /* move sub group numbers */
                if(cfg.sub[j]->grp>=i)
                    cfg.sub[j]->grp++; }

		if((cfg.grp[i]=(grp_t *)MALLOC(sizeof(grp_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(grp_t));
			continue; }
		memset((grp_t *)cfg.grp[i],0,sizeof(grp_t));
		strcpy(cfg.grp[i]->lname,str);
		strcpy(cfg.grp[i]->sname,str2);
		cfg.total_grps++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		SETHELP(WHERE);
/*
Delete All Data in Group:

If you wish to delete the messages in all the sub-boards in this group,
select Yes.
*/
		j=1;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Delete All Data in Group",opt);
		if(j==-1)
			continue;
		if(j==0)
			for(j=0;j<cfg.total_subs;j++)
				if(cfg.sub[j]->grp==i) {
					sprintf(str,"%s.s*",cfg.sub[j]->code);
					if(!cfg.sub[j]->data_dir[0])
						sprintf(tmp,"%ssubs/",cfg.data_dir);
					else
						strcpy(tmp,cfg.sub[j]->data_dir);
					delfiles(tmp,str);
					clearptrs(j); }
		FREE(cfg.grp[i]);
		for(j=0;j<cfg.total_subs;) {
			if(cfg.sub[j]->grp==i) {	/* delete subs of this group */
				FREE(cfg.sub[j]);
				cfg.total_subs--;
				k=j;
				while(k<cfg.total_subs) {	/* move all subs down */
					cfg.sub[k]=cfg.sub[k+1];
					for(q=0;q<cfg.total_qhubs;q++)
						for(s=0;s<cfg.qhub[q]->subs;s++)
							if(cfg.qhub[q]->sub[s]==k)
								cfg.qhub[q]->sub[s]--;
					k++; } }
			else j++; }
		for(j=0;j<cfg.total_subs;j++)	/* move sub group numbers down */
			if(cfg.sub[j]->grp>i)
				cfg.sub[j]->grp--;
		cfg.total_grps--;
		while(i<cfg.total_grps) {
			cfg.grp[i]=cfg.grp[i+1];
			i++; }
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savgrp=*cfg.grp[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.grp[i]=savgrp;
		uifc.changes=1;
		continue; }
	done=0;
	while(!done) {
		j=0;
		sprintf(opt[j++],"%-27.27s%s","Long Name",cfg.grp[i]->lname);
		sprintf(opt[j++],"%-27.27s%s","Short Name",cfg.grp[i]->sname);
		sprintf(opt[j++],"%-27.27s%.40s","Access Requirements"
			,cfg.grp[i]->arstr);
		strcpy(opt[j++],"Clone Options");
		strcpy(opt[j++],"Export Areas...");
		strcpy(opt[j++],"Import Areas...");
		strcpy(opt[j++],"Message Sub-boards...");
		opt[j][0]=0;
		sprintf(str,"%s Group",cfg.grp[i]->sname);
		uifc.savnum=0;
		SETHELP(WHERE);
/*
Message Group Configuration:

This menu allows you to configure the security requirements for access
to this message group. You can also add, delete, and configure the
sub-boards of this group by selecting the Messages Sub-boards... option.
*/
		switch(uifc.list(WIN_ACT,6,4,60,&dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Group Long Name:

This is a description of the message group which is displayed when a
user of the system uses the /* command from the main menu.
*/
				strcpy(str,cfg.grp[i]->lname);	/* save incase setting to null */
				if(!uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Listings"
					,cfg.grp[i]->lname,LEN_GLNAME,K_EDIT))
					strcpy(cfg.grp[i]->lname,str);
				break;
			case 1:
				SETHELP(WHERE);
/*
Group Short Name:

This is a short description of the message group which is used for
main menu and reading messages prompts.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Prompts"
					,cfg.grp[i]->sname,LEN_GSNAME,K_EDIT);
				break;
			case 2:
				sprintf(str,"%s Group",cfg.grp[i]->sname);
				getar(str,cfg.grp[i]->arstr);
				break;
			case 3: 	/* Clone Options */
				j=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				SETHELP(WHERE);
/*
Clone Sub-board Options:

If you want to clone the options of the first sub-board of this group
into all sub-boards of this group, select Yes.

The options cloned are posting requirements, reading requirements,
operator requirments, moderated user requirments, toggle options,
network options (including EchoMail origin line, EchoMail address,
and QWK Network tagline), maximum number of messages, maximum number
of CRCs, maximum age of messages, storage method, and data directory.
*/
				j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
					,"Clone Options of First Sub-board into All of Group",opt);
				if(j==0) {
					k=-1;
					for(j=0;j<cfg.total_subs;j++)
						if(cfg.sub[j]->grp==i) {
							if(k==-1)
								k=j;
							else {
								uifc.changes=1;
								cfg.sub[j]->misc=(cfg.sub[k]->misc|SUB_HDRMOD);
								strcpy(cfg.sub[j]->post_arstr,cfg.sub[k]->post_arstr);
								strcpy(cfg.sub[j]->read_arstr,cfg.sub[k]->read_arstr);
								strcpy(cfg.sub[j]->op_arstr,cfg.sub[k]->op_arstr);
								strcpy(cfg.sub[j]->mod_arstr,cfg.sub[k]->mod_arstr);
								strcpy(cfg.sub[j]->origline,cfg.sub[k]->origline);
								strcpy(cfg.sub[j]->tagline,cfg.sub[k]->tagline);
								strcpy(cfg.sub[j]->data_dir,cfg.sub[k]->data_dir);
								strcpy(cfg.sub[j]->echomail_sem
									,cfg.sub[k]->echomail_sem);
								cfg.sub[j]->maxmsgs=cfg.sub[k]->maxmsgs;
								cfg.sub[j]->maxcrcs=cfg.sub[k]->maxcrcs;
								cfg.sub[j]->maxage=cfg.sub[k]->maxage;

								cfg.sub[j]->faddr=cfg.sub[k]->faddr; } } }
				break;
			case 4:
				k=0;
				ported=0;
				q=uifc.changes;
				strcpy(opt[k++],"SUBS.TXT    (Synchronet)");
				strcpy(opt[k++],"AREAS.BBS   (MSG)");
				strcpy(opt[k++],"AREAS.BBS   (SMB)");
				strcpy(opt[k++],"AREAS.BBS   (SBBSECHO)");
				strcpy(opt[k++],"FIDONET.NA  (Fido)");
				opt[k][0]=0;
				SETHELP(WHERE);
/*
Export Area File Format:

This menu allows you to choose the format of the area file you wish to
export the current message group into.
*/
				k=0;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Export Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sSUBS.TXT",cfg.ctrl_dir);
				else if(k==1 || k==2)
					sprintf(str,"AREAS.BBS");
				else if(k==3)
					sprintf(str,"%sAREAS.BBS",cfg.data_dir);
				else if(k==4)
					sprintf(str,"FIDONET.NA");
				strupr(str);
				if(k && k<4)
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"Uplinks"
						,str2,40,0)<=0) {
						uifc.changes=q;
						break; }
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename"
					,str,40,K_EDIT)<=0) {
					uifc.changes=q;
					break; }
				if(fexist(str)) {
					strcpy(opt[0],"Overwrite");
					strcpy(opt[1],"Append");
					opt[2][0]=0;
					j=0;
					j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"File Exists",opt);
					if(j==-1)
						break;
					if(j==0) j=O_WRONLY|O_TRUNC;
					else	 j=O_WRONLY|O_APPEND; }
				else
					j=O_WRONLY|O_CREAT;
				if((stream=fnopen(&file,str,j))==NULL) {
					sprintf(str,"Open Failure: %d (%s)"
						,errno,strerror(errno));
					uifc.msg(str);
					uifc.changes=q;
					break; 
				}
				uifc.pop("Exporting Areas...");
				for(j=0;j<cfg.total_subs;j++) {
					if(cfg.sub[j]->grp!=i)
						continue;
					ported++;
					if(k==1) {		/* AREAS.BBS *.MSG */
						sprintf(str,"%s%s/",cfg.echomail_dir,cfg.sub[j]->code);
						fprintf(stream,"%-30s %-20s %s\r\n"
							,str,stou(cfg.sub[j]->sname),str2);
						continue; }
					if(k==2) {		/* AREAS.BBS SMB */
						if(!cfg.sub[j]->data_dir[0])
							sprintf(str,"%ssubs/%s",cfg.data_dir,cfg.sub[j]->code);
						else
							sprintf(str,"%s%s",cfg.sub[j]->data_dir,cfg.sub[j]->code);
						fprintf(stream,"%-30s %-20s %s\r\n"
							,str,stou(cfg.sub[j]->sname),str2);
                        continue; }
					if(k==3) {		/* AREAS.BBS SBBSECHO */
						fprintf(stream,"%-30s %-20s %s\r\n"
							,cfg.sub[j]->code,stou(cfg.sub[j]->sname),str2);
						continue; }
					if(k==4) {		/* FIDONET.NA */
						fprintf(stream,"%-20s %s\r\n"
							,stou(cfg.sub[j]->sname),cfg.sub[j]->lname);
						continue; }
					fprintf(stream,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
							"%s\r\n%s\r\n%s\r\n"
						,cfg.sub[j]->lname
						,cfg.sub[j]->sname
						,cfg.sub[j]->qwkname
						,cfg.sub[j]->code
						,cfg.sub[j]->data_dir
						,cfg.sub[j]->arstr
						,cfg.sub[j]->read_arstr
						,cfg.sub[j]->post_arstr
						,cfg.sub[j]->op_arstr
						);
					fprintf(stream,"%lX\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
						,cfg.sub[j]->misc
						,cfg.sub[j]->tagline
						,cfg.sub[j]->origline
						,cfg.sub[j]->echomail_sem
						,cfg.sub[j]->newsgroup
						,faddrtoa(&cfg.sub[j]->faddr,tmp)
						);
					fprintf(stream,"%lu\r\n%lu\r\n%u\r\n%u\r\n%s\r\n"
						,cfg.sub[j]->maxmsgs
						,cfg.sub[j]->maxcrcs
						,cfg.sub[j]->maxage
						,cfg.sub[j]->ptridx
						,cfg.sub[j]->mod_arstr
						);
					fprintf(stream,"***END-OF-SUB***\r\n\r\n"); }
				fclose(stream);
				uifc.pop(0);
				sprintf(str,"%lu Message Areas Exported Successfully",ported);
				uifc.msg(str);
				uifc.changes=q;
				break;
			case 5:
				ported=0;
				k=0;
				strcpy(opt[k++],"SUBS.TXT    (Synchronet)");
				strcpy(opt[k++],"AREAS.BBS   (MSG)");
				strcpy(opt[k++],"AREAS.BBS   (SMB)");
				strcpy(opt[k++],"AREAS.BBS   (SBBSECHO)");
				strcpy(opt[k++],"FIDONET.NA  (Fido)");
				opt[k][0]=0;
				SETHELP(WHERE);
/*
Import Area File Format:

This menu allows you to choose the format of the area file you wish to
import into the current message group.
*/
				k=0;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Import Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sSUBS.TXT",cfg.ctrl_dir);
				else if(k==1 || k==2)
					sprintf(str,"AREAS.BBS");
				else if(k==3)
					sprintf(str,"%sAREAS.BBS",cfg.data_dir);
				else if(k==4)
					sprintf(str,"FIDONET.NA");
				strupr(str);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename"
					,str,40,K_EDIT)<=0)
                    break;
				if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
					uifc.msg("Open Failure");
                    break; }
				uifc.pop("Importing Areas...");
				while(!feof(stream)) {
					if(!fgets(str,128,stream)) break;
					truncsp(str);
					if(!str[0])
						continue;
					if(k) {
						p=str;
						while(*p && *p<=SP) p++;
						if(!*p || *p==';')
							continue;
						memset(&tmpsub,0,sizeof(sub_t));
						tmpsub.misc|=
							(SUB_FIDO|SUB_NAME|SUB_TOUSER|SUB_QUOTE|SUB_HYPER);
						if(k==1) {		/* AREAS.BBS *.MSG */
							p=strrchr(str,'\\');
                            if(p==NULL) p=strrchr(str,'/');
                            if(p) *p=0;
                            else p=str;
							//sprintf(tmpsub.echopath,"%.*s",LEN_DIR,str);
							p++;
							sprintf(tmpsub.code,"%.8s",p);
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,p);
							p=strchr(tmpsub.sname,SP);
							if(p) *p=0;
							strcpy(tmpsub.sname,utos(tmpsub.sname));
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME
								,tmpsub.sname);
							sprintf(tmpsub.qwkname,"%.*s",10
                                ,tmpsub.sname);
							}
						if(k==2) {		/* AREAS.BBS SMB */
							p=strrchr(str,'\\');
                            if(p==NULL) p=strrchr(str,'/');                            
                            if(p) *p=0;
                            else p=str;
							sprintf(tmpsub.data_dir,"%.*s",LEN_DIR,str);
							p++;
							sprintf(tmpsub.code,"%.8s",p);
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,p);
							p=strchr(tmpsub.sname,SP);
							if(p) *p=0;
							strcpy(tmpsub.sname,utos(tmpsub.sname));
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME
								,tmpsub.sname);
							sprintf(tmpsub.qwkname,"%.*s",10
                                ,tmpsub.sname);
                            }
						else if(k==3) { /* AREAS.BBS SBBSECHO */
							p=str;
							while(*p && *p>SP) p++;
							*p=0;
							sprintf(tmpsub.code,"%.8s",str);
							p++;
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,p);
							p=strchr(tmpsub.sname,SP);
							if(p) *p=0;
							strcpy(tmpsub.sname,utos(tmpsub.sname));
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME
								,tmpsub.sname);
							sprintf(tmpsub.qwkname,"%.*s",10
                                ,tmpsub.sname);
							}
						else if(k==4) { /* FIDONET.NA */
                            p=str;
							while(*p && *p>SP) p++;
							*p=0;
							sprintf(tmpsub.code,"%.8s",str);
							sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,utos(str));
							sprintf(tmpsub.qwkname,"%.10s",tmpsub.sname);
							p++;
							while(*p && *p<=SP) p++;
							sprintf(tmpsub.lname,"%.*s",LEN_SLNAME,p);
							}
						ported++; }
					else {
						memset(&tmpsub,0,sizeof(sub_t));
						tmpsub.grp=i;
						sprintf(tmpsub.lname,"%.*s",LEN_SLNAME,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.sname,"%.*s",LEN_SSNAME,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.qwkname,"%.*s",10,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.code,"%.*s",8,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.data_dir,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.arstr,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.read_arstr,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.post_arstr,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.op_arstr,"%.*s",LEN_ARSTR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.misc=ahtoul(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.tagline,"%.*s",80,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.origline,"%.*s",50,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.echomail_sem,"%.*s",LEN_DIR,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						SAFECOPY(tmpsub.newsgroup,str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.faddr=atofaddr(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.maxmsgs=atol(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.maxcrcs=atol(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.maxage=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						tmpsub.ptridx=atoi(str);
						if(!fgets(str,128,stream)) break;
						truncsp(str);
						sprintf(tmpsub.mod_arstr,"%.*s",LEN_ARSTR,str);

						ported++;
						while(!feof(stream)
							&& strcmp(str,"***END-OF-SUB***")) {
							if(!fgets(str,128,stream)) break;
							truncsp(str); } }

					truncsp(tmpsub.code);
                    strip_slash(tmpsub.code);
					truncsp(tmpsub.sname);
					truncsp(tmpsub.lname);
					truncsp(tmpsub.qwkname);
					for(j=0;j<cfg.total_subs;j++) {
						if(cfg.sub[j]->grp!=i)
							continue;
						if(!stricmp(cfg.sub[j]->code,tmpsub.code))
							break; }
					if(j==cfg.total_subs) {

						if((cfg.sub=(sub_t **)REALLOC(cfg.sub
							,sizeof(sub_t *)*(cfg.total_subs+1)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_subs+1);
							cfg.total_subs=0;
							bail(1);
							break; }

						for(ptridx=0;ptridx>-1;ptridx++) {
							for(n=0;n<cfg.total_subs;n++)
								if(cfg.sub[n]->ptridx==ptridx)
									break;
							if(n==cfg.total_subs)
								break; }

						if((cfg.sub[j]=(sub_t *)MALLOC(sizeof(sub_t)))
							==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(sub_t));
							break; }
						memset(cfg.sub[j],0,sizeof(sub_t)); }
					if(!k)
						memcpy(cfg.sub[j],&tmpsub,sizeof(sub_t));
					else {
                        cfg.sub[j]->grp=i;
						if(cfg.total_faddrs)
							cfg.sub[j]->faddr=cfg.faddr[0];
						strcpy(cfg.sub[j]->code,tmpsub.code);
						strcpy(cfg.sub[j]->sname,tmpsub.sname);
						strcpy(cfg.sub[j]->lname,tmpsub.lname);
						strcpy(cfg.sub[j]->qwkname,tmpsub.qwkname);
						//strcpy(cfg.sub[j]->echopath,tmpsub.echopath);
						strcpy(cfg.sub[j]->data_dir,tmpsub.data_dir);
						if(j==cfg.total_subs)
							cfg.sub[j]->maxmsgs=1000;
						}
					if(j==cfg.total_subs) {
						cfg.sub[j]->ptridx=ptridx;
						cfg.sub[j]->misc=tmpsub.misc;
						cfg.total_subs++; }
					uifc.changes=1; }
				fclose(stream);
				uifc.pop(0);
				sprintf(str,"%lu Message Areas Imported Successfully",ported);
                uifc.msg(str);
				break;

			case 6:
                sub_cfg(i);
				break; } } }

}

void msg_opts()
{
	char str[128],*p;
	static int msg_dflt;
	int i,j;

	while(1) {
		i=0;
		sprintf(opt[i++],"%-33.33s%s"
			,"BBS ID for QWK Packets",cfg.sys_id);
		sprintf(opt[i++],"%-33.33s%s"
			,"Local Time Zone",zonestr(cfg.sys_timezone));
		sprintf(opt[i++],"%-33.33s%u seconds"
			,"Maximum Retry Time",cfg.smb_retry_time);
		if(cfg.max_qwkmsgs)
			sprintf(str,"%lu",cfg.max_qwkmsgs);
		else
			sprintf(str,"Unlimited");
		sprintf(opt[i++],"%-33.33s%s"
			,"Maximum QWK Messages",str);
		sprintf(opt[i++],"%-33.33s%s","Pre-pack QWK Requirements",cfg.preqwk_arstr);
		if(cfg.mail_maxage)
			sprintf(str,"Enabled (%u days old)",cfg.mail_maxage);
        else
            strcpy(str,"Disabled");
		sprintf(opt[i++],"%-33.33s%s","Purge E-mail by Age",str);
		if(cfg.sys_misc&SM_DELEMAIL)
			strcpy(str,"Immediately");
		else
			strcpy(str,"Daily");
		sprintf(opt[i++],"%-33.33s%s","Purge Deleted E-mail",str);
		if(cfg.mail_maxcrcs)
			sprintf(str,"Enabled (%lu mail CRCs)",cfg.mail_maxcrcs);
		else
			strcpy(str,"Disabled");
		sprintf(opt[i++],"%-33.33s%s","Duplicate E-mail Checking",str);
		sprintf(opt[i++],"%-33.33s%s","Allow Anonymous E-mail"
			,cfg.sys_misc&SM_ANON_EM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Allow Quoting in E-mail"
			,cfg.sys_misc&SM_QUOTE_EM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Allow Uploads in E-mail"
			,cfg.sys_misc&SM_FILE_EM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Allow Forwarding to NetMail"
			,cfg.sys_misc&SM_FWDTONET ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Kill Read E-mail"
			,cfg.sys_misc&SM_DELREADM ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Users Can View Deleted Messages"
			,cfg.sys_misc&SM_USRVDELM ? "Yes" : cfg.sys_misc&SM_SYSVDELM
				? "Sysops Only":"No");
		strcpy(opt[i++],"Extra Attribute Codes...");
		opt[i][0]=0;
		uifc.savnum=0;
		SETHELP(WHERE);
/*
Message Options:

This is a menu of system-wide message related options. Messages include
E-mail and public posts (on sub-boards).
*/

		switch(uifc.list(WIN_ORG|WIN_ACT|WIN_MID|WIN_CHE,0,0,72,&msg_dflt,0
			,"Message Options",opt)) {
			case -1:
				i=save_changes(WIN_MID);
				if(i==-1)
				   continue;
				if(!i) {
					write_msgs_cfg(&cfg,backup_level);
					write_main_cfg(&cfg,backup_level);
                    rerun_nodes();
                }
				return;
			case 0:
				strcpy(str,cfg.sys_id);
				SETHELP(WHERE);
/*
BBS ID for QWK Packets:

This is a short system ID for your BBS that is used for QWK packets.
It should be an abbreviation of your BBS name or other related string.
This ID will be used for your outgoing and incoming QWK packets. If
you plan on networking via QWK packets with another Synchronet BBS,
this ID should not begin with a number. The maximum length of the ID
is eight characters and cannot contain spaces or other invalid DOS
filename characters. In a QWK packet network, each system must have
a unique QWK system ID.
*/

				uifc.input(WIN_MID|WIN_SAV,0,0,"BBS ID for QWK Packets"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(cfg.sys_id,str);
				else
					uifc.msg("Invalid ID");
				break;
			case 1:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
United States Time Zone:

If your local time zone is the United States, select Yes.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"United States Time Zone",opt);
				if(i==-1)
					break;
				if(i==0) {
					strcpy(opt[i++],"Atlantic");
					strcpy(opt[i++],"Eastern");
					strcpy(opt[i++],"Central");
					strcpy(opt[i++],"Mountain");
					strcpy(opt[i++],"Pacific");
					strcpy(opt[i++],"Yukon");
					strcpy(opt[i++],"Hawaii/Alaska");
					strcpy(opt[i++],"Bering");
					opt[i][0]=0;
					i=0;
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Time Zone",opt);
					if(i==-1)
						break;
					uifc.changes=1;
					switch(i) {
						case 0:
							cfg.sys_timezone=AST;
							break;
						case 1:
							cfg.sys_timezone=EST;
							break;
						case 2:
							cfg.sys_timezone=CST;
                            break;
						case 3:
							cfg.sys_timezone=MST;
                            break;
						case 4:
							cfg.sys_timezone=PST;
                            break;
						case 5:
							cfg.sys_timezone=YST;
                            break;
						case 6:
							cfg.sys_timezone=HST;
                            break;
						case 7:
							cfg.sys_timezone=BST;
							break; }
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					i=1;
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Daylight Savings",opt);
					if(i==-1)
                        break;
					if(!i)
						cfg.sys_timezone|=DAYLIGHT;
					break; }
				i=0;
				strcpy(opt[i++],"Midway");
				strcpy(opt[i++],"Vancouver");
				strcpy(opt[i++],"Edmonton");
				strcpy(opt[i++],"Winnipeg");
				strcpy(opt[i++],"Bogota");
				strcpy(opt[i++],"Caracas");
				strcpy(opt[i++],"Rio de Janeiro");
				strcpy(opt[i++],"Fernando de Noronha");
				strcpy(opt[i++],"Azores");
				strcpy(opt[i++],"London");
				strcpy(opt[i++],"Berlin");
				strcpy(opt[i++],"Athens");
				strcpy(opt[i++],"Moscow");
				strcpy(opt[i++],"Dubai");
				strcpy(opt[i++],"Kabul");
				strcpy(opt[i++],"Karachi");
				strcpy(opt[i++],"Bombay");
				strcpy(opt[i++],"Kathmandu");
				strcpy(opt[i++],"Dhaka");
				strcpy(opt[i++],"Bangkok");
				strcpy(opt[i++],"Hong Kong");
				strcpy(opt[i++],"Tokyo");
				strcpy(opt[i++],"Sydney");
				strcpy(opt[i++],"Noumea");
				strcpy(opt[i++],"Wellington");
				strcpy(opt[i++],"Other...");
				opt[i][0]=0;
				i=0;
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Time Zone",opt);
				if(i==-1)
					break;
				uifc.changes=1;
				switch(i) {
					case 0:
						cfg.sys_timezone=MID;
						break;
					case 1:
						cfg.sys_timezone=VAN;
						break;
					case 2:
						cfg.sys_timezone=EDM;
						break;
					case 3:
						cfg.sys_timezone=WIN;
						break;
					case 4:
						cfg.sys_timezone=BOG;
						break;
					case 5:
						cfg.sys_timezone=CAR;
						break;
					case 6:
						cfg.sys_timezone=RIO;
						break;
					case 7:
						cfg.sys_timezone=FER;
						break;
					case 8:
						cfg.sys_timezone=AZO;
                        break;
					case 9:
						cfg.sys_timezone=LON;
                        break;
					case 10:
						cfg.sys_timezone=BER;
                        break;
					case 11:
						cfg.sys_timezone=ATH;
                        break;
					case 12:
						cfg.sys_timezone=MOS;
                        break;
					case 13:
						cfg.sys_timezone=DUB;
                        break;
					case 14:
						cfg.sys_timezone=KAB;
                        break;
					case 15:
						cfg.sys_timezone=KAR;
                        break;
					case 16:
						cfg.sys_timezone=BOM;
                        break;
					case 17:
						cfg.sys_timezone=KAT;
                        break;
					case 18:
						cfg.sys_timezone=DHA;
                        break;
					case 19:
						cfg.sys_timezone=BAN;
                        break;
					case 20:
						cfg.sys_timezone=HON;
                        break;
					case 21:
						cfg.sys_timezone=TOK;
                        break;
					case 22:
						cfg.sys_timezone=SYD;
                        break;
					case 23:
						cfg.sys_timezone=NOU;
                        break;
					case 24:
						cfg.sys_timezone=WEL;
                        break;
					default:
						if(cfg.sys_timezone>720 || cfg.sys_timezone<-720)
							cfg.sys_timezone=0;
						sprintf(str,"%02d:%02d"
							,cfg.sys_timezone/60,cfg.sys_timezone<0
							? (-cfg.sys_timezone)%60 : cfg.sys_timezone%60);
						uifc.input(WIN_MID|WIN_SAV,0,0
							,"Time (HH:MM) East (+) or West (-) of Universal "
								"Time"
							,str,6,K_EDIT|K_UPPER);
						cfg.sys_timezone=atoi(str)*60;
						p=strchr(str,':');
						if(p) {
							if(cfg.sys_timezone<0)
								cfg.sys_timezone-=atoi(p+1);
							else
								cfg.sys_timezone+=atoi(p+1); }
                        break;
						}
                break;
			case 2:
				SETHELP(WHERE);
/*
Maximum Message Base Retry Time:

This is the maximum number of seconds to allow while attempting to open
or lock a message base (a value in the range of 10 to 45 seconds should
be fine).
*/
				ultoa(cfg.smb_retry_time,str,10);
				uifc.input(WIN_MID|WIN_SAV,0,0
					,"Maximum Message Base Retry Time (in seconds)"
					,str,2,K_NUMBER|K_EDIT);
				cfg.smb_retry_time=atoi(str);
				break;
			case 3:
				SETHELP(WHERE);
/*
Maximum Messages Per QWK Packet:

This is the maximum number of messages (excluding E-mail), that a user
can have in one QWK packet for download. This limit does not effect
QWK network nodes (Q restriction). If set to 0, no limit is imposed.
*/

				ultoa(cfg.max_qwkmsgs,str,10);
				uifc.input(WIN_MID|WIN_SAV,0,0
					,"Maximum Messages Per QWK Packet (0=No Limit)"
					,str,6,K_NUMBER|K_EDIT);
				cfg.max_qwkmsgs=atol(str);
                break;
			case 4:
				SETHELP(WHERE);
/*
Pre-pack QWK Requirements:

ALL user accounts on the BBS meeting this requirmenet will have a QWK
packet automatically created for them every day after midnight
(during the internal daily event).

This is mainly intended for QWK network nodes that wish to save connect
time by having their packets pre-packed. If a large number of users meet
this requirement, it can take up a large amount of disk space on your
system (in the DATA\FILE directory).
*/
				getar("Pre-pack QWK (Use with caution!)",cfg.preqwk_arstr);
				break;
			case 5:
				sprintf(str,"%u",cfg.mail_maxage);
                SETHELP(WHERE);
/*
Maximum Age of Mail:

This value is the maximum number of days that mail will be kept.
*/
                uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Age of Mail "
                    "(in days)",str,5,K_EDIT|K_NUMBER);
                cfg.mail_maxage=atoi(str);
                break;
			case 6:
				strcpy(opt[0],"Daily");
				strcpy(opt[1],"Immediately");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Purge Deleted E-mail:

If you wish to have deleted e-mail physically (and permanently) removed
from your e-mail database immediately after a users exits the reading
e-mail prompt, set this option to Immediately.

For the best system performance and to avoid delays when deleting e-mail
from a large e-mail database, set this option to Daily (the default).
Your system maintenance will automatically purge deleted e-mail once a
day.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Purge Deleted E-mail",opt);
				if(!i && cfg.sys_misc&SM_DELEMAIL) {
					cfg.sys_misc&=~SM_DELEMAIL;
					uifc.changes=1; }
				else if(i==1 && !(cfg.sys_misc&SM_DELEMAIL)) {
					cfg.sys_misc|=SM_DELEMAIL;
					uifc.changes=1; }
                break;
			case 7:
				sprintf(str,"%lu",cfg.mail_maxcrcs);
                SETHELP(WHERE);
/*
Maximum Number of Mail CRCs:

This value is the maximum number of CRCs that will be kept for duplicate
mail checking. Once this maximum number of CRCs is reached, the oldest
CRCs will be automatically purged.
*/
                uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Number of Mail "
                    "CRCs",str,5,K_EDIT|K_NUMBER);
                cfg.mail_maxcrcs=atol(str);
                break;
			case 8:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=1;
				SETHELP(WHERE);
/*
Allow Anonymous E-mail:

If you want users with the A exemption to be able to send E-mail
anonymously, set this option to Yes.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Anonymous E-mail",opt);
				if(!i && !(cfg.sys_misc&SM_ANON_EM)) {
					cfg.sys_misc|=SM_ANON_EM;
					uifc.changes=1; }
				else if(i==1 && cfg.sys_misc&SM_ANON_EM) {
					cfg.sys_misc&=~SM_ANON_EM;
					uifc.changes=1; }
				break;
			case 9:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Allow Quoting in E-mail:

If you want your users to be allowed to use message quoting when
responding in E-mail, set this option to Yes.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Quoting in E-mail",opt);
				if(!i && !(cfg.sys_misc&SM_QUOTE_EM)) {
					cfg.sys_misc|=SM_QUOTE_EM;
					uifc.changes=1; }
				else if(i==1 && cfg.sys_misc&SM_QUOTE_EM) {
					cfg.sys_misc&=~SM_QUOTE_EM;
					uifc.changes=1; }
				break;
			case 10:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Allow File Attachment Uploads in E-mail:

If you want your users to be allowed to attach an uploaded file to
an E-mail message, set this option to Yes.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow File Attachment Uploads in E-mail",opt);
				if(!i && !(cfg.sys_misc&SM_FILE_EM)) {
					cfg.sys_misc|=SM_FILE_EM;
					uifc.changes=1; }
				else if(i==1 && cfg.sys_misc&SM_FILE_EM) {
					cfg.sys_misc&=~SM_FILE_EM;
					uifc.changes=1; }
				break;
			case 11:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=0;
				SETHELP(WHERE);
/*
Allow Users to Have Their E-mail Forwarded to NetMail:

If you want your users to be able to have any e-mail sent to them
optionally (at the sender's discretion) forwarded to a NetMail address,
set this option to Yes.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Forwarding of E-mail to NetMail",opt);
				if(!i && !(cfg.sys_misc&SM_FWDTONET)) {
					cfg.sys_misc|=SM_FWDTONET;
					uifc.changes=1; }
				else if(i==1 && cfg.sys_misc&SM_FWDTONET) {
					cfg.sys_misc&=~SM_FWDTONET;
					uifc.changes=1; }
                break;
			case 12:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=1;
				SETHELP(WHERE);
/*
Kill Read E-mail Automatically:

If this option is set to Yes, e-mail that has been read will be
automatically deleted when message base maintenance is run.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Kill Read E-mail Automatically",opt);
				if(!i && !(cfg.sys_misc&SM_DELREADM)) {
					cfg.sys_misc|=SM_DELREADM;
					uifc.changes=1; }
				else if(i==1 && cfg.sys_misc&SM_DELREADM) {
					cfg.sys_misc&=~SM_DELREADM;
					uifc.changes=1; }
                break;
			case 13:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				strcpy(opt[2],"Sysops Only");
				opt[3][0]=0;
				i=1;
				SETHELP(WHERE);
/*
Users Can View Deleted Messages:

If this option is set to Yes, then users will be able to view messages
they've sent and deleted or messages sent to them and they've deleted
with the option of un-deleting the message before the message is
physically purged from the e-mail database.

If this option is set to No, then when a message is deleted, it is no
longer viewable (with SBBS) by anyone.

If this option is set to Sysops Only, then only sysops and sub-ops (when
appropriate) can view deleted messages.
*/

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Users Can View Deleted Messages",opt);
				if(!i && (cfg.sys_misc&(SM_USRVDELM|SM_SYSVDELM))
					!=(SM_USRVDELM|SM_SYSVDELM)) {
					cfg.sys_misc|=(SM_USRVDELM|SM_SYSVDELM);
					uifc.changes=1; }
				else if(i==1 && cfg.sys_misc&(SM_USRVDELM|SM_SYSVDELM)) {
					cfg.sys_misc&=~(SM_USRVDELM|SM_SYSVDELM);
					uifc.changes=1; }
				else if(i==2 && (cfg.sys_misc&(SM_USRVDELM|SM_SYSVDELM))
					!=SM_SYSVDELM) {
					cfg.sys_misc|=SM_SYSVDELM;
					cfg.sys_misc&=~SM_USRVDELM;
					uifc.changes=1; }
                break;
			case 14:
				SETHELP(WHERE);
/*
Extra Attribute Codes...

Synchronet can suppport the native text attribute codes of other BBS
programs in messages (menus, posts, e-mail, etc.) To enable the extra
attribute codes for another BBS program, set the corresponding option
to Yes.
*/

				j=0;
				while(1) {
					i=0;
					sprintf(opt[i++],"%-15.15s %-3.3s","WWIV"
						,cfg.sys_misc&SM_WWIV ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","PCBoard"
						,cfg.sys_misc&SM_PCBOARD ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","Wildcat"
						,cfg.sys_misc&SM_WILDCAT ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","Celerity"
						,cfg.sys_misc&SM_CELERITY ? "Yes":"No");
					sprintf(opt[i++],"%-15.15s %-3.3s","Renegade"
						,cfg.sys_misc&SM_RENEGADE ? "Yes":"No");
					opt[i][0]=0;
					j=uifc.list(WIN_BOT|WIN_RHT|WIN_SAV,2,2,0,&j,0
						,"Extra Attribute Codes",opt);
					if(j==-1)
						break;

					uifc.changes=1;
					switch(j) {
						case 0:
							cfg.sys_misc^=SM_WWIV;
							break;
						case 1:
							cfg.sys_misc^=SM_PCBOARD;
							break;
						case 2:
							cfg.sys_misc^=SM_WILDCAT;
							break;
						case 3:
							cfg.sys_misc^=SM_CELERITY;
							break;
						case 4:
							cfg.sys_misc^=SM_RENEGADE;
							break; } } } }
}
