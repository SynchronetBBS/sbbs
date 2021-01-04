/* Synchronet file database listing functions */

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

#include "sbbs.h"

#define BF_MAX	26	/* Batch Flag max: A-Z */	

int extdesclines(char *str);

/*****************************************************************************/
/* List files in directory 'dir' that match 'filespec'.						 */
/* 'mode' determines other criteria											 */
/* the files must meet before they'll be listed. 'mode' bit FL_NOHDR doesn't */
/* list the directory header.                                                */
/* Returns -1 if the listing was aborted, otherwise total files listed		 */
/*****************************************************************************/
int sbbs_t::listfiles(uint dirnum, const char *filespec, int tofile, long mode)
{
	char	hdr[256],letter='A',*p;
	uchar	flagprompt=0;
	int		c, d;
	uint	i,j;
	int		found=0,lastbat=0,disp;
	size_t	m=0;
	long	anchor=0,next;
	smbfile_t* bf[BF_MAX];	/* bf is batch flagged files */
	smb_t	smb;
	ulong	file_row[26];

	if(!smb_init_dir(&cfg, &smb, dirnum))
		return 0;
	if(mode&FL_ULTIME) {
		last_ns_time = now;
		if(!newfiles(&smb, ns_time))	// this is fast
			return 0;
	}
	if(smb_open_dir(&cfg, &smb, dirnum) != SMB_SUCCESS)
		return 0;

	size_t file_count = 0;
	smbfile_t* file_list = loadfiles(&cfg, &smb
		, (mode&(FL_FINDDESC|FL_EXFIND)) ? NULL : filespec
		, (mode&FL_ULTIME) ? ns_time : 0
		, file_detail_normal
		, /* sort: */TRUE
		, &file_count);
	if(file_list == NULL || file_count < 1) {
		smb_close(&smb);
		return 0;
	}

	if(!tofile) {
		action=NODE_LFIL;
		getnodedat(cfg.node_num,&thisnode,0);
		if(thisnode.action!=NODE_LFIL) {	/* was a sync */
			if(getnodedat(cfg.node_num,&thisnode,true)==0) {
				thisnode.action=NODE_LFIL;
				putnodedat(cfg.node_num,&thisnode); 
			} 
		} 
	}

	m = 0; // current file index
	smbfile_t* f;
	while(online) {
		if(m < file_count)
			f = &file_list[m];
		if(found<0)
			found=0;
		if(m>=file_count || flagprompt) {		  /* End of list */
			if(useron.misc&BATCHFLAG && !tofile && found && found!=lastbat
				&& !(mode&(FL_EXFIND|FL_VIEW))) {
				flagprompt=0;
				lncntr=0;
				if((i=batchflagprompt(&smb, bf, file_row, letter-'A', file_count))==2) {
					m=anchor;
					found-=letter-'A';
					letter='A'; 
				}
				else if(i==3) {
					if((long)anchor-(letter-'A')<0) {
						m=0;
						found=0; 
					}
					else {
						m=anchor-(letter-'A');
						found-=letter-'A'; 
					}
					letter='A'; 
				}
				else if((int)i==-1) {
					found = -1;
					break;
				}
				else
					break;
				getnodedat(cfg.node_num,&thisnode,0);
				nodesync(); 
			}
			else
				break; 
		}

		if(letter>'Z')
			letter='A';
		if(letter=='A')
			anchor=m;

		if(msgabort()) {		 /* used to be !tofile && msgabort() */
			found = -1;
			break;
		}
#if 0 /* unnecessary? */
		if(!(mode&(FL_FINDDESC|FL_EXFIND)) && filespec[0]
			&& !filematch(str,filespec)) {
			m+=11;
			continue; 
		}
#endif
		if(mode&(FL_FINDDESC|FL_EXFIND)) {
			p = strcasestr(f->desc, filespec);
			if(!(mode&FL_EXFIND) && p==NULL) {
				m++;
				continue; 
			}
			if(mode&FL_EXFIND && f->extdesc != NULL) { /* search extended description */
				if(!strcasestr((char*)f->extdesc, filespec) && p == NULL) {	/* not in description or */
					m++;											 /* extended description */
					continue; 
				}
			}
			else if(p == NULL) {			 /* no extended description and not in desc */
				m++;
				continue; 
			} 
		}
/** necessary?
		if(mode&FL_ULTIME) {
			if(ns_time>(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)|((long)ixbbuf[m+5]<<16)
				|((long)ixbbuf[m+6]<<24))) {
				m+=11;
				continue; 
			}
		}
**/
		if(useron.misc&BATCHFLAG && letter=='A' && found && !tofile
			&& !(mode&(FL_EXFIND|FL_VIEW))
			&& (!mode || !(useron.misc&EXPERT)))
			bputs(text[FileListBatchCommands]);
		m++;
		if(!found && !(mode&(FL_EXFIND|FL_VIEW))) {
			for(i=0;i<usrlibs;i++)
				if(usrlib[i]==cfg.dir[dirnum]->lib)
					break;
			for(j=0;j<usrdirs[i];j++)
				if(usrdir[i][j]==dirnum)
					break;						/* big header */
			if((!mode || !(useron.misc&EXPERT)) && !tofile && (!filespec[0]
				|| (strchr(filespec,'*') || strchr(filespec,'?')))) {
				sprintf(hdr,"%s%s.hdr",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
				if(fexistcase(hdr))
					printfile(hdr,0);	/* Use DATA\DIRS\<CODE>.HDR */
				else {
					if(useron.misc&BATCHFLAG)
						bputs(text[FileListBatchCommands]);
					else {
						CLS;
						d=strlen(cfg.lib[usrlib[i]]->lname)>strlen(cfg.dir[dirnum]->lname) ?
							strlen(cfg.lib[usrlib[i]]->lname)+17
							: strlen(cfg.dir[dirnum]->lname)+17;
						if(i>8 || j>8) d++;
						attr(cfg.color[clr_filelsthdrbox]);
						bputs("\xc9\xcd");            /* use to start with \r\n */
						for(c=0;c<d;c++)
							outchar('\xcd');
						bputs("\xbb\r\n\xba ");
						sprintf(hdr,text[BoxHdrLib],i+1,cfg.lib[usrlib[i]]->lname);
						bputs(hdr);
						for(c=bstrlen(hdr);c<d;c++)
							outchar(' ');
						attr(cfg.color[clr_filelsthdrbox]);
						bputs("\xba\r\n\xba ");
						sprintf(hdr,text[BoxHdrDir],j+1,cfg.dir[dirnum]->lname);
						bputs(hdr);
						for(c=bstrlen(hdr);c<d;c++)
							outchar(' ');
						attr(cfg.color[clr_filelsthdrbox]);
						bputs("\xba\r\n\xba ");
						sprintf(hdr,text[BoxHdrFiles], file_count);
						bputs(hdr);
						for(c=bstrlen(hdr);c<d;c++)
							outchar(' ');
						attr(cfg.color[clr_filelsthdrbox]);
						bputs("\xba\r\n\xc8\xcd");
						for(c=0;c<d;c++)
							outchar('\xcd');
						bputs("\xbc\r\n"); 
					}
				}
			}
			else {					/* short header */
				if(tofile) {
					sprintf(hdr,"(%u) %s ",i+1,cfg.lib[usrlib[i]]->sname);
					write(tofile,crlf,2);
					write(tofile,hdr,strlen(hdr)); 
				}
				else {
					sprintf(hdr,text[ShortHdrLib],i+1,cfg.lib[usrlib[i]]->sname);
					bputs("\r\1>\r\n");
					bputs(hdr); 
				}
				c=bstrlen(hdr);
				if(tofile) {
					sprintf(hdr,"(%u) %s",j+1,cfg.dir[dirnum]->lname);
					write(tofile,hdr,strlen(hdr)); 
				}
				else {
					sprintf(hdr,text[ShortHdrDir],j+1,cfg.dir[dirnum]->lname);
					bputs(hdr); 
				}
				c+=bstrlen(hdr);
				if(tofile) {
					write(tofile,crlf,2);
					sprintf(hdr,"%*s",c,nulstr);
					memset(hdr,0xC4,c);
					strcat(hdr,crlf);
					write(tofile,hdr,strlen(hdr)); 
				}
				else {
					CRLF;
					attr(cfg.color[clr_filelstline]);
					while(c--)
						outchar('\xC4');
					CRLF; 
				} 
			} 
		}
		long currow = row;
		next=m;
		disp=1;
		if(mode&(FL_EXFIND|FL_VIEW)) {
			if(!found)
				bputs("\r\1>");
			if(!viewfile(f, INT_TO_BOOL(mode&FL_EXFIND))) {
				found = -1;
				break;
			}
			CRLF;
		}
		else if(tofile)
			listfiletofile(f, dirnum, tofile);
		else if(mode&FL_FINDDESC)
			disp=listfile(f, dirnum, filespec, letter);
		else
			disp=listfile(f, dirnum, nulstr, letter);
		if(!disp && letter>'A') {
			next=m-1;
			letter--; 
		}
		else {
			disp=1;
			found++; 
		}
		if(sys_status&SS_ABORT) {
			found = -1;
			break;
		}
		if(mode&(FL_EXFIND|FL_VIEW))
			continue;
		if(useron.misc&BATCHFLAG && !tofile) {
			if(disp) {
				bf[letter-'A'] = f;
				file_row[letter-'A'] = currow;
			}
			m++;
			if(flagprompt || letter=='Z' || !disp ||
				(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?')
				&& !(mode&FL_FINDDESC))
				|| (useron.misc&BATCHFLAG && !tofile && lncntr>=rows-2)
				) {
				flagprompt=0;
				lncntr=0;
				lastbat=found;
				if((int)(i=batchflagprompt(&smb, bf, file_row, letter-'A'+1, file_count))<1) {
					if((int)i==-1)
						found = -1;
					break;
				}
				if(i==2) {
					next=anchor;
					found-=(letter-'A')+1; 
				}
				else if(i==3) {
					if((long)anchor-((letter-'A'+1))<0) {
						next=0;
						found=0; 
					}
					else {
						next=anchor-((letter-'A'+1));
						found-=letter-'A'+1; 
					} 
				}
				getnodedat(cfg.node_num,&thisnode,0);
				nodesync();
				letter='A';	
			}
			else
				letter++; 
		}
		if(useron.misc&BATCHFLAG && !tofile
			&& lncntr>=rows-2) {
			lncntr=0;		/* defeat pause() */
			flagprompt=1; 
		}
		m=next;
		if(mode&FL_FINDDESC) continue;
		if(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?') && m)
			break; 
	}

	freefiles(file_list, file_count);
	smb_close(&smb);
	return found;
}

/****************************************************************************/
/* Prints one file's information on a single line                           */
/* Return 1 if displayed, 0 otherwise										*/
/****************************************************************************/
bool sbbs_t::listfile(smbfile_t* f, uint dirnum, const char *search, const char letter)
{
	char	*ptr,*cr,*lf;
	bool	exist = true;
	char*	ext=NULL;
	char	path[MAX_PATH+1];
    int		i,j;
    ulong	cdt;
	int		size_attr=clr_filecdt;

	if(f->extdesc != NULL && *f->extdesc && (useron.misc&EXTDESC)) {
		ext = f->extdesc;
		if((useron.misc&BATCHFLAG) && lncntr+extdesclines(ext)>=rows-2 && letter!='A')
			return false;
	}

	attr(cfg.color[clr_filename]);
	char fname[13];	/* This is one of the only 8.3 filename formats left! (used for display purposes only) */
	bprintf("%-*s", (int)sizeof(fname)-1, format_filename(f->name, fname, sizeof(fname)-1, /* pad: */TRUE));
	getfilepath(&cfg, f, path);

	if(f->extdesc != NULL && *f->extdesc && !(useron.misc&EXTDESC))
		outchar('+');
	else
		outchar(' '); 
	if(useron.misc&BATCHFLAG) {
		attr(cfg.color[clr_filedesc]);
		bprintf("%c",letter); 
	}
	cdt = f->cost;
	if(f->size < 0) {
		exist = false;
		size_attr = clr_err; 
	}
	else if((cfg.dir[dirnum]->misc & (DIR_FREE | DIR_FCHK)) == (DIR_FREE | DIR_FCHK))
		cdt = getfilesize(&cfg, f);
	char bytes[32];
	byte_estimate_to_str(cdt, bytes, sizeof(bytes), /* units: */1, /* precision: */1);
	attr(cfg.color[size_attr]);
	if(useron.misc&BATCHFLAG) {
		if(!cdt && !(cfg.dir[dirnum]->misc&DIR_FREE)) {
			attr(curatr^(HIGH|BLINK));
			bputs("  FREE"); 
		}
		else 
			bprintf("%6s", bytes);
	}
	else {
		if(!cdt && !(cfg.dir[dirnum]->misc&DIR_FREE)) {  /* FREE file */
			attr(curatr^(HIGH|BLINK));
			bputs("   FREE"); 
		}
		else 
			bprintf("%7s", bytes);
	}
	if(exist)
		outchar(' ');
	else
		outchar('-');
	attr(cfg.color[clr_filedesc]);

	if(ext == NULL) {
		if(search[0]) { /* high-light string in string */
			ptr = strcasestr(f->desc, search);
			if(ptr != NULL) {
				i=strlen(search);
				j=ptr - f->desc;
				bprintf("%.*s",j,f->desc);
				attr(cfg.color[clr_filedesc]^HIGH);
				bprintf("%.*s",i,f->desc+j);
				attr(cfg.color[clr_filedesc]);
				bprintf("%.*s",(int)strlen(f->desc)-(j+i),f->desc+j+i);
			}
		}
		else {
			if(f->desc == NULL || *f->desc == '\0')
				bputs(f->name);
			else
				bputs(f->desc);
		}
		CRLF; 
	} else {
		char* ext_desc = strdup((char*)ext);
		ptr=(char*)ext_desc;
		while(ptr && *ptr && !msgabort()) {
			cr=strchr(ptr,CR);
			lf=strchr(ptr,LF);
			if(lf && (lf<cr || !cr)) cr=lf;
			if(cr>ptr+LEN_FDESC)
				cr=ptr+LEN_FDESC;
			else if(cr)
				*cr=0;
			char str[256];
			sprintf(str,"%.*s\r\n",LEN_FDESC,ptr);
			putmsg(str,P_NOATCODES|P_SAVEATR);
			if(!cr) {
				if(strlen(ptr)>LEN_FDESC)
					cr=ptr+LEN_FDESC;
				else
					break; 
			}
			if(!(*(cr+1)) || !(*(cr+2)))
				break;
			bprintf("%21s",nulstr);
			ptr=cr;
			if(!(*ptr)) ptr++;
			while(*ptr==LF || *ptr==CR) ptr++; 
		}
		free(ext_desc);
	}
	return true;
}

/****************************************************************************/
/* Batch flagging prompt for download, extended info, and archive viewing	*/
/* Returns -1 if 'Q' or Ctrl-C, 0 if skip, 1 if [Enter], 2 otherwise        */
/* or 3, backwards. 														*/
/****************************************************************************/
int sbbs_t::batchflagprompt(smb_t* smb, smbfile_t** bf, ulong* row, uint total
							,long totalfiles)
{
	char	ch,str[256],fname[128],*p,remcdt=0,remfile=0;
	int		c, d;
	char 	tmp[512];
	uint	i,j,ml=0,md=0,udir,ulib;

	for(ulib=0;ulib<usrlibs;ulib++)
		if(usrlib[ulib]==cfg.dir[smb->dirnum]->lib)
			break;
	for(udir=0;udir<usrdirs[ulib];udir++)
		if(usrdir[ulib][udir]==smb->dirnum)
			break;

	CRLF;
	while(online) {
		bprintf(text[BatchFlagPrompt]
			,ulib+1
			,cfg.lib[cfg.dir[smb->dirnum]->lib]->sname
			,udir+1
			,cfg.dir[smb->dirnum]->sname
			,total, totalfiles);
		ch=getkey(K_UPPER);
		clearline();
		if(ch=='?') {
			menu("batflag");
			if(lncntr)
				pause();
			return(2); 
		}
		if(ch==text[YNQP][2] || sys_status&SS_ABORT)
			return(-1);
		if(ch=='S')
			return(0);
		if(ch=='P' || ch=='-')
			return(3);
		if(ch=='B' || ch=='D') {    /* Flag for batch download */
			if(useron.rest&FLAG('D')) {
				bputs(text[R_Download]);
				return(2); 
			}
			if(total==1) {
				addtobatdl(bf[0]);
				if(ch=='D')
					start_batch_download();
				CRLF;
				return(2); 
			}
			link_list_t saved_hotspots = mouse_hotspots;
			ZERO_VAR(mouse_hotspots);
			for(i=0; i < total; i++)
				add_hotspot((char)('A' + i), /* hungry: */true, -1, -1, row[i]);
			bputs(text[BatchDlFlags]);
			d=getstr(str, BF_MAX, K_NOCRLF);
			clear_hotspots();
			mouse_hotspots = saved_hotspots;
			lncntr=0;
			if(sys_status&SS_ABORT)
				return(-1);
			if(d > 0) { 	/* d is string length */
				strupr(str);
				CRLF;
				lncntr=0;
				for(c=0;c<d;c++) {
					if(batdn_total() >= cfg.max_batdn) {
						bprintf(text[BatchDlQueueIsFull],str+c);
						break; 
					}
					if(str[c]=='*' || strchr(str+c,'.')) {     /* filename or spec given */
//						f.dir=dirnum;
						p=strchr(str+c,' ');
						if(!p) p=strchr(str+c,',');
						if(p) *p=0;
						for(i=0;i<total;i++) {
							if(batdn_total() >= cfg.max_batdn) {
								bprintf(text[BatchDlQueueIsFull],str+c);
								break; 
							}
							if(filematch(bf[i]->name, str+c)) {
								addtobatdl(bf[i]); 
							} 
						} 
					}
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						addtobatdl(bf[str[c]-'A']);
					} 
				}
				if(ch=='D')
					start_batch_download();
				CRLF;
				return(2); 
			}
			clearline();
			continue; 
		}

		if(ch=='E' || ch=='V') {    /* Extended Info */
			if(total==1) {
				if(!viewfile(bf[0], ch=='E'))
					return(-1);
				return(2); 
			}
			link_list_t saved_hotspots = mouse_hotspots;
			ZERO_VAR(mouse_hotspots);
			for(i=0; i < total; i++)
				add_hotspot((char)('A' + i), /* hungry: */true, -1, -1, row[i]);
			bputs(text[BatchDlFlags]);
			d=getstr(str, BF_MAX, K_NOCRLF);
			clear_hotspots();
			mouse_hotspots = saved_hotspots;
			lncntr=0;
			if(sys_status&SS_ABORT)
				return(-1);
			if(d > 0) { 	/* d is string length */
				strupr(str);
				CRLF;
				lncntr=0;
				for(c=0;c<d;c++) {
					if(str[c]=='*' || strchr(str+c,'.')) {     /* filename or spec given */
//						f.dir=dirnum;
						p=strchr(str+c,' ');
						if(!p) p=strchr(str+c,',');
						if(p) *p=0;
						for(i=0;i<total;i++) {
//							padfname(str+c,tmp);
							if(filematch(bf[i]->name, str+c)) {
								if(!viewfile(bf[i], ch=='E'))
									return(-1); 
							} 
						} 
					}
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						if(!viewfile(bf[str[c]-'A'], ch=='E'))
							return(-1); 
					} 
				}
				return(2); 
			}
			clearline();
			continue; 
		}

		if((ch=='R' || ch=='M')     /* Delete or Move */
			&& !(useron.rest&FLAG('R'))
			&& (dir_op(smb->dirnum) || useron.exempt&FLAG('R'))) {
			if(total==1) {
				strcpy(str,"A");
				d=1; 
			}
			else {
				link_list_t saved_hotspots = mouse_hotspots;
				ZERO_VAR(mouse_hotspots);
				for(i=0; i < total; i++)
					add_hotspot((char)('A' + i), /* hungry: */true, -1, -1, row[i]);
				bputs(text[BatchDlFlags]);
				d=getstr(str, BF_MAX, K_NOCRLF);
				clear_hotspots();
				mouse_hotspots = saved_hotspots;
			}
			lncntr=0;
			if(sys_status&SS_ABORT)
				return(-1);
			if(d > 0) { 	/* d is string length */
				strupr(str);
				CRLF;
				if(ch=='R') {
					if(noyes(text[RemoveFileQ]))
						return(2);
					remcdt=remfile=1;
					if(dir_op(smb->dirnum)) {
						remcdt=!noyes(text[RemoveCreditsQ]);
						remfile=!noyes(text[DeleteFileQ]); } 
				}
				else if(ch=='M') {
					CRLF;
					for(i=0;i<usrlibs;i++)
						bprintf(text[MoveToLibLstFmt],i+1,cfg.lib[usrlib[i]]->lname);
					SYNC;
					bprintf(text[MoveToLibPrompt],cfg.dir[smb->dirnum]->lib+1);
					if((int)(ml=getnum(usrlibs))==-1)
						return(2);
					if(!ml)
						ml=cfg.dir[smb->dirnum]->lib;
					else
						ml--;
					CRLF;
					for(j=0;j<usrdirs[ml];j++)
						bprintf(text[MoveToDirLstFmt]
							,j+1,cfg.dir[usrdir[ml][j]]->lname);
					SYNC;
					bprintf(text[MoveToDirPrompt],usrdirs[ml]);
					if((int)(md=getnum(usrdirs[ml]))==-1)
						return(2);
					if(!md)
						md=usrdirs[ml]-1;
					else md--;
					CRLF; 
				}
				lncntr=0;
				for(c=0;c<d;c++) {
					if(str[c]=='*' || strchr(str+c,'.')) {     /* filename or spec given */
//						f.dir=dirnum;
						p=strchr(str+c,' ');
						if(!p) p=strchr(str+c,',');
						if(p) *p=0;
						for(i=0;i<total;i++) {
							if(filematch(bf[i]->name, str+c)) {
								if(ch=='R') {
									removefile(smb, bf[i]);
									if(remfile) {
										sprintf(tmp,"%s%s",cfg.dir[smb->dirnum]->path,fname);
										remove(tmp); 
									}
									if(remcdt)
										removefcdt(smb, bf[i]); 
								}
								else if(ch=='M')
									movefile(smb, bf[i], usrdir[ml][md]); 
							} 
						} 
					}
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						if(ch=='R') {
							removefile(smb, bf[str[c]-'A']);
							if(remfile) {
								sprintf(tmp,"%s%s",cfg.dir[smb->dirnum]->path,fname);
								remove(tmp); 
							}
							if(remcdt)
								removefcdt(smb, bf[str[c]-'A']); 
						}
						else if(ch=='M')
							movefile(smb, bf[str[c]-'A'], usrdir[ml][md]); 
					} 
				}
				return(2); 
			}
			clearline();
			continue; 
		}

		return(1); 
	}

	return(-1);
}

/****************************************************************************/
/* List detailed information about the files in 'filespec'. Prompts for     */
/* action depending on 'mode.'                                              */
/* Returns number of files matching filespec that were found                */
/****************************************************************************/
int sbbs_t::listfileinfo(uint dirnum, char *filespec, long mode)
{
	char	str[258],path[258],dirpath[MAX_PATH + 1],done=0,ch;
	char 	tmp[512];
	int		error;
	int		found=0;
    uint	i,j;
    size_t	m;
	long	usrcdt;
    time_t	start,end,t;
    smbfile_t*	f;
	struct	tm tm;
	smb_t	smb;

	if(!smb_init_dir(&cfg, &smb, dirnum))
		return 0;
	if(smb_open_dir(&cfg, &smb, dirnum) != SMB_SUCCESS)
		return 0;

	size_t file_count = 0;
	smbfile_t* file_list = loadfiles(&cfg, &smb
		, filespec
		, /* time_t */0
		, file_detail_extdesc
		, /* sort: */TRUE
		, &file_count);
	if(file_list == NULL || file_count < 1) {
		smb_close(&smb);
		return 0;
	}

	m=0;
	while(online && !done && m < file_count) {
		f = &file_list[m];
		if(mode==FI_REMOVE && dir_op(dirnum))
			action=NODE_SYSP;
		else action=NODE_LFIL;
		if(msgabort()) {
			found=-1;
			break; 
		}
		m++;
		if(mode==FI_OLD && f->hdr.last_downloaded > ns_time)
			continue;
		if((mode==FI_OLDUL || mode==FI_OLD) && f->hdr.when_written.time > ns_time)
			continue;
		curdirnum = dirnum;
		if(mode==FI_OFFLINE && getfilesize(&cfg, f) >= 0)
			continue;
		if(f->hdr.altpath>0 && f->hdr.altpath<=cfg.altpaths)
			strcpy(dirpath,cfg.altpath[f->hdr.altpath-1]);
		else
			strcpy(dirpath,cfg.dir[f->dir]->path);
		if(mode==FI_CLOSE /* && !f.opencount */)
			continue;
		if(mode==FI_USERXFER) {
#if 0
			for(p=usrxfrbuf;p<usrxfrbuf+usrxfrlen;p+=24) {
				sprintf(str,"%17.17s",p);   /* %4.4u %12.12s */
				if(!strcmp(str+5,f.name) && useron.number==atoi(str))
					break; 
			}
			if(p>=usrxfrbuf+usrxfrlen) /* file wasn't found */
#endif
				continue; 
		}
		if((mode==FI_REMOVE) && (!dir_op(dirnum) && stricmp(f->from
			,useron.alias) && !(useron.exempt&FLAG('R'))))
			continue;
		found++;
		if(mode==FI_INFO) {
			if(!viewfile(f, true)) {
				done=1;
				found=-1; 
			} 
		}
		else
			fileinfo(f);
		if(mode==FI_CLOSE) {
#if 0
			if(!noyes(text[CloseFileRecordQ])) {
				f.opencount=0;
				putfiledat(&cfg,&f); 
			} 
#endif
		}
		else if(mode==FI_REMOVE || mode==FI_OLD || mode==FI_OLDUL
			|| mode==FI_OFFLINE) {
			SYNC;
//			CRLF;
#if 0
			if(f.opencount) {
				mnemonics(text[QuitOrNext]);
				strcpy(str,"QN\r"); 
			}
			else
#endif
			if(dir_op(dirnum)) {
				mnemonics(text[SysopRemoveFilePrompt]);
				strcpy(str,"VEFMCQRN\r"); 
			}
			else if(useron.exempt&FLAG('R')) {
				mnemonics(text[RExemptRemoveFilePrompt]);
				strcpy(str,"VEMQRN\r"); 
			}
			else {
				mnemonics(text[UserRemoveFilePrompt]);
				strcpy(str,"VEQRN\r"); 
			}
			switch(getkeys(str,0)) {
				case 'V':
					viewfilecontents(f);
					CRLF;
					ASYNC;
					pause();
					m--;
					continue;
				case 'E':   /* edit file information */
					if(dir_op(dirnum)) {
						bputs(text[EditFilename]);
						SAFECOPY(str, f->name);
						if(!getstr(str, SMB_FILENAME_MAXLEN, K_EDIT|K_AUTODEL))
							break;
						if(strcmp(str,f->name) != 0) { /* rename */
							if(stricmp(str,f->name)
								&& findfile(&cfg,f->dir,path))
								bprintf(text[FileAlreadyThere],path);
							else {
								SAFEPRINTF2(path,"%s%s",dirpath,f->name);
								SAFEPRINTF2(tmp,"%s%s",dirpath,str);
								if(fexistcase(path) && rename(path,tmp))
									bprintf(text[CouldntRenameFile],path,tmp);
								else {
									bprintf(text[FileRenamed],path,tmp);
									smb_hfield_str(f, SMB_FILENAME, str);
									updatefile(&cfg, f);
								} 
							} 
						} 
					}
					bputs(text[EditDescription]);
					char fdesc[LEN_FDESC + 1];
					SAFECOPY(fdesc, f->desc);
					getstr(fdesc, sizeof(fdesc)-1, K_LINE|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					if(strcmp(fdesc, f->desc))
						smb_hfield_str(f, SMB_FILEDESC, fdesc);
					if(f->extdesc != NULL && *f->extdesc) {
						if(!noyes(text[DeleteExtDescriptionQ])) {
							// TODO
						} 
					}
					if(!dir_op(dirnum)) {
						updatefile(&cfg, f);
						break; 
					}
					char uploader[LEN_ALIAS + 1];
					SAFECOPY(uploader, f->from);
					bputs(text[EditUploader]);
					if(!getstr(uploader, sizeof(uploader), K_EDIT|K_AUTODEL))
						break;
					smb_hfield_str(f, SMB_FILEUPLOADER, uploader);
					ultoa(f->cost,str,10);
					bputs(text[EditCreditValue]);
					getstr(str,10,K_NUMBER|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					f->cost = atol(str);
					smb_hfield_bin(f, SMB_COST, f->cost);
					ultoa(f->hdr.times_downloaded,str,10);
					bputs(text[EditTimesDownloaded]);
					getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					f->hdr.times_downloaded=atoi(str);
					if(cfg.altpaths || f->hdr.altpath) {
						ultoa(f->hdr.altpath,str,10);
						bputs(text[EditAltPath]);
						getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
						f->hdr.altpath=atoi(str);
						if(f->hdr.altpath>cfg.altpaths)
							f->hdr.altpath=0; 
					}
					if(sys_status&SS_ABORT)
						break;
					inputnstime32((time32_t*)&f->hdr.when_imported.time);
					updatefile(&cfg, f);
					break;
				case 'F':   /* delete file only */
					SAFEPRINTF2(str,"%s%s",dirpath,f->name);
					if(!fexistcase(str))
						bprintf(text[FileDoesNotExist],str);
					else {
						if(!noyes(text[DeleteFileQ])) {
							if(remove(str))
								bprintf(text[CouldntRemoveFile],str);
							else {
								sprintf(tmp,"deleted %s"
									,str);
								logline(nulstr,tmp); 
							} 
						} 
					}
					break;
				case 'R':   /* remove file from database */
					if(noyes(text[RemoveFileQ]))
						break;
					removefile(&smb, f);
					SAFEPRINTF2(str,"%s%s",dirpath,f->name);
					if(fexistcase(str)) {
						if(dir_op(dirnum)) {
							if(!noyes(text[DeleteFileQ])) {
								if(remove(str))
									bprintf(text[CouldntRemoveFile],str);
								else {
									sprintf(tmp,"deleted %s"
										,str);
									logline(nulstr,tmp); 
								} 
							} 
						}
						else if(remove(str))    /* always remove if not sysop */
							bprintf(text[CouldntRemoveFile],str); 
					}
					if(dir_op(dirnum) || useron.exempt&FLAG('R')) {
						i=cfg.lib[cfg.dir[f->dir]->lib]->offline_dir;
						if(i!=dirnum && i!=INVALID_DIR
							&& !findfile(&cfg,i,f->name)) {
							sprintf(str,text[AddToOfflineDirQ]
								,f->name,cfg.lib[cfg.dir[i]->lib]->sname,cfg.dir[i]->sname);
							if(yesno(str)) {
								addfile(&cfg, i, f, f->extdesc);
							} 
						} 
					}
					if(dir_op(dirnum) || stricmp(f->from, useron.alias)) {
						if(noyes(text[RemoveCreditsQ]))
	/* Fall through */      break; 
					}
				case 'C':   /* remove credits only */
					if((i=matchuser(&cfg,f->from,TRUE /*sysop_alias*/))==0) {
						bputs(text[UnknownUser]);
						break; 
					}
					if(dir_op(dirnum)) {
						usrcdt=(ulong)(f->cost*(cfg.dir[f->dir]->up_pct/100.0));
						if(f->hdr.times_downloaded)     /* all downloads */
							usrcdt+=(ulong)((long)f->hdr.times_downloaded
								*f->cost*(cfg.dir[f->dir]->dn_pct/100.0));
						ultoa(usrcdt,str,10);
						bputs(text[CreditsToRemove]);
						getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL);
						f->cost=atol(str); 
					}
					usrcdt=adjustuserrec(&cfg,i,U_CDT,10,-(long)f->cost);
					if(i==useron.number)
						useron.cdt=usrcdt;
					sprintf(str,text[FileRemovedUserMsg]
						,f->name,f->cost ? ultoac(f->cost,tmp) : text[No]);
					putsmsg(&cfg,i,str);
					usrcdt=adjustuserrec(&cfg,i,U_ULB,10,-f->size);
					if(i==useron.number)
						useron.ulb=usrcdt;
					usrcdt=adjustuserrec(&cfg,i,U_ULS,5,-1);
					if(i==useron.number)
						useron.uls=(ushort)usrcdt;
					break;
				case 'M':   /* move the file to another dir */
					CRLF;
					for(i=0;i<usrlibs;i++)
						bprintf(text[MoveToLibLstFmt],i+1,cfg.lib[usrlib[i]]->lname);
					SYNC;
					bprintf(text[MoveToLibPrompt],cfg.dir[dirnum]->lib+1);
					if((int)(i=getnum(usrlibs))==-1)
						continue;
					if(!i)
						i=cfg.dir[dirnum]->lib;
					else
						i--;
					CRLF;
					for(j=0;j<usrdirs[i];j++)
						bprintf(text[MoveToDirLstFmt]
							,j+1,cfg.dir[usrdir[i][j]]->lname);
					SYNC;
					bprintf(text[MoveToDirPrompt],usrdirs[i]);
					if((int)(j=getnum(usrdirs[i]))==-1)
						continue;
					if(!j)
						j=usrdirs[i]-1;
					else j--;
					CRLF;
					movefile(&smb, f, usrdir[i][j]);
					break;
				case 'Q':   /* quit */
					found=-1;
					done=1;
					break; 
			} 
		}
		else if(mode==FI_DOWNLOAD || mode==FI_USERXFER) {
			SAFEPRINTF2(path,"%s%s",dirpath,f->name);
			if(getfilesize(&cfg, f) < 1L) { /* getfilesize will set this to -1 if non-existant */
				SYNC;       /* and 0 byte files shouldn't be d/led */
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; 
				}
				continue; 
			}
			if(!is_download_free(&cfg,f->dir,&useron,&client)
				&& f->cost>(useron.cdt+useron.freecdt)) {
				SYNC;
				bprintf(text[YouOnlyHaveNCredits]
					,ultoac(useron.cdt+useron.freecdt,tmp));
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; 
				}
				continue; 
			}
			if(!chk_ar(cfg.dir[f->dir]->dl_ar,&useron,&client)) {
				SYNC;
				bputs(text[CantDownloadFromDir]);
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; 
				}
				continue; 
			}

			if(!(cfg.dir[f->dir]->misc&DIR_TFREE) && gettimetodl(&cfg, f, cur_cps) > timeleft && !dir_op(dirnum)
				&& !(useron.exempt&FLAG('T'))) {
				SYNC;
				bputs(text[NotEnoughTimeToDl]);
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; 
				}
				continue; 
			}
			xfer_prot_menu(XFER_DOWNLOAD);
//			openfile(&f);
			SYNC;
			mnemonics(text[ProtocolBatchQuitOrNext]);
			sprintf(str,"B%cN\r",text[YNQP][2]);
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->dlcmd[0]
					&& chk_ar(cfg.prot[i]->ar,&useron,&client)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(str,tmp); 
				}
	//		  ungetkey(useron.prot);
			ch=(char)getkeys(str,0);
			if(ch==text[YNQP][2]) {
				found=-1;
				done=1; 
			}
			else if(ch=='B') {
				if(!addtobatdl(f)) {
//					closefile(&f);
					break; 
				} 
			}
			else if(ch!=CR && ch!='N') {
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->dlcmd[0] && cfg.prot[i]->mnemonic==ch
						&& chk_ar(cfg.prot[i]->ar,&useron,&client))
						break;
				if(i<cfg.total_prots) {
					delfiles(cfg.temp_dir,ALLFILES);
					if(cfg.dir[f->dir]->seqdev) {
						lncntr=0;
						seqwait(cfg.dir[f->dir]->seqdev);
						bprintf(text[RetrievingFile],f->name);
						SAFEPRINTF2(str,"%s%s",dirpath,f->name);
						SAFEPRINTF2(path,"%s%s",cfg.temp_dir,f->name);
						mv(str,path,1); /* copy the file to temp dir */
						if(getnodedat(cfg.node_num,&thisnode,true)==0) {
							thisnode.aux=0xf0;
							putnodedat(cfg.node_num,&thisnode);
						}
						CRLF; 
					}
					const char* file_ext = getfext(f->name);
					if(file_ext != NULL) {
						for(j=0; j<cfg.total_dlevents; j++) {
							if(!stricmp(cfg.dlevent[j]->ext, file_ext + 1)
								&& chk_ar(cfg.dlevent[j]->ar,&useron,&client)) {
								bputs(cfg.dlevent[j]->workstr);
								external(cmdstr(cfg.dlevent[j]->cmd,path,nulstr,NULL)
									,EX_OUTL);
								CRLF; 
							}
						}
					}
					getnodedat(cfg.node_num,&thisnode,1);
					action=NODE_DLNG;
					t=now + gettimetodl(&cfg, f, cur_cps);
					localtime_r(&t,&tm);
					thisnode.aux=(tm.tm_hour*60)+tm.tm_min;
					putnodedat(cfg.node_num,&thisnode); /* calculate ETA */
					start=time(NULL);
					error=protocol(cfg.prot[i],XFER_DOWNLOAD,path,nulstr,false);
					end=time(NULL);
					if(cfg.dir[f->dir]->misc&DIR_TFREE)
						starttime+=end-start;
					if(checkprotresult(cfg.prot[i],error, f))
						downloadedfile(f);
					else
						notdownloaded(f->size, start, end); 
					delfiles(cfg.temp_dir,ALLFILES);
					autohangup(); 
				} 
			} 
//			closefile(&f); 
		}
		if(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?')) 
			break; 
	}
	freefiles(file_list, file_count);
	smb_close(&smb);
	return(found);
}

/****************************************************************************/
/* Prints one file's information on a single line to a file 'file'          */
/****************************************************************************/
void sbbs_t::listfiletofile(smbfile_t* f, uint dirnum, int file)
{
	char	str[512];
    char	fpath[MAX_PATH + 1];
	bool	exist=true;

	SAFECOPY(str, f->name);
	if(f->extdesc != NULL)
		strcat(str, "+");
	else
		strcat(str, " ");
	getfilepath(&cfg, f, fpath);
	if(f->size < 0)
		exist=false;
	if(!f->cost)
		strcat(str, "   FREE");
	else
		sprintf(str+strlen(str), "%7lu", (ulong)f->cost);
	if(exist)
		strcat(str," ");
	else
		strcat(str,"-");
	strcat(str, f->desc);
	write(file,str,strlen(str));
	write(file,crlf,2);
}

int extdesclines(char *str)
{
	int i,lc,last;

	for(i=lc=last=0;str[i];i++)
		if(str[i]==LF || i-last>LEN_FDESC) {
			lc++;
			last=i; 
		}
	return(lc);
}
