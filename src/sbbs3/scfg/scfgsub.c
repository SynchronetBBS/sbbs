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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfg.h"

static sub_t** cut_qhub_sub;

bool new_sub(unsigned new_subnum, unsigned group_num, sub_t* pasted_sub, long misc)
{
	sub_t* new_subboard;
	if ((new_subboard = (sub_t *)malloc(sizeof(*new_subboard))) == NULL) {
		errormsg(WHERE, ERR_ALLOC, "sub-board", sizeof(*new_subboard));
		return false;
	}
	memset(new_subboard, 0, sizeof(*new_subboard));
	if (cfg.total_faddrs)
		new_subboard->faddr = cfg.faddr[0];
	/* ToDo: Define these defaults somewhere else: */
	new_subboard->misc = (SUB_NSDEF | SUB_SSDEF | SUB_QUOTE | SUB_TOUSER | SUB_FAST);
	new_subboard->misc |= misc;
	if(new_subboard->misc&(SUB_FIDO | SUB_INET))
		new_subboard->misc |= SUB_NOVOTING;
	new_subboard->pmode = P_MARKUP | P_HIDEMARKS | P_NOATCODES;
	new_subboard->maxmsgs = 500;

	/* Use last sub in group (if exists) as a template for new subs */
	for (unsigned u = 0; u < cfg.total_subs; u++) {
		if(cfg.sub[u]->grp == group_num) {
			*new_subboard = *cfg.sub[u];
			new_subboard->misc &= ~SUB_TEMPLATE;
			if(cfg.sub[u]->misc & SUB_TEMPLATE)	/* Use this sub (not last) if marked as template */
				break;
		}
	}
	new_subboard->misc |= SUB_HDRMOD;
	if (pasted_sub != NULL) {
		*new_subboard = *pasted_sub;
		if (cut_qhub_sub != NULL && (*cut_qhub_sub) == NULL) {
			*cut_qhub_sub = new_subboard;
			cut_qhub_sub = NULL;
		}
	}
	new_subboard->grp = group_num;

	/* Allocate a new (unused) pointer index (deprecated!) */
	for (; new_subboard->ptridx < USHRT_MAX; new_subboard->ptridx++) {
		int n;
		for (n = 0; n < cfg.total_subs; n++)
			if (cfg.sub[n]->ptridx == new_subboard->ptridx)
				break;
		if (n == cfg.total_subs)
			break;
	}

	sub_t **new_sub_list;
	if ((new_sub_list = (sub_t **)realloc(cfg.sub, sizeof(sub_t *)*(cfg.total_subs + 1))) == NULL) {
		errormsg(WHERE, ERR_ALLOC, "sub list", cfg.total_subs + 1);
		free(new_subboard);
		return false;
	}
	cfg.sub = new_sub_list;

	/* Move higher numbered subs (for inserting) */
	for (unsigned u = cfg.total_subs; u > new_subnum; u--)
		cfg.sub[u] = cfg.sub[u - 1];


	new_subboard->subnum = new_subnum;
	cfg.sub[new_subnum] = new_subboard;
	cfg.total_subs++;

	return true;
}

void remove_sub(scfg_t* cfg, unsigned subnum, bool cut)
{
	sub_t* sub = cfg->sub[subnum];
	if(cut)
		cut_qhub_sub = NULL;
	// Remove the sub-board from any QWKnet hub sub-boards
	for (unsigned q = 0; q < cfg->total_qhubs; q++) {
		for (unsigned s = 0; s < cfg->qhub[q]->subs; s++) {
			if (cfg->qhub[q]->sub[s] == sub) {
				if(cut && cut_qhub_sub == NULL)
					cut_qhub_sub = &cfg->qhub[q]->sub[s];
				cfg->qhub[q]->sub[s] = NULL;
			}
		}
	}
	FREE_AND_NULL(cfg->sub[subnum]);
	--cfg->total_subs;
	for (unsigned i = subnum; i < cfg->total_subs; i++)
		cfg->sub[i] = cfg->sub[i + 1];
}

void sub_cfg(uint grpnum)
{
	static int dflt,tog_dflt,tog_bar,opt_dflt,net_dflt,adv_dflt,bar;
	char str[128],str2[128],done=0,code[128];
	char path[MAX_PATH+1];
	char data_dir[MAX_PATH+1];
	int j,m,n;
	uint i,subnum[MAX_OPTS+1];
	static sub_t savsub;

	char* sub_long_name_help = 
		"`Sub-board Long Name:`\n"
		"\n"
		"This is a description of the message sub-board which is displayed in all\n"
		"sub-board (message area) listings.\n"
		"\n"
		"Sub-board Long Names are limited to 40 characters in length.\n"
		"\n"
		"If don't have any good ideas for a Long Name or description and just\n"
		"want to use a short name (less than or equal to 25 characters in length)\n"
		"for both the Sub-board Long Name and Short Name, there is no harm in"
		"doing that."
		;
	char* sub_short_name_help =
		"`Sub-board Short Name:`\n"
		"\n"
		"This is a unique name to assign the message sub-board. This name will be\n"
		"displayed in prompts and other areas where space may be limited or where\n"
		"a full message area description (Long Name) is unnecessary.\n"
		"\n"
		"Sub-board Short Names are limited to 25 characters in length.\n"
		;
	char* sub_code_help =
		"`Sub-board Internal Code Suffix:`\n"
		"\n"
		"Every sub-board must have its own unique code for Synchronet to refer\n"
		"to it internally. This code should be descriptive of the sub-board's\n"
		"topic, usually an abbreviation of the sub-board's name.\n"
		"\n"
		"`Note:` The Internal Code (displayed) is the complete internal code\n"
		"constructed from the message group's `Code Prefix` and the sub-board's\n"
		"`Code Suffix`.\n"
		"\n"
		"Changing a sub-board's internal code (suffix or prefix) changes the\n"
		"underlying database file filenames used for that sub-board, so change\n"
		"these values with caution."
		;

	while(1) {
		if(uifc.changes && cfg.grp[grpnum]->sort)
			sort_subs(grpnum);
		int maxlen = 0;
		bool template_shown = false;
		for(i=0,j=0;i<cfg.total_subs && j<MAX_OPTS;i++)
			if(cfg.sub[i]->grp==grpnum) {
				subnum[j]=i;
				int len = 0;
				opt[j][0] = 0;
				if(cfg.sub[subnum[0]]->qwkconf)
					len += sprintf(opt[j], "%-5u ", cfg.sub[i]->qwkconf);
				char* name = cfg.sub[i]->lname;
				int name_len = LEN_SLNAME;
				switch(cfg.grp[grpnum]->sort) {
					case AREA_SORT_SNAME:
						name = cfg.sub[i]->sname;
						name_len = LEN_SSNAME;
						break;
					case AREA_SORT_CODE:
						name = cfg.sub[i]->code_suffix;
						name_len = LEN_CODE;
						break;
					default:	/* Defeat stupid GCC warning */
						break;
				}
				sprintf(str, "%-*s %c", name_len, name, (cfg.sub[i]->misc&SUB_TEMPLATE && !template_shown) ? '*' : ' ');
				if(cfg.sub[i]->misc&SUB_TEMPLATE)
					template_shown = true;
				truncsp(str);
				len += sprintf(opt[j] + strlen(opt[j]), "%s", str);
				if(len > maxlen)
					maxlen = len;
				j++; 
			}
		subnum[j]=cfg.total_subs;
		opt[j][0]=0;
		sprintf(str,"%s Sub-boards (%u)",cfg.grp[grpnum]->sname, j);
		int mode = WIN_SAV|WIN_ACT|WIN_RHT;
		if(j)
			mode |= WIN_DEL|WIN_COPY|WIN_CUT|WIN_DELACT;
		if(j<MAX_OPTS)
			mode |= WIN_INS|WIN_XTR|WIN_PASTEXTR|WIN_INSACT;
		if(savsub.sname[0])
			mode |= WIN_PASTE;
		uifc.helpbuf=
			"`Message Sub-boards:`\n"
			"\n"
			"This is a list of message sub-boards that have been configured for the\n"
			"selected message group.\n"
			"\n"
			"The `template` sub-board, if one exists, will be denoted with an asterisk.\n"
			"Normally, only one template sub-board should exist per message group.\n"
			"\n"
			"To add a sub-board, select the desired position with the arrow keys and\n"
			"hit ~ INS ~.\n"
			"\n"
			"To delete a sub-board, select it with the arrow keys and hit ~ DEL ~.\n"
			"\n"
			"To configure a sub-board, select it with the arrow keys and hit ~ ENTER ~.\n"
		;
		i = uifc.list(mode, 0, 0, maxlen+5, &dflt, &bar, str, opt);
		if((signed)i==-1)
			return;
		int msk = i & MSK_ON;
		i &= MSK_OFF;
		if((int)i > j)
			continue;
		if(msk==MSK_INS) {
			strcpy(str,"General");
			uifc.helpbuf=sub_long_name_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board Long Name",str,LEN_SLNAME
				,K_EDIT)<1)
				continue;
			sprintf(str2,"%.*s",LEN_SSNAME,str);
			uifc.helpbuf=sub_short_name_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board Short Name",str2,LEN_SSNAME
				,K_EDIT)<1)
				continue;
	#if 0
			sprintf(str3,"%.10s",str2);
			uifc.helpbuf=
				"`Sub-board QWK Name:`\n"
				"\n"
				"This is the name of the sub-board used for QWK off-line readers.\n"
			;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board QWK Name",str3,10
				,K_EDIT)<1)
				continue;
	#endif
			SAFECOPY(code,str2);
			prep_code(code,/* prefix: */NULL);
			uifc.helpbuf=sub_code_help;
			if(uifc.input(WIN_MID|WIN_SAV,0,0,"Sub-board Internal Code Suffix",code,LEN_CODE
				,K_EDIT|K_UPPER)<1)
				continue;
			if(!code_ok(code)) {
				uifc.helpbuf=invalid_code;
				uifc.msg("Invalid Code");
				uifc.helpbuf=0;
				continue; 
			}

			if (!new_sub(subnum[i], grpnum, /* pasted_sub: */NULL, /* misc: */0))
				continue;

			SAFECOPY(cfg.sub[subnum[i]]->code_suffix,code);
			SAFECOPY(cfg.sub[subnum[i]]->lname,str);
			SAFECOPY(cfg.sub[subnum[i]]->sname,str2);
			SAFECOPY(cfg.sub[subnum[i]]->qwkname,code);
			if(strchr(str,'.') && strchr(str,' ')==NULL)
				SAFECOPY(cfg.sub[subnum[i]]->newsgroup,str);
			uifc.changes = TRUE;
			continue; 
		}
		if(msk == MSK_DEL || msk == MSK_CUT) {
			if (msk == MSK_DEL) {
				uifc.helpbuf=
					"`Delete Data in Sub-board:`\n"
					"\n"
					"If you want to delete all the messages for this sub-board, select `Yes`.\n"
				;
				j=1;
				SAFEPRINTF2(str,"%s%s.*"
					,cfg.grp[cfg.sub[subnum[i]]->grp]->code_prefix
					,cfg.sub[subnum[i]]->code_suffix);
				strlwr(str);
				if(!cfg.sub[subnum[i]]->data_dir[0])
					SAFEPRINTF(data_dir,"%ssubs/",cfg.data_dir);
				else
					SAFECOPY(data_dir,cfg.sub[subnum[i]]->data_dir);

				SAFEPRINTF2(path,"%s%s",data_dir,str);
				if(fexist(path)) {
					SAFEPRINTF(str2,"Delete %s",path);
					j=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
						,str2,uifcYesNoOpts);
					if(j==-1)
						continue;
					if(j==0)
						delfiles(data_dir,str, /* keep: */0);
				}
			}
			if(msk == MSK_CUT)
				savsub = *cfg.sub[subnum[i]];
			remove_sub(&cfg, subnum[i], msk == MSK_CUT);
			uifc.changes = TRUE;
			continue; 
		}
		if(msk==MSK_COPY) {
			savsub=*cfg.sub[subnum[i]];
			cut_qhub_sub = NULL;
			continue; 
		}
		if(msk == MSK_PASTE) {
			if (!new_sub(subnum[i], grpnum, &savsub, /* misc: */0))
				continue;
			uifc.changes = TRUE;
			continue; 
		}
		i=subnum[i];
		j=0;
		done=0;
		while(!done) {
			n=0;
			sprintf(opt[n++],"%-27.27s%s","Long Name",cfg.sub[i]->lname);
			sprintf(opt[n++],"%-27.27s%s","Short Name",cfg.sub[i]->sname);
			sprintf(opt[n++],"%-27.27s%s","QWK Name",cfg.sub[i]->qwkname);
			sprintf(opt[n++],"%-27.27s%s%s","Internal Code"
				,cfg.grp[cfg.sub[i]->grp]->code_prefix, cfg.sub[i]->code_suffix);
			sprintf(opt[n++],"%-27.27s%s","FidoNet Area Tag", cfg.sub[i]->area_tag);
			sprintf(opt[n++],"%-27.27s%s","Newsgroup Name",cfg.sub[i]->newsgroup);
			sprintf(opt[n++],"%-27.27s%s","Access Requirements"
				,cfg.sub[i]->arstr);
			sprintf(opt[n++],"%-27.27s%s","Reading Requirements"
				,cfg.sub[i]->read_arstr);
			sprintf(opt[n++],"%-27.27s%s","Posting Requirements"
				,cfg.sub[i]->post_arstr);
			sprintf(opt[n++],"%-27.27s%s","Operator Requirements"
				,cfg.sub[i]->op_arstr);
			sprintf(opt[n++],"%-27.27s%s","Moderated Posting User"
				,cfg.sub[i]->mod_arstr);
			if(cfg.sub[i]->maxmsgs)
				sprintf(str, "%"PRIu32, cfg.sub[i]->maxmsgs);
			else
				strcpy(str, "Unlimited");
			sprintf(opt[n++],"%-27.27s%s","Maximum Messages", str);
			if(cfg.sub[i]->maxage)
				sprintf(str,"Enabled (%u days old)",cfg.sub[i]->maxage);
			else
				strcpy(str,"Disabled");
			sprintf(opt[n++],"%-27.27s%s","Purge by Age",str);
			if(cfg.sub[i]->maxcrcs)
				sprintf(str,"Enabled (%"PRIu32" message CRCs)",cfg.sub[i]->maxcrcs);
			else
				strcpy(str,"Disabled");
			sprintf(opt[n++],"%-27.27s%s","Duplicate Checking",str);

			strcpy(opt[n++],"Toggle Options...");
			strcpy(opt[n++],"Network Options...");
			strcpy(opt[n++],"Advanced Options...");
			opt[n][0]=0;
			sprintf(str,"%s Sub-board", cfg.sub[i]->sname);
			uifc.helpbuf=
				"`Sub-board Configuration:`\n"
				"\n"
				"This menu allows you to configure the individual selected sub-board.\n"
				"Options with a trailing `...` provide a sub-menu of more options.\n"
			;
			switch(uifc.list(WIN_ACT|WIN_SAV|WIN_BOT|WIN_L2R
				,0,0,60,&opt_dflt,0,str,opt)) {
				case -1:
					done=1;
					break;
				case 0:
					uifc.helpbuf=sub_long_name_help;
					SAFECOPY(str, cfg.sub[i]->lname);
					if(uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Listings"
						,str,LEN_SLNAME,K_EDIT) > 0)
						SAFECOPY(cfg.sub[i]->lname, str);
					break;
				case 1:
					uifc.helpbuf=sub_short_name_help;
					SAFECOPY(str, cfg.sub[i]->sname);
					if(uifc.input(WIN_MID|WIN_SAV,0,17,"Name to use for Prompts"
						,str,LEN_SSNAME,K_EDIT) > 0)
						SAFECOPY(cfg.sub[i]->sname, str);
					break;
				case 2:
					uifc.helpbuf=
						"`Sub-board QWK Conference Name:`\n"
						"\n"
						"This is the name of the sub-board to be used in the CONTROL.DAT file\n"
						"included in QWK packets for display by QWK off-line readers.\n"
						"\n"
						"For QWK Extended (QWKE) packets, the included Conference Names will be\n"
						"automatically generated from each message group short name and each\n"
						"sub-board's long name.\n"
						"\n"
						"QWK Conference Names are not used in QWK networks."
					;
					uifc.input(WIN_MID|WIN_SAV,0,17,"QWK Conference Name"
						,cfg.sub[i]->qwkname, sizeof(cfg.sub[i]->qwkname)-1, K_EDIT);
					break;
				case 3:
					uifc.helpbuf=sub_code_help;
					SAFECOPY(str,cfg.sub[i]->code_suffix);
					uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code Suffix (unique)"
						,str,LEN_CODE,K_EDIT|K_UPPER);
					if(code_ok(str))
						SAFECOPY(cfg.sub[i]->code_suffix,str);
					else {
						uifc.helpbuf=invalid_code;
						uifc.msg("Invalid Code");
						uifc.helpbuf=0; 
					}
					break;
				case 4:
					uifc.helpbuf=
						"`FidoNet Area Tag:`\n"
						"\n"
						"This field may be used to specify the FidoNet-style `Echo/Area Tag` for\n"
						"this message area. If no tag name is configured here, a tag name will be\n"
						"automatically generated from the Sub-board's `Short Name`.\n"
						"\n"
						"This tag should ~ not ~ contain spaces."
					;
					uifc.input(WIN_MID|WIN_SAV, 0, 17, "FidoNet Area Tag"
						,cfg.sub[i]->area_tag, sizeof(cfg.sub[i]->area_tag)-1, K_EDIT|K_UPPER);
					break;
				case 5:
					uifc.helpbuf=
						"`Newsgroup Name:`\n"
						"\n"
						"This is the name of the sub-board used for newsgroup readers. If no name\n"
						"is configured here, a name will be automatically generated from the\n"
						"Sub-board's Short Name and message group's Short Name.\n"
						"\n"
						"This name should ~ not ~ contain spaces."
					;
					uifc.input(WIN_MID|WIN_SAV,0,17,""
						,cfg.sub[i]->newsgroup,sizeof(cfg.sub[i]->newsgroup)-1,K_EDIT);
					break;
				case 6:
					sprintf(str,"%s Access",cfg.sub[i]->sname);
					getar(str,cfg.sub[i]->arstr);
					break;
				case 7:
					sprintf(str,"%s Reading",cfg.sub[i]->sname);
					getar(str,cfg.sub[i]->read_arstr);
					break;
				case 8:
					sprintf(str,"%s Posting",cfg.sub[i]->sname);
					getar(str,cfg.sub[i]->post_arstr);
					break;
				case 9:
					sprintf(str,"%s Operator",cfg.sub[i]->sname);
					getar(str,cfg.sub[i]->op_arstr);
					break;
				case 10:
					sprintf(str,"%s Moderated Posting User",cfg.sub[i]->sname);
					getar(str,cfg.sub[i]->mod_arstr);
					break;
				case 11:
					sprintf(str,"%"PRIu32,cfg.sub[i]->maxmsgs);
					uifc.helpbuf=
						"`Maximum Number of Messages:`\n"
						"\n"
						"This value is the maximum number of messages that will be kept in the\n"
						"sub-board. It is possible for newly-posted or imported messages to\n"
						"exceed this maximum (it is `not` an immediately imposed limit).\n"
						"\n"
						"Older messages that exceed this maximum count are purged using `smbutil`,\n"
						"typically run as a timed event (e.g. `MSGMAINT`).\n"
						"\n"
						"A value of `0` means no maximum number of stored messages will be\n"
						"imposed during message-base maintenance."
					;
					uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Number of Messages (0=Unlimited)"
						,str,9,K_EDIT|K_NUMBER);
					cfg.sub[i]->maxmsgs=atoi(str);
					cfg.sub[i]->misc|=SUB_HDRMOD;
					break;
				case 12:
					sprintf(str,"%u",cfg.sub[i]->maxage);
					uifc.helpbuf=
						"`Maximum Age of Messages:`\n"
						"\n"
						"This value is the maximum number of days that messages will be kept in\n"
						"the sub-board.\n"
						"\n"
						"Message age is calculated from the date and time of message import/post\n"
						"and not necessarily the date/time the message was originally written.\n"
						"\n"
						"Old messages are purged using `smbutil`, typically run as a timed\n"
						"event (e.g. `MSGMAINT`).\n"
						"\n"
						"A value of `0` means no maximum age of stored messages will be\n"
						"imposed during message-base maintenance."
					;
					uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Age of Messages (in days)"
						,str,5,K_EDIT|K_NUMBER);
					cfg.sub[i]->maxage=atoi(str);
					cfg.sub[i]->misc|=SUB_HDRMOD;
					break;
				case 13:
					sprintf(str,"%"PRIu32,cfg.sub[i]->maxcrcs);
					uifc.helpbuf=
						"`Maximum Number of CRCs:`\n"
						"\n"
						"This value is the maximum number of CRCs that will be kept in the\n"
						"sub-board for duplicate message checking. Once this maximum number of\n"
						"CRCs is reached, the oldest CRCs will be automatically purged.\n"
						"\n"
						"A value of `0` means no CRCs (or other hashes) of message body text\n"
						"or meta-data will be saved (i.e. for purposes of duplicate message\n"
						"detection and rejection)."
					;
					uifc.input(WIN_MID|WIN_SAV,0,17,"Maximum Number of CRCs"
						,str,9,K_EDIT|K_NUMBER);
					cfg.sub[i]->maxcrcs=atol(str);
					cfg.sub[i]->misc|=SUB_HDRMOD;
					break;
				case 14:
					while(1) {
						n=0;
						sprintf(opt[n++],"%-30.30s%s","Allow Private Posts"
							,cfg.sub[i]->misc&SUB_PRIV ? cfg.sub[i]->misc&SUB_PONLY
							? "Only":"Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Allow Anonymous Posts"
							,cfg.sub[i]->misc&SUB_ANON ? cfg.sub[i]->misc&SUB_AONLY
							? "Only":"Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Post Using Real Names"
							,cfg.sub[i]->misc&SUB_NAME ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Users Can Edit Posts"
							,cfg.sub[i]->misc&SUB_EDIT ? cfg.sub[i]->misc&SUB_EDITLAST 
							? "Last" : "Yes" : "No");
						sprintf(opt[n++],"%-30.30s%s","Users Can Delete Posts"
							,cfg.sub[i]->misc&SUB_DEL ? cfg.sub[i]->misc&SUB_DELLAST
							? "Last" : "Yes" : "No");
						sprintf(opt[n++],"%-30.30s%s","Default On for New Scan"
							,cfg.sub[i]->misc&SUB_NSDEF ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Forced On for New Scan"
							,cfg.sub[i]->misc&SUB_FORCED ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Default On for Your Scan"
							,cfg.sub[i]->misc&SUB_SSDEF ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Public 'To' User"
							,cfg.sub[i]->misc&SUB_TOUSER ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Allow Message Voting"
							,cfg.sub[i]->misc&SUB_NOVOTING ? "No":"Yes");
						sprintf(opt[n++],"%-30.30s%s","Allow Message Quoting"
							,cfg.sub[i]->misc&SUB_QUOTE ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Allow Message Tagging"
							,cfg.sub[i]->misc&SUB_MSGTAGS ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Suppress User Signatures"
							,cfg.sub[i]->misc&SUB_NOUSERSIG ? "Yes":"No");
						sprintf(opt[n++],"%-30.30s%s","Permanent Operator Msgs"
							,cfg.sub[i]->misc&SUB_SYSPERM ? "Yes":"No");
	#if 0 /* this is not actually implemented (yet?) */
						sprintf(opt[n++],"%-30.30s%s","Kill Read Messages"
							,cfg.sub[i]->misc&SUB_KILL ? "Yes"
							: (cfg.sub[i]->misc&SUB_KILLP ? "Pvt" : "No"));
	#endif
						sprintf(opt[n++],"%-30.30s%s","Compress Messages (LZH)"
							,cfg.sub[i]->misc&SUB_LZH ? "Yes" : "No");
						sprintf(opt[n++],"%-30.30s%s","Apply Markup Codes"
							,cfg.sub[i]->pmode&P_MARKUP ? ((cfg.sub[i]->pmode&P_HIDEMARKS)  ? "Hide" : "Yes") : "No");
						sprintf(opt[n++],"%-30.30s%s","Extra Attribute Codes"
							,cfg.sub[i]->pmode&P_NOXATTRS ? "No" : "Yes");
						sprintf(opt[n++],"%-30.30s%s","Word-wrap Messages"
							,cfg.sub[i]->n_pmode&P_WORDWRAP ? "No" : "Yes");
						sprintf(opt[n++],"%-30.30s%s","Auto-detect UTF-8 Msgs"
							,cfg.sub[i]->pmode&P_AUTO_UTF8 ? "Yes" : "No");
						sprintf(opt[n++],"%-30.30s%s","Expand @-codes in Sysop Msgs"
							,cfg.sub[i]->pmode&P_NOATCODES ? "No" : "Yes");
						sprintf(opt[n++],"%-30.30s%s","Template for New Subs"
							,cfg.sub[i]->misc&SUB_TEMPLATE ? "Yes" : "No");

						opt[n][0]=0;
						uifc.helpbuf=
							"`Sub-board Toggle Options:`\n"
							"\n"
							"This menu allows you to toggle certain options for the selected\n"
							"sub-board between two or more settings, such as `Yes` and `No`.\n"
						;
						n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,1,0,&tog_dflt, &tog_bar
							,"Toggle Options",opt);
						if(n==-1)
							break;
						switch(n) {
							case __COUNTER__:
								if(cfg.sub[i]->misc&SUB_PONLY)
									n=2;
								else 
									n=(cfg.sub[i]->misc&SUB_PRIV) ? 0:1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								strcpy(opt[2],"Only");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Allow Private Posts on Sub-board:`\n"
									"\n"
									"If you want users to be able to post private messages to other users\n"
									"on this sub-board, set this value to `Yes`. Usually, E-mail is the\n"
									"preferred method of private communication. If you want users to be able\n"
									"to post private messages only on this sub-board, select `Only`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Allow Private Posts",opt);
								if(n==-1)
									break;
								if(!n && (cfg.sub[i]->misc&(SUB_PRIV|SUB_PONLY))
									!=SUB_PRIV) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_PONLY;
									cfg.sub[i]->misc|=SUB_PRIV;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_PRIV) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_PRIV;
									break; 
								}
								if(n==2 && (cfg.sub[i]->misc&(SUB_PRIV|SUB_PONLY))
									!=(SUB_PRIV|SUB_PONLY)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=(SUB_PRIV|SUB_PONLY); 
								}
								break;
							case __COUNTER__:
								if(cfg.sub[i]->misc&SUB_AONLY)
									n=2;
								else 
									n=(cfg.sub[i]->misc&SUB_ANON) ? 0:1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								strcpy(opt[2],"Only");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Allow Anonymous Posts on Sub-board:`\n"
									"\n"
									"If you want users with the `A` exemption to be able to post anonymously on\n"
									"this sub-board, select `Yes`. If you want all posts on this sub-board to be\n"
									"forced anonymous, select `Only`. If you do not want anonymous posts allowed\n"
									"on this sub-board at all, select `No`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Allow Anonymous Posts",opt);
								if(n==-1)
									break;
								if(!n && (cfg.sub[i]->misc&(SUB_ANON|SUB_AONLY))
									!=SUB_ANON) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_AONLY;
									cfg.sub[i]->misc|=SUB_ANON;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&(SUB_ANON|SUB_AONLY)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~(SUB_ANON|SUB_AONLY);
									break; 
								}
								if(n==2 && (cfg.sub[i]->misc&(SUB_ANON|SUB_AONLY))
									!=(SUB_ANON|SUB_AONLY)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=(SUB_ANON|SUB_AONLY); 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_NAME) ? 0:1;
								uifc.helpbuf=
									"`User Real Names in Posts on Sub-board:`\n"
									"\n"
									"If you allow aliases on your system, you can have messages on this\n"
									"sub-board automatically use the real name of the posting user by setting\n"
									"this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Use Real Names in Posts",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_NAME)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_NAME;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_NAME) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_NAME; 
								}
								break;
							case __COUNTER__:
								if(cfg.sub[i]->misc&SUB_EDITLAST)
									n=2;
								else
									n=(cfg.sub[i]->misc&SUB_EDIT) ? 0:1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								strcpy(opt[2],"Last Post Only");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Users Can Edit Posts on Sub-board:`\n"
									"\n"
									"If you wish to allow users to edit their messages after they have been\n"
									"posted, this option to `Yes`. If you wish to allow users to edit only the\n"
									"last message on a message base, set this option to `Last Post Only`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Users Can Edit Messages",opt);
								if(n==-1)
									break;
								if(n==0 /* yes */
									&& (cfg.sub[i]->misc&(SUB_EDIT|SUB_EDITLAST))
									!=SUB_EDIT
									) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_EDIT;
									cfg.sub[i]->misc&=~SUB_EDITLAST;
									break; 
								}
								if(n==1 /* no */
									&& cfg.sub[i]->misc&(SUB_EDIT|SUB_EDITLAST)
									) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~(SUB_EDIT|SUB_EDITLAST);
									break;
								}
								if(n==2 /* last only */
									&& (cfg.sub[i]->misc&(SUB_EDIT|SUB_EDITLAST))
									!=(SUB_EDIT|SUB_EDITLAST)
									) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=(SUB_EDIT|SUB_EDITLAST);
									break;
								}
								break;
							case __COUNTER__:
								if(cfg.sub[i]->misc&SUB_DELLAST)
									n=2;
								else 
									n=(cfg.sub[i]->misc&SUB_DEL) ? 0:1;

								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								strcpy(opt[2],"Last Post Only");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Users Can Delete Posts on Sub-board:`\n"
									"\n"
									"If you want users to be able to delete any of their own posts on this\n"
									"sub-board, set this option to `Yes`. If you want to allow users the\n"
									"ability to delete their message only if it is the last message on the\n"
									"sub-board, select `Last Post Only`. If you want to disallow users from\n"
									"deleting any of their posts, set this option to `No`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Users Can Delete Posts",opt);
								if(n==-1)
									break;
								if(!n && (cfg.sub[i]->misc&(SUB_DEL|SUB_DELLAST))
									!=SUB_DEL) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_DELLAST;
									cfg.sub[i]->misc|=SUB_DEL;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_DEL) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_DEL;
									break; 
								}
								if(n==2 && (cfg.sub[i]->misc&(SUB_DEL|SUB_DELLAST))
									!=(SUB_DEL|SUB_DELLAST)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=(SUB_DEL|SUB_DELLAST); 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_NSDEF) ? 0:1;
								uifc.helpbuf=
									"`Default On for New Scan:`\n"
									"\n"
									"If you want this sub-board to be included in all user new message scans\n"
									"by default, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Default On for New Scan",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_NSDEF)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_NSDEF;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_NSDEF) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_NSDEF; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_FORCED) ? 0:1;
								uifc.helpbuf=
									"`Forced On for New Scan:`\n"
									"\n"
									"If you want this sub-board to be included in all user new message scans\n"
									"even if the user has removed it from their new scan configuration, set\n"
									"this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Forced New Scan",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_FORCED)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_FORCED;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_FORCED) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_FORCED; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_SSDEF) ? 0:1;
								uifc.helpbuf=
									"`Default On for Your Scan:`\n"
									"\n"
									"If you want this sub-board to be included in all user personal message\n"
									"scans by default, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Default On for Your Scan",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_SSDEF)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_SSDEF;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_SSDEF) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_SSDEF; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_TOUSER) ? 0:1;
								uifc.helpbuf=
									"`Prompt for 'To' User on Public Posts:`\n"
									"\n"
									"If you want all posts on this sub-board to be prompted for a 'To' user,\n"
									"set this option to `Yes`. This is a useful option for sub-boards that\n"
									"are on a network that does not allow private posts.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Prompt for 'To' User on Public Posts",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_TOUSER)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_TOUSER;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_TOUSER) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_TOUSER; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_NOVOTING) ? 1:0;
								uifc.helpbuf=
									"`Allow Message Voting:`\n"
									"\n"
									"If you want users to be allowed to Up-Vote or Down-Vote messages on this\n"
									"sub-board, set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Allow Message Voting",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && (cfg.sub[i]->misc&SUB_NOVOTING)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc ^= SUB_NOVOTING;
									break; 
								}
								if(n==1 && !(cfg.sub[i]->misc&SUB_NOVOTING)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc ^= SUB_NOVOTING; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_QUOTE) ? 0:1;
								uifc.helpbuf=
									"`Allow Message Quoting:`\n"
									"\n"
									"If you want users to be allowed to quote messages on this sub-board, set\n"
									"this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Allow Message Quoting",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_QUOTE)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_QUOTE;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_QUOTE) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_QUOTE; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_MSGTAGS) ? 0:1;
								uifc.helpbuf=
									"`Allow Message Tagging:`\n"
									"\n"
									"If you want users to be allowed to add tags to messages on this sub-board, \n"
									"set this option to `Yes` (not to be confused with 'tag-lines').\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Allow Message Tagging",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_MSGTAGS)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_MSGTAGS;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_MSGTAGS) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_MSGTAGS; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_NOUSERSIG) ? 0:1;
								uifc.helpbuf=
									"Suppress User Signatures:\n"
									"\n"
									"If you do not wish to have user signatures automatically appended to\n"
									"messages posted in this sub-board, set this option to Yes.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Suppress User Signatures",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_NOUSERSIG)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_NOUSERSIG;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_NOUSERSIG) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_NOUSERSIG; 
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_SYSPERM) ? 0:1;
								uifc.helpbuf=
									"`Operator Messages Automatically Permanent:`\n"
									"\n"
									"If you want messages posted by `System` and `Sub-board Operators` to be\n"
									"automatically permanent (non-purgeable) for this sub-board, set this\n"
									"option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Permanent Operator Messages",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_SYSPERM)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_SYSPERM;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_SYSPERM) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_SYSPERM; 
								}
								break;
	#if 0 /* This is not actually implemented (yet?) */
							case 12:
								if(cfg.sub[i]->misc&SUB_KILLP)
									n=2;
								else
									n=(cfg.sub[i]->misc&SUB_KILL) ? 0:1;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"No");
								strcpy(opt[2],"Private");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Kill Read Messages Automatically:`\n"
									"\n"
									"If you want messages that have been read by the intended recipient to\n"
									"be automatically deleted by `SMBUTIL`, set this option to `Yes` or\n"
									"`Private` if you want only private messages to be automatically deleted.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Kill Read Messages",opt);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_KILL)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_KILL;
									cfg.sub[i]->misc&=~SUB_KILLP;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&(SUB_KILL|SUB_KILLP)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~(SUB_KILL|SUB_KILLP); 
								}
								if(n==2 && !(cfg.sub[i]->misc&SUB_KILLP)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_KILLP;
									cfg.sub[i]->misc&=~SUB_KILL;
									break; 
								}
								break;
	#endif
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_LZH) ? 0:1;
								uifc.helpbuf=
									"`Compress Messages with LZH Encoding:`\n"
									"\n"
									"If you want all messages in this sub-board to be automatically\n"
									"compressed via `LZH` (Lempel/Ziv/Huffman algorithm used in LHarc, LHA,\n"
									"and other popular compression and archive programs), this option to `Yes`.\n"
									"\n"
									"Compression will slow down the reading and writing of messages slightly,\n"
									"but the storage space saved can be as much as `50 percent`.\n"
									"\n"
									"Before setting this option to `Yes`, make sure that all of the SMB\n"
									"compatible mail programs you use support the `LZH` translation.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Compress Messages (LZH)",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_LZH)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_LZH;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_LZH) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_LZH; 
								}
								break;
							case __COUNTER__:
								n = (cfg.sub[i]->pmode&P_MARKUP) ? (cfg.sub[i]->pmode&P_HIDEMARKS ? 1 : 0) : 2;
								uifc.helpbuf=
									"`Interpret/Display Markup Codes in Messages:`\n"
									"\n"
									"Markup codes are called 'StyleCodes' in GoldEd, 'Rich Text' in SemPoint,\n"
									"and 'Structured Text' in Mozilla/Thunderbird.\n"
									"\n"
									"`*Bold Text*`\n"
									"/Italic Text/\n"
									"~#Inverse Text#~\n"
									"_Underlined Text_\n"
									"\n"
									"Markup character cannot be combined.\n"
								;
								strcpy(opt[0],"Yes");
								strcpy(opt[1],"Yes and Hide the Markup Characters");
								strcpy(opt[2],"No");
								opt[3][0]=0;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Interpret/Display Markup Codes in Messages", opt);
								if(n==-1)
									break;
								if(n == 0 && (cfg.sub[i]->pmode&(P_MARKUP|P_HIDEMARKS)) != P_MARKUP) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode |= P_MARKUP;
									cfg.sub[i]->pmode &= ~P_HIDEMARKS;
								}
								else if(n == 1 && (cfg.sub[i]->pmode&(P_MARKUP|P_HIDEMARKS)) != (P_MARKUP|P_HIDEMARKS)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode |= (P_MARKUP|P_HIDEMARKS);
								}
								else if(n == 2 && (cfg.sub[i]->pmode&(P_MARKUP|P_HIDEMARKS)) != 0) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode &= ~(P_MARKUP|P_HIDEMARKS);
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->pmode&P_NOXATTRS) ? 1:0;
								uifc.helpbuf=
									"`Extra Attribute Codes:`\n"
									"\n"
									"Set this option to `No` to disable the interpretation of so-called\n"
									"Extra Attribute Codes (printable color codes for other BBS software\n"
									"e.g. pipe codes).\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Interpret/Display Extra Attribute Codes in Messages",uifcYesNoOpts);
								if(n==-1)
									break;
								if(n == 0 && (cfg.sub[i]->pmode&P_NOXATTRS)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode ^= P_NOXATTRS;
								}
								else if(n == 1 && !(cfg.sub[i]->pmode&P_NOXATTRS)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode ^= P_NOXATTRS;
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->n_pmode&P_WORDWRAP) ? 1:0;
								uifc.helpbuf=
									"`Word-wrap Message Text:`\n"
									"\n"
									"Set this option to `No` to disable the automatic reflowing (word-wrapping)\n"
									"of the lines of text of messages in this sub-board when viewed from the\n"
									"Terminal Server."
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Word-wrap Message Text",uifcYesNoOpts);
								if(n==-1)
									break;
								if(n == 0 && (cfg.sub[i]->n_pmode&P_WORDWRAP)) {
									uifc.changes = TRUE;
									cfg.sub[i]->n_pmode ^= P_WORDWRAP;
								}
								else if(n == 1 && !(cfg.sub[i]->n_pmode&P_WORDWRAP)) {
									uifc.changes = TRUE;
									cfg.sub[i]->n_pmode ^= P_WORDWRAP;
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->pmode&P_AUTO_UTF8) ? 0:1;
								uifc.helpbuf=
									"`Automatically Detect UTF-8 Message Text:`\n"
									"\n"
									"Set this option to `Yes` to enable automatic detection of UTF-8 message\n"
									"text during display; no \"`charset=utf-8`\" need be specified by the sender.\n"
									"\n"
									"Note: Setting this option to `Yes` does *not* enable automatic detection\n"
									"of UTF-8 header field values (e.g. To, From, Subject)."
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Automatically Detect UTF-8 Message Text",uifcYesNoOpts);
								if(n==-1)
									break;
								if(n == 0 && !(cfg.sub[i]->pmode&P_AUTO_UTF8)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode ^= P_AUTO_UTF8;
								}
								else if(n == 1 && (cfg.sub[i]->pmode&P_AUTO_UTF8)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode ^= P_AUTO_UTF8;
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->pmode&P_NOATCODES) ? 1:0;
								uifc.helpbuf=
									"`Expand @-codes in Sysop-Posted Messages:`\n"
									"\n"
									"Set this option to `Yes` to enable the interpretation and expansion of\n"
									"message variables (@-codes) in messages posted by the local sysop\n"
									"(user #1).\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Expand @-codes in Sysop-Posted Messages",uifcYesNoOpts);
								if(n==-1)
									break;
								if(n == 0 && (cfg.sub[i]->pmode & P_NOATCODES)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode ^= P_NOATCODES;
								}
								else if(n == 1 && !(cfg.sub[i]->pmode & P_NOATCODES)) {
									uifc.changes = TRUE;
									cfg.sub[i]->pmode ^= P_NOATCODES;
								}
								break;
							case __COUNTER__:
								n=(cfg.sub[i]->misc&SUB_TEMPLATE) ? 0:1;
								uifc.helpbuf=
									"`Use this Sub-board as a Template for New Subs:`\n"
									"\n"
									"If you want this sub-board's options / settings to be used as the\n"
									"template for newly-created or cloned sub-boards in this message group,\n"
									"set this option to `Yes`.\n"
									"\n"
									"If multiple sub-boards have this option enabled, only the first will be\n"
									"used as the template."
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Use this Sub-board as a Template for New Subs",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_TEMPLATE)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_TEMPLATE;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_TEMPLATE) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_TEMPLATE; 
								}
								break;
							} 
						}
					break;
				case 15:
					while(1) {
						n=0;
						sprintf(opt[n++],"%-27.27s%s","Append Tag/Origin Line"
							,cfg.sub[i]->misc&SUB_NOTAG ? "No":"Yes");
						sprintf(opt[n++],"%-27.27s%s","Export ASCII Only"
							,cfg.sub[i]->misc&SUB_ASCII ? "Yes":"No");
						sprintf(opt[n++],"%-27.27s%s","Gate Between Net Types"
							,cfg.sub[i]->misc&SUB_GATE ? "Yes":"No");
						sprintf(opt[n++],"%-27.27s%s","QWK Networked"
							,cfg.sub[i]->misc&SUB_QNET ? "Yes":"No");
						sprintf(opt[n++],"QWK Tagline");
						sprintf(opt[n++],"%-27.27s%s","Internet (UUCP/NNTP)"
							,cfg.sub[i]->misc&SUB_INET ? "Yes":"No");
						sprintf(opt[n++],"%-27.27s%s","FidoNet EchoMail"
							,cfg.sub[i]->misc&SUB_FIDO ? "Yes":"No");
						sprintf(opt[n++],"%-27.27s%s","FidoNet Address"
							,smb_faddrtoa(&cfg.sub[i]->faddr,tmp));
						sprintf(opt[n++],"EchoMail Origin Line");
						opt[n][0]=0;
						uifc.helpbuf=
							"`Sub-board Network Options:`\n"
							"\n"
							"This menu contains options for the selected sub-board that pertain\n"
							"specifically to message networking.\n"
						;
						n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,60,&net_dflt,0
							,"Network Options",opt);
						if(n==-1)
							break;
						switch(n) {
							case 0:
								n=0;
								uifc.helpbuf=
									"`Append Tag/Origin Line to Posts:`\n"
									"\n"
									"If you want to disable the automatic addition of a network tagline or\n"
									"origin line to the bottom of outgoing networked posts from this\n"
									"sub-board, set this option to `No`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Append Tag/Origin Line to Posts",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && cfg.sub[i]->misc&SUB_NOTAG) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_NOTAG;
									break; 
								}
								if(n==1 && !(cfg.sub[i]->misc&SUB_NOTAG)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_NOTAG; 
								}
								break;
							case 1:
								n=0;
								uifc.helpbuf=
									"`Export ASCII Characters Only:`\n"
									"\n"
									"If the network that this sub-board is echoed on does not allow extended\n"
									"ASCII (>127) or control codes (<20, not including CR), set this option\n"
									"to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Export ASCII Characters Only",uifcYesNoOpts);
								if(n==-1)
									break;
								if(n && cfg.sub[i]->misc&SUB_ASCII) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_ASCII;
									break; 
								}
								if(!n && !(cfg.sub[i]->misc&SUB_ASCII)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_ASCII; 
								}
								break;
							case 2:
								n=1;
								uifc.helpbuf=
									"`Gate Between Net Types:`\n"
									"\n"
									"If this sub-board is networked using more than one network technology,\n"
									"and you want messages to be `gated` between the networks, set this\n"
									"option to `Yes`.\n"
									"\n"
									"If this option is set to `No`, messages imported from one network type\n"
									"will `not` be exported to another network type. This is the default and\n"
									"should be used unless you have `specific` permission from both networks\n"
									"to gate this sub-board. Incorrectly gated sub-boards can cause duplicate\n"
									"messages loops and cross-posted messages.\n"
									"\n"
									"This option does not affect the exporting of messages created on your\n"
									"BBS.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Gate Between Net Types",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_GATE)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_GATE;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_GATE) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_GATE; 
								}
								break;
							case 3:
								n=1;
								uifc.helpbuf=
									"`Sub-board Networked via QWK Packets:`\n"
									"\n"
									"If this sub-board is networked with other BBSs via QWK packets, this\n"
									"option should be set to `Yes`. With this option set to `Yes`, titles of\n"
									"posts on this sub-board will be limited to the QWK packet limitation of\n"
									"25 characters. It also allows the `N`etwork restriction to function\n"
									"properly.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Networked via QWK Packets",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_QNET)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_QNET;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_QNET) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_QNET; 
								}
								break;
							case 4:
								uifc.helpbuf=
									"`Sub-board QWK Network Tagline:`\n"
									"\n"
									"If you want to use a different QWK tagline than the configured default\n"
									"tagline in the `Networks` configuration, you should enter that tagline\n"
									"here. If this option is left blank, the default tagline is used.\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,0,"",cfg.sub[i]->tagline
									,sizeof(cfg.sub[i]->tagline)-1,K_MSG|K_EDIT);
								break;
							case 5:
								n=1;
								uifc.helpbuf=
									"`Sub-board Networked via Internet:`\n"
									"\n"
									"If this sub-board is networked to the Internet via UUCP or NNTP, this\n"
									"option should be set to `Yes`.\n"
									"\n"
									"It will allow the `N`etwork user restriction to function properly.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Networked via Internet",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_INET)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_INET;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_INET) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_INET; 
								}
								break;
							case 6:
								n=1;
								uifc.helpbuf=
									"`Sub-board Networked via FidoNet EchoMail:`\n"
									"\n"
									"If this sub-board is networked with a FidoNet-technology Network (FTN)\n"
									"EchoMail conference (public message area) set this option to `Yes`.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Networked via FidoNet EchoMail",uifcYesNoOpts);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_FIDO)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_FIDO;
									break; 
								}
								if(n==1 && cfg.sub[i]->misc&SUB_FIDO) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~SUB_FIDO; 
								}
								break;
							case 7:
							{
								int k = 0;
								if(!cfg.total_faddrs) {
									uifc.msg("You must configure a FidoNet address in SCFG->Networks->FidoNet");
									break;
								}
								uifc.helpbuf=
									"`Sub-board FidoNet Address:`\n"
									"\n"
									"If this sub-board is part of a FidoNet EchoMail conference, this is\n"
									"the address used for this sub-board.\n"
								;
								for(n=0; n<cfg.total_faddrs && n<MAX_OPTS; n++) {
									if(memcmp(&cfg.sub[i]->faddr, &cfg.faddr[n], sizeof(cfg.faddr[n])) == 0)
										k = i;
									strcpy(opt[n], smb_faddrtoa(&cfg.faddr[n],NULL)); 
								}
								opt[n][0]=0;
								n = uifc.list(WIN_RHT|WIN_SAV|WIN_ACT|WIN_INSACT, 0, 0, 0, &k, NULL, "FidoNet Address", opt);
								if(n >= 0 && n < cfg.total_faddrs) {
									cfg.sub[i]->faddr = cfg.faddr[n];
									uifc.changes = TRUE;
								}
								break;
							}
							case 8:
								uifc.helpbuf=
									"`Sub-board FidoNet Origin Line:`\n"
									"\n"
									"If this sub-board is part of a FidoNet EchoMail conference and you\n"
									"want to use an origin line other than the default origin line in the\n"
									"`Networks` configuration, set this value to the desired origin line.\n"
									"\n"
									"If this option is blank, the default origin line is used.\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,0,"",cfg.sub[i]->origline
									,sizeof(cfg.sub[i]->origline)-1,K_EDIT);
								break;
						} 
					}
					break;
				case 16:
					while(1) {
						n=0;
						if(cfg.sub[i]->qwkconf)
							sprintf(str,"Static (%u)",cfg.sub[i]->qwkconf);
						else
							strcpy(str,"Dynamic");
						sprintf(opt[n++],"%-27.27s%s","QWK Conference Number"
							,str);
						sprintf(opt[n++],"%-27.27s%s","Storage Method"
							,cfg.sub[i]->misc&SUB_HYPER ? "Hyper Allocation"
							: cfg.sub[i]->misc&SUB_FAST ? "Fast Allocation"
							: "Self-packing");
						if(!cfg.sub[i]->data_dir[0])
							sprintf(str,"%ssubs/",cfg.data_dir);
						else
							strcpy(str,cfg.sub[i]->data_dir);
						sprintf(opt[n++],"%-27.27s%s","Storage Directory",str);
						sprintf(opt[n++],"%-27.27s%s","Semaphore File",cfg.sub[i]->post_sem);
						sprintf(opt[n++],"%-27.27s%u","Pointer File Index",cfg.sub[i]->ptridx);
						sprintf(opt[n++],"%-27.27sNow %u / Was %u","Sub-board Index", i, cfg.sub[i]->subnum);
						opt[n][0]=0;
						uifc.helpbuf=
							"`Sub-board Advanced Options:`\n"
							"\n"
							"This menu contains options for the selected sub-board that are advanced\n"
							"in nature.\n"
						;
						n=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,3,2,60,&adv_dflt,0
							,"Advanced Options",opt);
						if(n==-1)
							break;
						switch(n) {
							case 0:
								uifc.helpbuf=
									"`Sub-board QWK Conference Number:`\n"
									"\n"
									"If you wish to have the QWK conference number for this sub-board\n"
									"automatically generated by Synchronet (based on the group number\n"
									"and sub-board number for the user), set this option to `Dynamic`.\n"
									"\n"
									"If you wish to have the same QWK conference number for this sub-board\n"
									"regardless of which user access it, set this option to `Static`\n"
									"by entering the conference number you want to use.\n"
								;
								if(cfg.sub[i]->qwkconf)
									sprintf(str,"%u",cfg.sub[i]->qwkconf);
								else
									str[0]=0;
								if(uifc.input(WIN_MID|WIN_SAV,0,17
									,"QWK Conference Number (0=Dynamic)"
									,str,5,K_EDIT|K_NUMBER)>=0)
									cfg.sub[i]->qwkconf=atoi(str);
								break;
							case 1:
								n=0;
								strcpy(opt[0],"Hyper Allocation");
								strcpy(opt[1],"Fast Allocation");
								strcpy(opt[2],"Self-packing");
								opt[3][0]=0;
								uifc.helpbuf=
									"`Self-Packing` is the slowest storage method because it conserves disk\n"
									"  space as it imports messages by using deleted message header and data\n"
									"  blocks for new messages automatically. If you use this storage method,\n"
									"  you will not need to run `smbutil p` on this message base unless you\n"
									"  accumilate a large number of deleted message blocks and wish to free\n"
									"  that disk space. You can switch from self-packing to fast allocation\n"
									"  storage method and back again as you wish.\n"
									"`Fast Allocation` is faster than self-packing because it does not search\n"
									"  for deleted message blocks for new messages. It automatically places\n"
									"  all new message blocks at the end of the header and data files. If you\n"
									"  use this storage method, you will need to run `smbutil p` on this\n"
									"  message base periodically or it will continually use up disk space.\n"
									"`Hyper Allocation` is the fastest storage method because it does not\n"
									"  maintain allocation files at all. Once a message base is setup to use\n"
									"  this storage method, it should not be changed without first deleting\n"
									"  the message base data files in your `data/subs` directory for this\n"
									"  sub-board. You must use `smbutil p` as with the fast allocation method.\n"
								;
								n=uifc.list(WIN_SAV|WIN_MID,0,0,0,&n,0
									,"Storage Method",opt);
								if(n==-1)
									break;
								if(!n && !(cfg.sub[i]->misc&SUB_HYPER)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_HYPER;
									cfg.sub[i]->misc&=~SUB_FAST;
									cfg.sub[i]->misc|=SUB_HDRMOD;
									break; 
								}
								if(!n)
									break;
								if(cfg.sub[i]->misc&SUB_HYPER) {	/* Switching from hyper */
									strcpy(opt[0],"Yes");
									strcpy(opt[1],"No, I want to use Hyper Allocation");
									opt[2][0]=0;
									m=0;
									if(uifc.list(WIN_SAV|WIN_MID,0,0,0,&m,0
										,"Delete all data for this sub-board?",opt)!=0)
										break;
									if(cfg.sub[i]->data_dir[0])
										sprintf(str,"%s",cfg.sub[i]->data_dir);
									else
										sprintf(str,"%ssubs/",cfg.data_dir);
									sprintf(str2,"%s%s.*"
										,cfg.grp[cfg.sub[i]->grp]->code_prefix
										,cfg.sub[i]->code_suffix);
									strlwr(str2);
									delfiles(str,str2, /* keep: */0); 
								}

								if(cfg.sub[i]->misc&SUB_HYPER)
									cfg.sub[i]->misc|=SUB_HDRMOD;
								if(n==1 && !(cfg.sub[i]->misc&SUB_FAST)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc|=SUB_FAST;
									cfg.sub[i]->misc&=~SUB_HYPER;
									break; 
								}
								if(n==2 && cfg.sub[i]->misc&(SUB_FAST|SUB_HYPER)) {
									uifc.changes = TRUE;
									cfg.sub[i]->misc&=~(SUB_FAST|SUB_HYPER);
									break; 
								}
								break;
							case 2:
								uifc.helpbuf=
									"`Sub-board Storage Directory:`\n"
									"\n"
									"Use this if you wish to place the data directory for this sub-board on\n"
									"another drive or in another directory besides the default setting.\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,17,"Directory"
									,cfg.sub[i]->data_dir,sizeof(cfg.sub[i]->data_dir)-1,K_EDIT);
								break; 
							case 3:
								uifc.helpbuf=
									"`Sub-board Semaphore File:`\n"
									"\n"
									"This is a filename that will be created as a semaphore (signal) to an\n"
									"external program or event whenever a message is posted in this\n"
									"sub-board.\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,17,"Semaphore File"
									,cfg.sub[i]->post_sem,sizeof(cfg.sub[i]->post_sem)-1,K_EDIT);
								break; 
							case 4:
								uifc.helpbuf=
									"`Sub-board Pointer Index:`\n"
									"\n"
									"You should normally have no reason to modify this value. If you get\n"
									"crossed-up or duplicate ptridx values, then you may want to adjust\n"
									"this value, but do so with great care and trepidation.\n"
								;
								sprintf(str,"%u",cfg.sub[i]->ptridx);
								if(uifc.input(WIN_MID|WIN_SAV,0,17
									,"Pointer File Index (Danger!)"
									,str,5,K_EDIT|K_NUMBER)>=0)
									cfg.sub[i]->ptridx=atoi(str);
								break;
							case 5:
								uifc.msg("This value cannot be changed.");
								break;

						} 
					}
					break;
			} 
		} 
	}
}
