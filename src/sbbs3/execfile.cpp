/* execfile.cpp */

/* Synchronet file transfer-related command shell/module routines */

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
#include "cmdshell.h"

int sbbs_t::exec_file(csi_t *csi)
{
	char	str[256],ch;
	int		s;
	uint 	i,j,x,y;
	file_t	f;

	switch(*(csi->ip++)) {

		case CS_FILE_SELECT_AREA:
			csi->logic=LOGIC_FALSE;
			if(!usrlibs) return(0);
			while(online) {
				j=0;
				if(usrlibs>1) {
					sprintf(str,"%smenu/libs.*", cfg.text_dir);
					if(fexist(str))
						menu("libs");
					else {
						bputs(text[CfgLibLstHdr]);
						for(i=0;i<usrlibs && !msgabort();i++) {
							if(i==curlib)
								outchar('*');
							else outchar(SP);
							if(i<9) outchar(SP);
							if(i<99) outchar(SP);
							bprintf(text[CfgLibLstFmt]
								,i+1,cfg.lib[usrlib[i]]->lname); } }
					sprintf(str,text[JoinWhichLib],curlib+1);
					mnemonics(str);
					j=getnum(usrlibs);
					if((int)j==-1)
						return(0);
					if(!j)
						j=curlib;
					else
						j--; }
				sprintf(str,"%smenu/dirs%u.*", cfg.text_dir, usrlib[j]+1);
				if(fexist(str)) {
					sprintf(str,"dirs%u",usrlib[j]+1);
					menu(str); }
				else {
					CLS;
					bprintf(text[DirLstHdr], cfg.lib[usrlib[j]]->lname);
					for(i=0;i<usrdirs[j] && !msgabort();i++) {
						if(i==curdir[j]) outchar('*');
						else outchar(SP);
						sprintf(str,text[DirLstFmt],i+1
							,cfg.dir[usrdir[j][i]]->lname,nulstr
							,getfiles(&cfg,usrdir[j][i]));
						if(i<9) outchar(SP);
						if(i<99) outchar(SP);
						bputs(str); } }
				sprintf(str,text[JoinWhichDir],curdir[j]+1);
				mnemonics(str);
				i=getnum(usrdirs[j]);
				if((int)i==-1) {
					if(usrlibs==1)
						return(0);
					continue; }
				if(!i)
					i=curdir[j];
				else
					i--;
				curlib=j;
				curdir[curlib]=i;
				csi->logic=LOGIC_TRUE;
				return(0); }
			return(0);

		case CS_FILE_GET_DIR_NUM:

			if(useron.misc&COLDKEYS) {
				i=atoi(csi->str);
				if(i && i<=usrdirs[curlib] && usrlibs)
					curdir[curlib]=i-1;
				return(0); }

			ch=getkey(K_UPPER);
			outchar(ch);
			if((ch&0xf)*10U<=usrdirs[curlib] && (ch&0xf) && usrlibs) {
				i=(ch&0xf)*10;
				ch=getkey(K_UPPER);
				if(!isdigit(ch) && ch!=CR) {
					ungetkey(ch);
					curdir[curlib]=(i/10)-1;
					return(0); }
				outchar(ch);
				if(ch==CR) {
					curdir[curlib]=(i/10)-1;
					return(0); }
				logch(ch,0);
				i+=ch&0xf;
				if(i*10<=usrdirs[curlib]) { 	/* 100+ dirs */
					i*=10;
					ch=getkey(K_UPPER);
					if(!isdigit(ch) && ch!=CR) {
						ungetkey(ch);
						curdir[curlib]=(i/10)-1;
						return(0); }
					outchar(ch);
					if(ch==CR) {
						curdir[curlib]=(i/10)-1;
						return(0); }
					logch(ch,0);
					i+=ch&0xf; }
				if(i<=usrdirs[curlib])
					curdir[curlib]=i-1;
				return(0); }
			if((ch&0xf)<=(int)usrdirs[curlib] && (ch&0xf) && usrlibs)
				curdir[curlib]=(ch&0xf)-1;
			return(0);

		case CS_FILE_GET_LIB_NUM:

			if(useron.misc&COLDKEYS) {
				i=atoi(csi->str);
				if(i && i<=usrlibs)
					curlib=i-1;
				return(0); }

			ch=getkey(K_UPPER);
			outchar(ch);
			if((ch&0xf)*10U<=usrlibs && (ch&0xf)) {
				i=(ch&0xf)*10;
				ch=getkey(K_UPPER);
				if(!isdigit(ch) && ch!=CR) {
					ungetkey(ch);
					curlib=(i/10)-1;
					return(0); }
				outchar(ch);
				if(ch==CR) {
					curlib=(i/10)-1;
					return(0); }
				logch(ch,0);
				i+=ch&0xf;
				if(i<=usrlibs)
					curlib=i-1;
				return(0); }
			if((ch&0xf)<=(int)usrlibs && (ch&0xf))
				curlib=(ch&0xf)-1;
			return(0);

		case CS_FILE_SHOW_LIBRARIES:
			if(!usrlibs) return(0);
			sprintf(str,"%smenu/libs.*", cfg.text_dir);
			if(fexist(str)) {
				menu("libs");
				return(0); }
			bputs(text[LibLstHdr]);
			for(i=0;i<usrlibs && !msgabort();i++) {
				if(i==curlib)
					outchar('*');
				else outchar(SP);
				if(i<9) outchar(SP);
				bprintf(text[LibLstFmt],i+1
					,cfg.lib[usrlib[i]]->lname,nulstr,usrdirs[i]); }
			return(0);

		case CS_FILE_SHOW_DIRECTORIES:
			if(!usrlibs) return(0);
			sprintf(str,"%smenu/dirs%u.*", cfg.text_dir, usrlib[curlib]+1);
			if(fexist(str)) {
				sprintf(str,"dirs%u",usrlib[curlib]+1);
				menu(str);
				return(0); }
			CRLF;
			bprintf(text[DirLstHdr],cfg.lib[usrlib[curlib]]->lname);
			for(i=0;i<usrdirs[curlib] && !msgabort();i++) {
				if(i==curdir[curlib]) outchar('*');
				else outchar(SP);
				sprintf(str,text[DirLstFmt],i+1
					,cfg.dir[usrdir[curlib][i]]->lname,nulstr
					,getfiles(&cfg,usrdir[curlib][i]));
				if(i<9) outchar(SP);
				if(i<99) outchar(SP);
				bputs(str); }
			return(0);

		case CS_FILE_LIBRARY_UP:
			curlib++;
			if(curlib>=usrlibs)
				curlib=0;
			return(0);
		case CS_FILE_LIBRARY_DOWN:
			if(!curlib)
				curlib=usrlibs-1;
			else curlib--;
			return(0);
		case CS_FILE_DIRECTORY_UP:
			if(!usrlibs) return(0);
			curdir[curlib]++;
			if(curdir[curlib]>=usrdirs[curlib])
				curdir[curlib]=0;
			return(0);
		case CS_FILE_DIRECTORY_DOWN:
			if(!usrlibs) return(0);
			if(!curdir[curlib])
				curdir[curlib]=usrdirs[curlib]-1;
			else curdir[curlib]--;
			return(0);
		case CS_FILE_SET_AREA:
			csi->logic=LOGIC_TRUE;
			for(i=0;i<usrlibs;i++)
				for(j=0;j<usrdirs[i];j++)
					if(!stricmp(csi->str,cfg.dir[usrdir[i][j]]->code)) {
						curlib=i;
						curdir[i]=j;
						return(0); }
			csi->logic=LOGIC_FALSE;
			return(0);
		case CS_FILE_SET_LIBRARY:
			csi->logic=LOGIC_TRUE;
			for(i=0;i<usrlibs;i++)
				if(!stricmp(cfg.lib[usrlib[i]]->sname,csi->str))
					break;
			if(i<usrlibs)
				curlib=i;
			else
				csi->logic=LOGIC_FALSE;
			return(0);

		case CS_FILE_UPLOAD:
			csi->logic=LOGIC_FALSE;
			if(usrlibs) {
				i=usrdir[curlib][curdir[curlib]];
				if(cfg.upload_dir!=INVALID_DIR
					&& !chk_ar(cfg.dir[i]->ul_ar,&useron))
					i=cfg.upload_dir; 
			} else
				i=cfg.upload_dir;
			csi->logic=upload(i) ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);
		case CS_FILE_UPLOAD_USER:
			csi->logic=LOGIC_FALSE;
			if(cfg.user_dir==INVALID_DIR) {
				bputs(text[NoUserDir]);
				return(0); 
			}
			csi->logic=upload(cfg.user_dir) ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);
		case CS_FILE_UPLOAD_SYSOP:
			csi->logic=LOGIC_FALSE;
			if(cfg.sysop_dir==INVALID_DIR) {
				bputs(text[NoSysopDir]);
				return(0); 
			}
			csi->logic=upload(cfg.sysop_dir) ? LOGIC_TRUE:LOGIC_FALSE;
			return(0);
		case CS_FILE_DOWNLOAD:
			if(!usrlibs) return(0);
			if(useron.rest&FLAG('D')) {
				bputs(text[R_Download]);
				return(0); }
			padfname(csi->str,str);
			strupr(str);
			if(!listfileinfo(usrdir[curlib][curdir[curlib]],str,FI_DOWNLOAD)) {
				bputs(text[SearchingAllDirs]);
				for(i=0;i<usrdirs[curlib];i++)
					if(i!=curdir[curlib] &&
						(s=listfileinfo(usrdir[curlib][i],str,FI_DOWNLOAD))!=0)
						if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
							return(0);
				bputs(text[SearchingAllLibs]);
				for(i=0;i<usrlibs;i++) {
					if(i==curlib) continue;
					for(j=0;j<usrdirs[i];j++)
						if((s=listfileinfo(usrdir[i][j],str,FI_DOWNLOAD))!=0)
							if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
								return(0); } }
			return(0);
		case CS_FILE_DOWNLOAD_USER: /* Download from user dir */
			csi->logic=LOGIC_FALSE;
			if(cfg.user_dir==INVALID_DIR) {
				bputs(text[NoUserDir]);
				return(0); }
			if(useron.rest&FLAG('D')) {
				bputs(text[R_Download]);
				return(0); }
			CRLF;
			if(!listfileinfo(cfg.user_dir,nulstr,FI_USERXFER))
				bputs(text[NoFilesForYou]);
			else
				csi->logic=LOGIC_TRUE;
			return(0);
		case CS_FILE_DOWNLOAD_BATCH:
			if(batdn_total && yesno(text[DownloadBatchQ])) {
				start_batch_download();
				csi->logic=LOGIC_TRUE; }
			else
				csi->logic=LOGIC_FALSE;
			return(0);
		case CS_FILE_BATCH_ADD_LIST:
			batch_add_list(csi->str);
			return(0);
		case CS_FILE_BATCH_ADD:
			csi->logic=LOGIC_FALSE;
			if(!csi->str[0])
				return(0);
			padfname(csi->str,f.name);
			for(x=y=0;x<usrlibs;x++) {
				for(y=0;y<usrdirs[x];y++)
					if(findfile(&cfg,usrdir[x][y],f.name))
						break;
				if(y<usrdirs[x])
					break; }
			if(x>=usrlibs)
				return(0);
			f.dir=usrdir[x][y];
			getfileixb(&cfg,&f);
			f.size=0;
			getfiledat(&cfg,&f);
			addtobatdl(&f);
			csi->logic=LOGIC_TRUE;
			return(0);
		case CS_FILE_BATCH_CLEAR:
			if(!batdn_total) {
				csi->logic=LOGIC_FALSE;
				return(0); }
			csi->logic=LOGIC_TRUE;
			for(i=0;i<batdn_total;i++) {
				f.dir=batdn_dir[i];
				f.datoffset=batdn_offset[i];
				f.size=batdn_size[i];
				strcpy(f.name,batdn_name[i]);
				closefile(&f); }
			batdn_total=0;
			return(0);

		case CS_FILE_VIEW:
			if(!usrlibs) return(0);
			padfname(csi->str,str);
			strupr(str);
			csi->logic=LOGIC_TRUE;
			if(listfiles(usrdir[curlib][curdir[curlib]],str,0,FL_VIEW))
				return(0);
			bputs(text[SearchingAllDirs]);
			for(i=0;i<usrdirs[curlib];i++) {
				if(i==curdir[curlib]) continue;
				if(listfiles(usrdir[curlib][i],str,0,FL_VIEW))
					break; }
			if(i<usrdirs[curlib])
				return(0);
			bputs(text[SearchingAllLibs]);
			for(i=0;i<usrlibs;i++) {
				if(i==curlib) continue;
				for(j=0;j<usrdirs[i];j++)
					if(listfiles(usrdir[i][j],str,0,FL_VIEW))
						return(0); }
			csi->logic=LOGIC_FALSE;
			bputs(text[FileNotFound]);
			return(0);
		case CS_FILE_LIST:	  /* List files in current dir */
			if(!usrlibs) return(0);
			csi->logic=LOGIC_FALSE;
			if(!getfiles(&cfg,usrdir[curlib][curdir[curlib]])) {
				bputs(text[EmptyDir]);
				return(0); }
			padfname(csi->str,str);
			strupr(str);
			s=listfiles(usrdir[curlib][curdir[curlib]],str,0,0);
			if(s>1) {
				bprintf(text[NFilesListed],s); }
			csi->logic=!s;
			return(0);
		case CS_FILE_LIST_EXTENDED: /* Extended Information on files */
			if(!usrlibs) return(0);
			padfname(csi->str,str);
			strupr(str);
			if(!listfileinfo(usrdir[curlib][curdir[curlib]],str,FI_INFO)) {
				bputs(text[SearchingAllDirs]);
				for(i=0;i<usrdirs[curlib];i++)
					if(i!=curdir[curlib] && (s=listfileinfo(usrdir[curlib][i]
						,str,FI_INFO))!=0)
						if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
							return(0);
				bputs(text[SearchingAllLibs]);
				for(i=0;i<usrlibs;i++) {
					if(i==curlib) continue;
					for(j=0;j<usrdirs[i];j++)
						if((s=listfileinfo(usrdir[i][j],str,FI_INFO))!=0)
							if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
								return(0); } }
			return(0);
		case CS_FILE_FIND_TEXT: 	/* Find text in descriptions */
			scandirs(FL_FINDDESC);
			return(0);
		case CS_FILE_FIND_TEXT_ALL: 	/* Find text in descriptions */
			scanalldirs(FL_FINDDESC);
			return(0);
		case CS_FILE_FIND_NAME: 	/* Find text in descriptions */
			scandirs(FL_NO_HDR);
			return(0);
		case CS_FILE_FIND_NAME_ALL: 	/* Find text in descriptions */
			scanalldirs(FL_NO_HDR);
			return(0);
		case CS_FILE_BATCH_SECTION:
			batchmenu();
			return(0);
		case CS_FILE_TEMP_SECTION:
			temp_xfer();
			return(0);
		case CS_FILE_PTRS_CFG:
			csi->logic=!inputnstime(&ns_time);
			return(0);
		case CS_FILE_NEW_SCAN:
			scandirs(FL_ULTIME);
			return(0);
		case CS_FILE_NEW_SCAN_ALL:
			scanalldirs(FL_ULTIME);
			return(0);
		case CS_FILE_REMOVE:
			if(!usrlibs) return(0);
			if(useron.rest&FLAG('R')) {
				bputs(text[R_RemoveFiles]);
				return(0); }
			padfname(csi->str,str);
			strupr(str);
			if(!listfileinfo(usrdir[curlib][curdir[curlib]],str,FI_REMOVE)) {
				if(cfg.user_dir!=INVALID_DIR
					&& cfg.user_dir!=usrdir[curlib][curdir[curlib]])
					if((s=listfileinfo(cfg.user_dir,str,FI_REMOVE))!=0)
						if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
							return(0);
				bputs(text[SearchingAllDirs]);
				for(i=0;i<usrdirs[curlib];i++)
					if(i!=curdir[curlib] && i!=cfg.user_dir
						&& (s=listfileinfo(usrdir[curlib][i],str,FI_REMOVE))!=0)
						if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
							return(0);
				bputs(text[SearchingAllLibs]);
				for(i=0;i<usrlibs;i++) {
					if(i==curlib || i==cfg.user_dir) continue;
					for(j=0;j<usrdirs[i]; j++)
						if((s=listfileinfo(usrdir[i][j],str,FI_REMOVE))!=0)
							if(s==-1 || (!strchr(str,'?') && !strchr(str,'*')))
								return(0); } }
			return(0);
	 }

	errormsg(WHERE,ERR_CHK,"shell function",*(csi->ip-1));
	return(0);
}
