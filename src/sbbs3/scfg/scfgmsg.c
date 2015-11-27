/* scfgmsg.c */

/* $Id$ */

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

#include "scfg.h"
#include "strwrap.h"	/* itoa() */

char *utos(char *str)
{
	static char out[128];
	int i;

	for(i=0;str[i] && i<sizeof(out)-1;i++)
		if(str[i]=='_')
			out[i]=' ';
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
	if(str[i]==' ')
		out[i]='_';
	else
		out[i]=str[i];
out[i]=0;
return(out);
}

void clearptrs(int subnum)
{
	char str[256];
	ushort idx,scancfg;
	int file, i;
	size_t gi;
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
                idx=(ushort)(tell(file)/10);
                for(i=0;i<cfg.total_subs;i++)
                    if(cfg.sub[i]->ptridx==idx)
                        break;
                write(file,&l,sizeof(l));
                write(file,&l,sizeof(l));
                scancfg=0xff;
                if(i<cfg.total_subs) {
                    if(!(cfg.sub[i]->misc&SUB_NSDEF))
                        scancfg&=~SUB_CFG_NSCAN;
                    if(!(cfg.sub[i]->misc&SUB_SSDEF))
                        scancfg&=~SUB_CFG_SSCAN; 
				} else	/* Default to scan OFF for unknown sub */
					scancfg&=~(SUB_CFG_NSCAN|SUB_CFG_SSCAN);
                write(file,&scancfg,sizeof(scancfg)); 
			}
            lseek(file,((long)cfg.sub[subnum]->ptridx)*10L,SEEK_SET);
            write(file,&l,sizeof(l));	/* date set to null */
            write(file,&l,sizeof(l));	/* date set to null */
            scancfg=0xff;
            if(!(cfg.sub[subnum]->misc&SUB_NSDEF))
                scancfg&=~SUB_CFG_NSCAN;
            if(!(cfg.sub[subnum]->misc&SUB_SSDEF))
                scancfg&=~SUB_CFG_SSCAN;
            write(file,&scancfg,sizeof(scancfg));
            close(file); 
		}
    }
	globfree(&g);
    uifc.pop(0);
}

void msgs_cfg()
{
	static int dflt,msgs_dflt,bar;
	char	str[256],str2[256],done=0;
	char*	p;
	char*	tp;
    char	tmp[128];
	char	tmp_code[32];
	int		j,k,q,s;
	int		i,file,ptridx,n;
	unsigned u;
	unsigned total_subs;
	long	ported;
	sub_t	tmpsub;
	static grp_t savgrp;
	FILE*	stream;

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
	uifc.helpbuf=
		"`Message Groups:`\n"
		"\n"
		"This is a listing of message groups for your BBS. Message groups are\n"
		"used to logically separate your message `sub-boards` into groups. Every\n"
		"sub-board belongs to a message group. You must have at least one message\n"
		"group and one sub-board configured.\n"
		"\n"
		"One popular use for message groups is to separate local sub-boards and\n"
		"networked sub-boards. One might have a `Local` message group that contains\n"
		"non-networked sub-boards of various topics and also have a `FidoNet`\n"
		"message group that contains sub-boards that are echoed across FidoNet.\n"
		"Some sysops separate sub-boards into more specific areas such as `Main`,\n"
		"`Technical`, or `Adult`. If you have many sub-boards that have a common\n"
		"subject denominator, you may want to have a separate message group for\n"
		"those sub-boards for a more organized message structure.\n"
	;
	i=uifc.list(j,0,0,45,&msgs_dflt,&bar,"Message Groups",opt);
	if(i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
		   continue;
		if(!j) {
			write_msgs_cfg(&cfg,backup_level);
            refresh_cfg(&cfg);
        }
		return;
    }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		uifc.helpbuf=
			"`Group Long Name:`\n"
			"\n"
			"This is a description of the message group which is displayed when a\n"
			"user of the system uses the `/*` command from the main menu.\n"
		;
		strcpy(str,"Main");
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Group Long Name",str,LEN_GLNAME
			,K_EDIT)<1)
			continue;
		uifc.helpbuf=
			"`Group Short Name:`\n"
			"\n"
			"This is a short description of the message group which is used for the\n"
			"main menu and reading message prompts.\n"
		;
		sprintf(str2,"%.*s",LEN_GSNAME,str);
		if(uifc.input(WIN_MID,0,0,"Group Short Name",str2,LEN_GSNAME,K_EDIT)<1)
			continue;
		if((cfg.grp=(grp_t **)realloc(cfg.grp,sizeof(grp_t *)*(cfg.total_grps+1)))==NULL) {
            errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_grps+1);
			cfg.total_grps=0;
			bail(1);
            continue; 
		}

		if(cfg.total_grps) {	/* was cfg.total_subs (?) */
            for(j=cfg.total_grps;j>i;j--)   /* insert above */
                cfg.grp[j]=cfg.grp[j-1];
            for(j=0;j<cfg.total_subs;j++)   /* move sub group numbers */
                if(cfg.sub[j]->grp>=i)
                    cfg.sub[j]->grp++; 
		}

		if((cfg.grp[i]=(grp_t *)malloc(sizeof(grp_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(grp_t));
			continue; 
		}
		memset((grp_t *)cfg.grp[i],0,sizeof(grp_t));
		strcpy(cfg.grp[i]->lname,str);
		strcpy(cfg.grp[i]->sname,str2);
		cfg.total_grps++;
		uifc.changes=1;
		continue; 
	}
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		uifc.helpbuf=
			"`Delete All Data in Group:`\n"
			"\n"
			"If you wish to delete the messages in all the sub-boards in this group,\n"
			"select `Yes`.\n"
		;
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
					sprintf(str,"%s%s.s*"
						,cfg.grp[cfg.sub[j]->grp]->code_prefix
						,cfg.sub[j]->code_suffix);
					strlwr(str);
					if(!cfg.sub[j]->data_dir[0])
						sprintf(tmp,"%ssubs/",cfg.data_dir);
					else
						strcpy(tmp,cfg.sub[j]->data_dir);
					delfiles(tmp,str);
					clearptrs(j); 
				}
		free(cfg.grp[i]);
		for(j=0;j<cfg.total_subs;) {
			if(cfg.sub[j]->grp==i) {	/* delete subs of this group */
				free(cfg.sub[j]);
				cfg.total_subs--;
				k=j;
				while(k<cfg.total_subs) {	/* move all subs down */
					cfg.sub[k]=cfg.sub[k+1];
					for(q=0;q<cfg.total_qhubs;q++)
						for(s=0;s<cfg.qhub[q]->subs;s++)
							if(cfg.qhub[q]->sub[s]==k)
								cfg.qhub[q]->sub[s]--;
					k++; 
				} 
			}
			else j++; 
		}
		for(j=0;j<cfg.total_subs;j++)	/* move sub group numbers down */
			if(cfg.sub[j]->grp>i)
				cfg.sub[j]->grp--;
		cfg.total_grps--;
		while(i<cfg.total_grps) {
			cfg.grp[i]=cfg.grp[i+1];
			i++; 
		}
		uifc.changes=1;
		continue; 
	}
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savgrp=*cfg.grp[i];
		continue; 
	}
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.grp[i]=savgrp;
		uifc.changes=1;
		continue; 
	}
	done=0;
	while(!done) {
		j=0;
		sprintf(opt[j++],"%-27.27s%s","Long Name",cfg.grp[i]->lname);
		sprintf(opt[j++],"%-27.27s%s","Short Name",cfg.grp[i]->sname);
		sprintf(opt[j++],"%-27.27s%s","Internal Code Prefix",cfg.grp[i]->code_prefix);
		sprintf(opt[j++],"%-27.27s%.40s","Access Requirements"
			,cfg.grp[i]->arstr);
		strcpy(opt[j++],"Clone Options");
		strcpy(opt[j++],"Export Areas...");
		strcpy(opt[j++],"Import Areas...");
		strcpy(opt[j++],"Message Sub-boards...");
		opt[j][0]=0;
		sprintf(str,"%s Group",cfg.grp[i]->sname);
		uifc.helpbuf=
			"`Message Group Configuration:`\n"
			"\n"
			"This menu allows you to configure the security requirements for access\n"
			"to this message group. You can also add, delete, and configure the\n"
			"sub-boards of this group by selecting the `Messages Sub-boards...` option.\n"
		;
		switch(uifc.list(WIN_ACT,6,4,60,&dflt,0,str,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				uifc.helpbuf=
					"`Group Long Name:`\n"
					"\n"
					"This is a description of the message group which is displayed when a\n"
					"user of the system uses the `/*` command from the main menu.\n"
				;
				strcpy(str,cfg.grp[i]->lname);	/* save incase setting to null */
				if(!uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Listings"
					,cfg.grp[i]->lname,LEN_GLNAME,K_EDIT))
					strcpy(cfg.grp[i]->lname,str);
				break;
			case 1:
				uifc.helpbuf=
					"`Group Short Name:`\n"
					"\n"
					"This is a short description of the message group which is used for\n"
					"main menu and reading messages prompts.\n"
				;
				uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Prompts"
					,cfg.grp[i]->sname,LEN_GSNAME,K_EDIT);
				break;
			case 2:
				uifc.helpbuf=
					"`Internal Code Prefix:`\n"
					"\n"
					"This is an `optional` code prefix used to help generate unique internal\n"
					"codes for the sub-boards in this message group. If this option\n"
					"is used, sub-board internal codes will be constructed from this prefix\n"
					"and the specified code suffix for each sub-board.\n"
				;
				uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code Prefix"
					,cfg.grp[i]->code_prefix,LEN_CODE,K_EDIT|K_UPPER);
				break;
			case 3:
				sprintf(str,"%s Group",cfg.grp[i]->sname);
				getar(str,cfg.grp[i]->arstr);
				break;
			case 4: 	/* Clone Options */
				j=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.helpbuf=
					"`Clone Sub-board Options:`\n"
					"\n"
					"If you want to clone the options of the first sub-board of this group\n"
					"into all sub-boards of this group, select `Yes`.\n"
					"\n"
					"The options cloned are posting requirements, reading requirements,\n"
					"operator requirments, moderated user requirments, toggle options,\n"
					"network options (including EchoMail origin line, EchoMail address,\n"
					"and QWK Network tagline), maximum number of messages, maximum number\n"
					"of CRCs, maximum age of messages, storage method, and data directory.\n"
				;
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
								strcpy(cfg.sub[j]->post_sem,cfg.sub[k]->post_sem);
								cfg.sub[j]->maxmsgs=cfg.sub[k]->maxmsgs;
								cfg.sub[j]->maxcrcs=cfg.sub[k]->maxcrcs;
								cfg.sub[j]->maxage=cfg.sub[k]->maxage;

								cfg.sub[j]->faddr=cfg.sub[k]->faddr; 
							} 
						} 
				}
				break;
			case 5:
				k=0;
				ported=0;
				q=uifc.changes;
				strcpy(opt[k++],"SUBS.TXT    (Synchronet)");
				strcpy(opt[k++],"AREAS.BBS   (SBBSecho)");
				strcpy(opt[k++],"FIDONET.NA  (Fido)");
				opt[k][0]=0;
				uifc.helpbuf=
					"`Export Area File Format:`\n"
					"\n"
					"This menu allows you to choose the format of the area file you wish to\n"
					"export the current message group into.\n"
				;
				k=0;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Export Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sSUBS.TXT",cfg.ctrl_dir);
				else if(k==1)
					sprintf(str,"%sAREAS.BBS",cfg.data_dir);
				else if(k==2)
					sprintf(str,"FIDONET.NA");
				if(k==1)
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"Uplinks"
						,str2,sizeof(str2)-1,0)<=0) {
						uifc.changes=q;
						break; 
					}
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename"
					,str,sizeof(str)-1,K_EDIT)<=0) {
					uifc.changes=q;
					break; 
				}
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
					else	 j=O_WRONLY|O_APPEND; 
				}
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
					if(k==1) {		/* AREAS.BBS SBBSecho */
						fprintf(stream,"%s%-30s %-20s %s\r\n"
							,cfg.grp[cfg.sub[j]->grp]->code_prefix
							,cfg.sub[j]->code_suffix
							,stou(cfg.sub[j]->sname)
							,str2);
						continue; 
					}
					if(k==2) {		/* FIDONET.NA */
						fprintf(stream,"%-20s %s\r\n"
							,stou(cfg.sub[j]->sname),cfg.sub[j]->lname);
						continue; 
					}
					fprintf(stream,"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
							"%s\r\n%s\r\n%s\r\n"
						,cfg.sub[j]->lname
						,cfg.sub[j]->sname
						,cfg.sub[j]->qwkname
						,cfg.sub[j]->code_suffix
						,cfg.sub[j]->data_dir
						,cfg.sub[j]->arstr
						,cfg.sub[j]->read_arstr
						,cfg.sub[j]->post_arstr
						,cfg.sub[j]->op_arstr
						);
					fprintf(stream,"%"PRIX32"\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n"
						,cfg.sub[j]->misc
						,cfg.sub[j]->tagline
						,cfg.sub[j]->origline
						,cfg.sub[j]->post_sem
						,cfg.sub[j]->newsgroup
						,smb_faddrtoa(&cfg.sub[j]->faddr,tmp)
						);
					fprintf(stream,"%"PRIu32"\r\n%"PRIu32"\r\n%u\r\n%u\r\n%s\r\n"
						,cfg.sub[j]->maxmsgs
						,cfg.sub[j]->maxcrcs
						,cfg.sub[j]->maxage
						,cfg.sub[j]->ptridx
						,cfg.sub[j]->mod_arstr
						);
					fprintf(stream,"***END-OF-SUB***\r\n\r\n"); 
				}
				fclose(stream);
				uifc.pop(0);
				sprintf(str,"%lu Message Areas Exported Successfully",ported);
				uifc.msg(str);
				uifc.changes=q;
				break;
			case 6:
				ported=0;
				k=0;
				strcpy(opt[k++],"SUBS.TXT    (Synchronet)");
				strcpy(opt[k++],"AREAS.BBS   (Generic)");
				strcpy(opt[k++],"AREAS.BBS   (SBBSecho)");
				strcpy(opt[k++],"FIDONET.NA  (Fido)");
				opt[k][0]=0;
				uifc.helpbuf=
					"`Import Area File Format:`\n"
					"\n"
					"This menu allows you to choose the format of the area file you wish to\n"
					"import into the current message group.\n"
				;
				k=0;
				k=uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
					,"Import Area File Format",opt);
				if(k==-1)
					break;
				if(k==0)
					sprintf(str,"%sSUBS.TXT",cfg.ctrl_dir);
				else if(k==1)
					sprintf(str,"AREAS.BBS");
				else if(k==2)
					sprintf(str,"%sAREAS.BBS",cfg.data_dir);
				else if(k==3)
					sprintf(str,"FIDONET.NA");
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Filename"
					,str,sizeof(str)-1,K_EDIT)<=0)
                    break;
				if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
					uifc.msg("Open Failure");
                    break; 
				}
				uifc.pop("Importing Areas...");
				total_subs = cfg.total_subs;	 /* Save original number of subs */
				ptridx = 0;
				while(!feof(stream)) {
					if(!fgets(str,sizeof(str),stream)) break;
					truncsp(str);
					if(!str[0])
						continue;
					if(k) {
						p=str;
						while(*p && *p<=' ') p++;
						if(!*p || *p==';')
							continue;
						memset(&tmpsub,0,sizeof(sub_t));
						tmpsub.misc|=
							(SUB_FIDO|SUB_NAME|SUB_TOUSER|SUB_QUOTE|SUB_HYPER);
						if(k==1) {		/* AREAS.BBS Generic/.MSG */
							p=str;
							SKIP_WHITESPACE(p);			/* Find path	*/
							FIND_WHITESPACE(p);			/* Skip path	*/
							SKIP_WHITESPACE(p);			/* Find tag		*/
							truncstr(p," \t");			/* Truncate tag */
							SAFECOPY(tmp_code,p);		/* Copy tag to internal code */
							SAFECOPY(tmpsub.lname,utos(p));
							SAFECOPY(tmpsub.sname,tmpsub.lname);
							SAFECOPY(tmpsub.qwkname,tmpsub.qwkname);
						}
						else if(k==2) { /* AREAS.BBS SBBSecho */
							p=str;
							SKIP_WHITESPACE(p);			/* Find internal code */
							tp=p;
							FIND_WHITESPACE(tp);
							*tp=0;						/* Truncate internal code */
							SAFECOPY(tmp_code,p);		/* Copy internal code suffix */
							p=tp+1;
							SKIP_WHITESPACE(p);			/* Find echo tag */
							truncstr(p," \t");			/* Truncate tag */
							SAFECOPY(tmpsub.lname,utos(p));
							SAFECOPY(tmpsub.sname,tmpsub.lname);
							SAFECOPY(tmpsub.qwkname,tmpsub.sname);
						}
						else if(k==3) { /* FIDONET.NA */
                            p=str;
							SKIP_WHITESPACE(p);			/* Find echo tag */
							tp=p;
							FIND_WHITESPACE(tp);		/* Find end of tag */
							*tp=0;						/* Truncate echo tag */
							SAFECOPY(tmp_code,p);		/* Copy tag to internal code suffix */
							SAFECOPY(tmpsub.sname,utos(p));	/* ... to short name, converting underscores to spaces */
							SAFECOPY(tmpsub.qwkname,tmpsub.sname);	/* ... to QWK name .... */
							p=tp+1;
							SKIP_WHITESPACE(p);			/* Find description */
							SAFECOPY(tmpsub.lname,p);	/* Copy description to long name */
						}
					}
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
						SAFECOPY(tmp_code,str);
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
						sprintf(tmpsub.post_sem,"%.*s",LEN_DIR,str);
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

						while(!feof(stream)
							&& strcmp(str,"***END-OF-SUB***")) {
							if(!fgets(str,128,stream)) break;
							truncsp(str); 
						} 
					}

                    SAFECOPY(tmpsub.code_suffix, prep_code(tmp_code,cfg.grp[i]->code_prefix));
					truncsp(tmpsub.sname);
					truncsp(tmpsub.lname);
					truncsp(tmpsub.qwkname);

					if(tmpsub.code_suffix[0]==0
						|| tmpsub.sname[0]==0
						|| tmpsub.lname[0]==0
						|| tmpsub.qwkname[0]==0)
						continue;

					for(u=0;u<total_subs;u++) {
						if(cfg.sub[u]->grp!=i)
							continue;
						if(!stricmp(cfg.sub[u]->code_suffix,tmpsub.code_suffix))
							break; 
					}
					j=u;
					if(j==total_subs) {
						j=cfg.total_subs;
						if((cfg.sub=(sub_t **)realloc(cfg.sub
							,sizeof(sub_t *)*(cfg.total_subs+1)))==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_subs+1);
							cfg.total_subs=0;
							bail(1);
							break; 
						}

						if((cfg.sub[j]=(sub_t *)malloc(sizeof(sub_t)))
							==NULL) {
							errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(sub_t));
							break; 
						}
						memset(cfg.sub[j],0,sizeof(sub_t)); 
					}
					if(!k) {
						n=cfg.sub[j]->ptridx;	/* save original ptridx */
						memcpy(cfg.sub[j],&tmpsub,sizeof(sub_t));
						cfg.sub[j]->ptridx=n;	/* restore original ptridx */
					} else {
                        cfg.sub[j]->grp=i;
						if(cfg.total_faddrs)
							cfg.sub[j]->faddr=cfg.faddr[0];
						strcpy(cfg.sub[j]->code_suffix,tmpsub.code_suffix);
						strcpy(cfg.sub[j]->sname,tmpsub.sname);
						strcpy(cfg.sub[j]->lname,tmpsub.lname);
						strcpy(cfg.sub[j]->qwkname,tmpsub.qwkname);
						strcpy(cfg.sub[j]->data_dir,tmpsub.data_dir);
						if(j==cfg.total_subs)
							cfg.sub[j]->maxmsgs=1000;
					}
					if(j==cfg.total_subs) {	/* adding new sub-board */
						for(;ptridx<USHRT_MAX;ptridx++) {
							for(u=0;u<total_subs;u++)
								if(cfg.sub[u]->ptridx==ptridx)
									break;
							if(u==total_subs)
								break; 
						}
						cfg.sub[j]->ptridx=ptridx;	/* use new ptridx */
						cfg.sub[j]->misc=tmpsub.misc;
						cfg.total_subs++; 
						ptridx++;	/* don't use the same ptridx for next sub */
					}
					uifc.changes=1; 
					ported++;
				}
				fclose(stream);
				uifc.pop(0);
				sprintf(str,"%lu Message Areas Imported Successfully",ported);
                uifc.msg(str);
				break;

			case 7:
                sub_cfg(i);
				break; 
			} 
		} 
	}
}

void msg_opts()
{
	char str[128];
	static int msg_dflt;
	int i,j,n;

	while(1) {
		i=0;
		sprintf(opt[i++],"%-33.33s%s"
			,"BBS ID for QWK Packets",cfg.sys_id);
		sprintf(opt[i++],"%-33.33s%u seconds"
			,"Maximum Retry Time",cfg.smb_retry_time);
		if(cfg.max_qwkmsgs)
			sprintf(str,"%"PRIu32,cfg.max_qwkmsgs);
		else
			sprintf(str,"Unlimited");
		sprintf(opt[i++],"%-33.33s%s"
			,"Maximum QWK Messages",str);
		if(cfg.max_qwkmsgage)
			sprintf(str,"%u days",cfg.max_qwkmsgage);
		else
			sprintf(str,"Unlimited");
		sprintf(opt[i++],"%-33.33s%s"
			,"Maximum QWK Message Age",str);
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
			sprintf(str,"Enabled (%"PRIu32" mail CRCs)",cfg.mail_maxcrcs);
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
		sprintf(opt[i++],"%-33.33s%s","Receive E-mail by Real Name"
			,cfg.msg_misc&MM_REALNAME ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Include Signatures in E-mail"
			,cfg.msg_misc&MM_EMAILSIG ? "Yes" : "No");
		sprintf(opt[i++],"%-33.33s%s","Users Can View Deleted Messages"
			,cfg.sys_misc&SM_USRVDELM ? "Yes" : cfg.sys_misc&SM_SYSVDELM
				? "Sysops Only":"No");
		sprintf(opt[i++],"%-33.33s%hu","Days of New Messages for Guest", cfg.guest_msgscan_init);
		strcpy(opt[i++],"Extra Attribute Codes...");
		opt[i][0]=0;
		uifc.helpbuf=
			"`Message Options:`\n"
			"\n"
			"This is a menu of system-wide message related options. Messages include\n"
			"E-mail and public posts (on sub-boards).\n"
		;

		switch(uifc.list(WIN_ORG|WIN_ACT|WIN_MID|WIN_CHE,0,0,72,&msg_dflt,0
			,"Message Options",opt)) {
			case -1:
				i=save_changes(WIN_MID);
				if(i==-1)
				   continue;
				if(!i) {
					cfg.new_install=new_install;
					write_msgs_cfg(&cfg,backup_level);
					write_main_cfg(&cfg,backup_level);
                    refresh_cfg(&cfg);
                }
				return;
			case 0:
				strcpy(str,cfg.sys_id);
				uifc.helpbuf=
					"`BBS ID for QWK Packets:`\n"
					"\n"
					"This is a short system ID for your BBS that is used for QWK packets.\n"
					"It should be an abbreviation of your BBS name or other related string.\n"
					"This ID will be used for your outgoing and incoming QWK packets. If\n"
					"you plan on networking via QWK packets with another Synchronet BBS,\n"
					"this ID should not begin with a number. The maximum length of the ID\n"
					"is eight characters and cannot contain spaces or other invalid DOS\n"
					"filename characters. In a QWK packet network, each system must have\n"
					"a unique QWK system ID.\n"
				;

				uifc.input(WIN_MID|WIN_SAV,0,0,"BBS ID for QWK Packets"
					,str,LEN_QWKID,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(cfg.sys_id,str);
				else
					uifc.msg("Invalid ID");
				break;
			case 1:
				uifc.helpbuf=
					"`Maximum Message Base Retry Time:`\n"
					"\n"
					"This is the maximum number of seconds to allow while attempting to open\n"
					"or lock a message base (a value in the range of 10 to 45 seconds should\n"
					"be fine).\n"
				;
				ultoa(cfg.smb_retry_time,str,10);
				uifc.input(WIN_MID|WIN_SAV,0,0
					,"Maximum Message Base Retry Time (in seconds)"
					,str,2,K_NUMBER|K_EDIT);
				cfg.smb_retry_time=atoi(str);
				break;
			case 2:
				uifc.helpbuf=
					"`Maximum Messages Per QWK Packet:`\n"
					"\n"
					"This is the maximum number of messages (excluding E-mail), that a user\n"
					"can have in one QWK packet for download. This limit does not effect\n"
					"QWK network nodes (`Q` restriction). If set to `0`, no limit is imposed.\n"
				;

				ultoa(cfg.max_qwkmsgs,str,10);
				uifc.input(WIN_MID|WIN_SAV,0,0
					,"Maximum Messages Per QWK Packet (0=No Limit)"
					,str,6,K_NUMBER|K_EDIT);
				cfg.max_qwkmsgs=atol(str);
                break;
			case 3:
				uifc.helpbuf=
					"`Maximum Age of Messages Imported From QWK Packets:`\n"
					"\n"
					"This is the maximum age of messages (in days), allowed for messages in\n"
					"QWK packets. Messages with an age older than this value will not be\n"
					"imported. If set to `0`, no age limit is imposed.\n"
				;

				itoa(cfg.max_qwkmsgage,str,10);
				uifc.input(WIN_MID|WIN_SAV,0,0
					,"Maximum Age (in days) of QWK-imported Messages (0=No Limit)"
					,str,4,K_NUMBER|K_EDIT);
				cfg.max_qwkmsgage=atoi(str);
                break;
			case 4:
				uifc.helpbuf=
					"`Pre-pack QWK Requirements:`\n"
					"\n"
					"ALL user accounts on the BBS meeting this requirmenet will have a QWK\n"
					"packet automatically created for them every day after midnight\n"
					"(during the internal daily event).\n"
					"\n"
					"This is mainly intended for QWK network nodes that wish to save connect\n"
					"time by having their packets pre-packed. If a large number of users meet\n"
					"this requirement, it can take up a large amount of disk space on your\n"
					"system (in the `DATA\\FILE` directory).\n"
				;
				getar("Pre-pack QWK (Use with caution!)",cfg.preqwk_arstr);
				break;
			case 5:
				sprintf(str,"%u",cfg.mail_maxage);
                uifc.helpbuf=
	                "`Maximum Age of Mail:`\n"
	                "\n"
	                "This value is the maximum number of days that mail will be kept.\n"
                ;
                uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Age of Mail "
                    "(in days)",str,5,K_EDIT|K_NUMBER);
                cfg.mail_maxage=atoi(str);
                break;
			case 6:
				strcpy(opt[0],"Daily");
				strcpy(opt[1],"Immediately");
				opt[2][0]=0;
				i=cfg.sys_misc&SM_DELEMAIL ? 0:1;
				uifc.helpbuf=
					"`Purge Deleted E-mail:`\n"
					"\n"
					"If you wish to have deleted e-mail physically (and permanently) removed\n"
					"from your e-mail database immediately after a users exits the reading\n"
					"e-mail prompt, set this option to `Immediately`.\n"
					"\n"
					"For the best system performance and to avoid delays when deleting e-mail\n"
					"from a large e-mail database, set this option to `Daily` (the default).\n"
					"Your system maintenance will automatically purge deleted e-mail once a\n"
					"day.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Purge Deleted E-mail",opt);
				if(!i && cfg.sys_misc&SM_DELEMAIL) {
					cfg.sys_misc&=~SM_DELEMAIL;
					uifc.changes=1; 
				}
				else if(i==1 && !(cfg.sys_misc&SM_DELEMAIL)) {
					cfg.sys_misc|=SM_DELEMAIL;
					uifc.changes=1; 
				}
                break;
			case 7:
				sprintf(str,"%"PRIu32,cfg.mail_maxcrcs);
                uifc.helpbuf=
	                "`Maximum Number of Mail CRCs:`\n"
	                "\n"
	                "This value is the maximum number of CRCs that will be kept for duplicate\n"
	                "mail checking. Once this maximum number of CRCs is reached, the oldest\n"
	                "CRCs will be automatically purged.\n"
                ;
                uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Number of Mail "
                    "CRCs",str,5,K_EDIT|K_NUMBER);
                cfg.mail_maxcrcs=atol(str);
                break;
			case 8:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=cfg.sys_misc&SM_ANON_EM ? 0:1;
				uifc.helpbuf=
					"`Allow Anonymous E-mail:`\n"
					"\n"
					"If you want users with the `A` exemption to be able to send E-mail\n"
					"anonymously, set this option to `Yes`.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Anonymous E-mail",opt);
				if(!i && !(cfg.sys_misc&SM_ANON_EM)) {
					cfg.sys_misc|=SM_ANON_EM;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&SM_ANON_EM) {
					cfg.sys_misc&=~SM_ANON_EM;
					uifc.changes=1; 
				}
				break;
			case 9:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=cfg.sys_misc&SM_QUOTE_EM ? 0:1;
				uifc.helpbuf=
					"`Allow Quoting in E-mail:`\n"
					"\n"
					"If you want your users to be allowed to use message quoting when\n"
					"responding in E-mail, set this option to `Yes`.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Quoting in E-mail",opt);
				if(!i && !(cfg.sys_misc&SM_QUOTE_EM)) {
					cfg.sys_misc|=SM_QUOTE_EM;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&SM_QUOTE_EM) {
					cfg.sys_misc&=~SM_QUOTE_EM;
					uifc.changes=1; 
				}
				break;
			case 10:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=cfg.sys_misc&SM_FILE_EM ? 0:1;
				uifc.helpbuf=
					"`Allow File Attachment Uploads in E-mail:`\n"
					"\n"
					"If you want your users to be allowed to attach an uploaded file to\n"
					"an E-mail message, set this option to `Yes`.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow File Attachment Uploads in E-mail",opt);
				if(!i && !(cfg.sys_misc&SM_FILE_EM)) {
					cfg.sys_misc|=SM_FILE_EM;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&SM_FILE_EM) {
					cfg.sys_misc&=~SM_FILE_EM;
					uifc.changes=1; 
				}
				break;
			case 11:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=cfg.sys_misc&SM_FWDTONET ? 0:1;
				uifc.helpbuf=
					"`Allow Users to Have Their E-mail Forwarded to NetMail:`\n"
					"\n"
					"If you want your users to be able to have any e-mail sent to them\n"
					"optionally (at the sender's discretion) forwarded to a NetMail address,\n"
					"set this option to `Yes`.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Allow Forwarding of E-mail to NetMail",opt);
				if(!i && !(cfg.sys_misc&SM_FWDTONET)) {
					cfg.sys_misc|=SM_FWDTONET;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&SM_FWDTONET) {
					cfg.sys_misc&=~SM_FWDTONET;
					uifc.changes=1; 
				}
                break;
			case 12:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=cfg.sys_misc&SM_DELREADM ? 0:1;
				uifc.helpbuf=
					"`Kill Read E-mail Automatically:`\n"
					"\n"
					"If this option is set to `Yes`, e-mail that has been read will be\n"
					"automatically deleted when message base maintenance is run.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Kill Read E-mail Automatically",opt);
				if(!i && !(cfg.sys_misc&SM_DELREADM)) {
					cfg.sys_misc|=SM_DELREADM;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&SM_DELREADM) {
					cfg.sys_misc&=~SM_DELREADM;
					uifc.changes=1; 
				}
                break;
			case 13:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=cfg.msg_misc&MM_REALNAME ? 0:1;
				uifc.helpbuf=
					"`Receive E-mail by Real Name:`\n"
					"\n"
					"If this option is set to ~Yes~, e-mail messages may be received when\n"
					"addressed to a user's real name (rather than their alias).\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Receive E-mail by Real Name",opt);
				if(!i && !(cfg.msg_misc&MM_REALNAME)) {
					cfg.msg_misc|=MM_REALNAME;
					uifc.changes=1; 
				}
				else if(i==1 && cfg.msg_misc&MM_REALNAME) {
					cfg.msg_misc&=~MM_REALNAME;
					uifc.changes=1; 
				}
                break;
			case 14:
				n=(cfg.sub[i]->misc&MM_EMAILSIG) ? 0:1;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.helpbuf=
					"`Include User Signatures in E-mail:`\n"
					"\n"
					"If you wish to have user signatures automatically appended to e-mail\n"
					"messages, set this option to ~Yes~.\n"
				;
				n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
					,"Include User Signatures in E-mail",opt);
				if(n==-1)
                    break;
				if(!n && !(cfg.msg_misc&MM_EMAILSIG)) {
					uifc.changes=1;
					cfg.msg_misc|=MM_EMAILSIG;
					break; 
				}
				if(n==1 && cfg.msg_misc&MM_EMAILSIG) {
					uifc.changes=1;
					cfg.msg_misc&=~MM_EMAILSIG; 
				}
                break;
			case 15:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				strcpy(opt[2],"Sysops Only");
				opt[3][0]=0;
				i=1;
				uifc.helpbuf=
					"`Users Can View Deleted Messages:`\n"
					"\n"
					"If this option is set to `Yes`, then users will be able to view messages\n"
					"they've sent and deleted or messages sent to them and they've deleted\n"
					"with the option of un-deleting the message before the message is\n"
					"physically purged from the e-mail database.\n"
					"\n"
					"If this option is set to `No`, then when a message is deleted, it is no\n"
					"longer viewable (with SBBS) by anyone.\n"
					"\n"
					"If this option is set to `Sysops Only`, then only sysops and sub-ops (when\n"
					"appropriate) can view deleted messages.\n"
				;

				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Users Can View Deleted Messages",opt);
				if(!i && (cfg.sys_misc&(SM_USRVDELM|SM_SYSVDELM))
					!=(SM_USRVDELM|SM_SYSVDELM)) {
					cfg.sys_misc|=(SM_USRVDELM|SM_SYSVDELM);
					uifc.changes=1; 
				}
				else if(i==1 && cfg.sys_misc&(SM_USRVDELM|SM_SYSVDELM)) {
					cfg.sys_misc&=~(SM_USRVDELM|SM_SYSVDELM);
					uifc.changes=1; 
				}
				else if(i==2 && (cfg.sys_misc&(SM_USRVDELM|SM_SYSVDELM))
					!=SM_SYSVDELM) {
					cfg.sys_misc|=SM_SYSVDELM;
					cfg.sys_misc&=~SM_USRVDELM;
					uifc.changes=1; 
				}
                break;
			case 16:
				uifc.helpbuf=
					"`Days of New Messages for Guest:`\n"
					"\n"
					"This option allows you to set the number of days worth of messages\n"
					"which will be included in a Guest login's `new message scan`.\n"
					"\n"
					"The value `0` means there will be `no` new messages for the Guest account.\n"
				;
				sprintf(str,"%hu",cfg.guest_msgscan_init);
				uifc.input(WIN_SAV|WIN_MID,0,0
					,"Days of New Messages for Guest's new message scan"
					,str,4,K_EDIT|K_NUMBER);
				cfg.guest_msgscan_init=atoi(str);
                break;
			case 17:
				uifc.helpbuf=
					"`Extra Attribute Codes...`\n"
					"\n"
					"Synchronet can suppport the native text attribute codes of other BBS\n"
					"programs in messages (menus, posts, e-mail, etc.) To enable the extra\n"
					"attribute codes for another BBS program, set the corresponding option\n"
					"to `Yes`.\n"
				;

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
							break; 
				} 
			} 
		} 
	}
}
