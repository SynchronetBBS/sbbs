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
#include "filedat.h"

#define BF_MAX	26	/* Batch Flag max: A-Z */	

int extdesclines(char *str);

/*****************************************************************************/
/* List files in directory 'dir' that match 'filespec'.						 */
/* 'mode' determines other criteria											 */
/* the files must meet before they'll be listed. 'mode' bit FL_NOHDR doesn't */
/* list the directory header.                                                */
/* Returns -1 if the listing was aborted, otherwise total files listed		 */
/*****************************************************************************/
int sbbs_t::listfiles(uint dirnum, const char *filespec, FILE* tofile, long mode)
{
	char	hdr[256],letter='A',*p;
	uchar	flagprompt=0;
	int		c, d;
	uint	i,j;
	int		found=0,lastbat=0,disp;
	size_t	m=0;
	long	anchor=0,next;
	file_t* bf[BF_MAX];	/* bf is batch flagged files */
	smb_t	smb;
	ulong	file_row[26];
	size_t	longest = 0;

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
	file_t* file_list = loadfiles(&smb
		, (mode&(FL_FINDDESC|FL_EXFIND)) ? NULL : filespec
		, (mode&FL_ULTIME) ? ns_time : 0
		, file_detail_extdesc
		, (enum file_sort)cfg.dir[dirnum]->sort
		, &file_count);
	if(file_list == NULL || file_count < 1) {
		smb_close(&smb);
		free(file_list);
		return 0;
	}

	for(i = 0; i < file_count; i++) {
		size_t len = strlen(file_list[i].name);
		if(len > longest)
			longest = len;
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
	file_t* f;
	while(online) {
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
		if(m < file_count)
			f = &file_list[m];

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
			p = (f->desc == NULL) ? NULL : strcasestr(f->desc, filespec);
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
					c = fprintf(tofile,"\r\n(%u) %s ",i+1,cfg.lib[usrlib[i]]->sname) - 2;
				}
				else {
					sprintf(hdr,text[ShortHdrLib],i+1,cfg.lib[usrlib[i]]->sname);
					bputs("\r\1>\r\n");
					bputs(hdr); 
					c=bstrlen(hdr);
				}
				if(tofile) {
					c += fprintf(tofile,"(%u) %s",j+1,cfg.dir[dirnum]->lname);
				}
				else {
					sprintf(hdr,text[ShortHdrDir],j+1,cfg.dir[dirnum]->lname);
					bputs(hdr); 
					c+=bstrlen(hdr);
				}
				if(tofile) {
					fprintf(tofile,"\r\n%.*s\r\n", c, "----------------------------------------------------------------");
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
			listfiletofile(f, tofile);
		else if(mode&FL_FINDDESC)
			disp=listfile(f, dirnum, filespec, letter, longest);
		else
			disp=listfile(f, dirnum, nulstr, letter, longest);
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
bool sbbs_t::listfile(file_t* f, uint dirnum, const char *search, const char letter, size_t namelen)
{
	char	*ptr;
	bool	exist = true;
	char*	ext=NULL;
	char	path[MAX_PATH+1];
    int		i,j;
    off_t	cdt;
	int		size_attr=clr_filecdt;

	if(f->extdesc != NULL && *f->extdesc && (useron.misc&EXTDESC)) {
		ext = f->extdesc;
		if((useron.misc&BATCHFLAG) && lncntr+extdesclines(ext)>=rows-2 && letter!='A')
			return false;
	}

	cond_newline();
	attr(cfg.color[(f->hdr.attr & MSG_DELETE) ? clr_err : clr_filename]);
	char fname[SMB_FILEIDX_NAMELEN + 1];
	if(namelen < 12 || cols < 132)
		namelen = 12;
	else if(namelen > sizeof(fname) - 1)
		namelen = sizeof(fname) - 1;
	bprintf("%-*s", (int)namelen, format_filename(f->name, fname, namelen, /* pad: */TRUE));
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
	if(f->size == -1) {
		exist = false;
		size_attr = clr_err; 
	}
	else if((cfg.dir[dirnum]->misc & (DIR_FREE | DIR_FCHK)) == (DIR_FREE | DIR_FCHK))
		cdt = getfilesize(&cfg, f);
	char bytes[32];
	unsigned units = 1;
	do {
		byte_estimate_to_str(cdt, bytes, sizeof(bytes), units, /* precision: */1);
		units *= 1024;
	} while(strlen(bytes) > 6 && units < 1024 * 1024 * 1024);
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
		char* fdesc = f->desc;
		SKIP_WHITESPACE(fdesc);
		if(fdesc == NULL || *fdesc == '\0')
			bputs(P_TRUNCATE, f->name);
		else if(search[0]) { /* high-light string in string */
			ptr = strcasestr(fdesc, search);
			if(ptr != NULL) {
				i=strlen(search);
				j=ptr - fdesc;
				bprintf("%.*s",j,fdesc);
				attr(cfg.color[clr_filedesc]^HIGH);
				bprintf("%.*s",i,fdesc+j);
				attr(cfg.color[clr_filedesc]);
				bprintf("%.*s",(int)strlen(fdesc)-(j+i),fdesc+j+i);
			}
		}
		else {
			bputs(P_TRUNCATE, fdesc);
		}
		CRLF; 
	} else {
		truncsp(ext);
		while(strncmp(ext, "\r\n", 2) == 0
			|| strnicmp(ext, "\001N", 2) == 0
			|| strnicmp(ext, "\0010", 2) == 0
			|| strnicmp(ext, "\001W", 2) == 0)
			ext += 2;
		putmsg(ext, P_INDENT | P_NOATCODES | P_CPM_EOF | P_TRUNCATE);
	}
	return true;
}

/****************************************************************************/
/* Batch flagging prompt for download, extended info, and archive viewing	*/
/* Returns -1 if 'Q' or Ctrl-C, 0 if skip, 1 if [Enter], 2 otherwise        */
/* or 3, backwards. 														*/
/****************************************************************************/
int sbbs_t::batchflagprompt(smb_t* smb, file_t** bf, ulong* row, uint total
							,long totalfiles)
{
	char	ch,str[256],*p,remcdt=0,remfile=0;
	int		c, d;
	char 	path[MAX_PATH + 1];
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
		if(ch=='T') {
			useron.misc ^= EXTDESC;
			return 2;
		}
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
				cond_newline();
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
				if(total > 1)
					newline();
				if(ch=='R') {
					if(noyes(text[RemoveFileQ]))
						return(2);
					remcdt = TRUE;
					remfile = TRUE;
					if(dir_op(smb->dirnum)) {
						remfile=!noyes(text[DeleteFileQ]);
						remcdt=!noyes(text[RemoveCreditsQ]);
					}
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
									if(removefile(smb, bf[i])) {
										if(remfile) {
											if(remove(getfilepath(&cfg, bf[i], path)) != 0)
												errormsg(WHERE, ERR_REMOVE, path);
										}
										if(remcdt)
											removefcdt(bf[i]);
									}
								}
								else if(ch=='M')
									movefile(smb, bf[i], usrdir[ml][md]); 
							} 
						} 
					}
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						file_t* f = bf[str[c]-'A'];
						if(ch=='R') {
							if(removefile(smb, f)) {
								if(remfile) {
									if(remove(getfilepath(&cfg, f, path)) != 0 && fexist(path))
										errormsg(WHERE, ERR_REMOVE, path);
								}
								if(remcdt)
									removefcdt(f);
							}
						}
						else if(ch=='M')
							movefile(smb, f, usrdir[ml][md]); 
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
int sbbs_t::listfileinfo(uint dirnum, const char *filespec, long mode)
{
	char	str[MAX_PATH + 1],path[MAX_PATH + 1],dirpath[MAX_PATH + 1],done=0,ch;
	char 	tmp[512];
	int		error;
	int		found=0;
    uint	i,j;
    size_t	m;
    time_t	start,end,t;
    file_t*	f;
	struct	tm tm;
	smb_t	smb;

	if(!smb_init_dir(&cfg, &smb, dirnum))
		return 0;
	if(smb_open_dir(&cfg, &smb, dirnum) != SMB_SUCCESS)
		return 0;

	size_t file_count = 0;
	file_t* file_list = loadfiles(&smb
		, filespec
		, /* time_t */0
		, file_detail_extdesc
		, (enum file_sort)cfg.dir[dirnum]->sort
		, &file_count);
	if(file_list == NULL || file_count < 1) {
		smb_close(&smb);
		free(file_list);
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
		if(mode==FI_USERXFER) {
			str_list_t dest_user_list = strListSplitCopy(NULL, f->to_list, ",");
			char usernum[16];
			SAFEPRINTF(usernum, "%u", useron.number);
			int dest_user = strListFind(dest_user_list, usernum, /* case-sensitive: */true);
			strListFree(&dest_user_list);
			if(dest_user < 0)
				continue;
		}
		SAFECOPY(dirpath, cfg.dir[f->dir]->path);
		if((mode==FI_REMOVE) && (!dir_op(dirnum) && stricmp(f->from
			,useron.alias) && !(useron.exempt&FLAG('R'))))
			continue;
		found++;
		if(mode==FI_INFO) {
			switch(viewfile(f, true)) {
				case 0:
					done=1;
					found=-1;
					break;
				case -2:
					m--;
					if(m)
						m--;
					break;
			} 
		}
		else {
			showfileinfo(f, /* show_extdesc: */mode != FI_DOWNLOAD);
//			newline();
		}
		if(mode==FI_REMOVE || mode==FI_OLD || mode==FI_OLDUL
			|| mode==FI_OFFLINE) {
			SYNC;
//			CRLF;
			SAFECOPY(str, "VEQRNP\b-\r");
			if(dir_op(dirnum)) {
				mnemonics(text[SysopRemoveFilePrompt]);
				SAFECAT(str,"FMC");
			}
			else if(useron.exempt&FLAG('R')) {
				mnemonics(text[RExemptRemoveFilePrompt]);
				SAFECAT(str,"M");
			}
			else
				mnemonics(text[UserRemoveFilePrompt]);
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
						if(!getstr(str, MAX_FILENAME_LEN, K_EDIT|K_AUTODEL))
							break;
						if(strcmp(str,f->name) != 0) { /* rename */
							if(stricmp(str,f->name)
								&& findfile(&cfg, f->dir, path, NULL))
								bprintf(text[FileAlreadyThere],path);
							else {
								SAFEPRINTF2(path,"%s%s",dirpath,f->name);
								SAFEPRINTF2(tmp,"%s%s",dirpath,str);
								if(fexistcase(path) && rename(path,tmp))
									bprintf(text[CouldntRenameFile],path,tmp);
								else {
									bprintf(text[FileRenamed],path,tmp);
									smb_new_hfield_str(f, SMB_FILENAME, str);
									updatefile(&cfg, f);
								} 
							} 
						} 
					}
					// Description
					bputs(text[EditDescription]);
					char fdesc[LEN_FDESC + 1];
					SAFECOPY(fdesc, f->desc);
					getstr(fdesc, sizeof(fdesc)-1, K_LINE|K_EDIT|K_AUTODEL|K_TRIM);
					if(sys_status&SS_ABORT)
						break;
					if(strcmp(fdesc, f->desc))
						smb_new_hfield_str(f, SMB_FILEDESC, fdesc);

					// Tags
					if((cfg.dir[dirnum]->misc & DIR_FILETAGS) || dir_op(dirnum)) {
						char tags[64] = "";
						bputs(text[TagFilePrompt]);
						if(f->tags != NULL)
							SAFECOPY(tags, f->tags);
						getstr(tags, sizeof(tags)-1, K_LINE|K_EDIT|K_AUTODEL|K_TRIM);
						if(sys_status&SS_ABORT)
							break;
						if((f->tags == NULL && *tags != '\0') || (f->tags != NULL && strcmp(tags, f->tags)))
							smb_new_hfield_str(f, SMB_TAGS, tags);
					}
					// Extended Description
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
					smb_new_hfield_str(f, SMB_FILEUPLOADER, uploader);
					ultoa(f->cost,str,10);
					bputs(text[EditCreditValue]);
					getstr(str,10,K_NUMBER|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					f->cost = atol(str);
					smb_new_hfield(f, SMB_COST, sizeof(f->cost), &f->cost);
					ultoa(f->hdr.times_downloaded,str,10);
					bputs(text[EditTimesDownloaded]);
					getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					f->hdr.times_downloaded=atoi(str);
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
								SAFEPRINTF(tmp, "deleted %s", str);
								logline(nulstr, tmp); 
							} 
						} 
					}
					break;
				case 'R':   /* remove file from database */
					if(noyes(text[RemoveFileQ]))
						break;
					if(removefile(&smb, f)) {
						getfilepath(&cfg, f, path);
						if(fexistcase(path)) {
							if(dir_op(dirnum)) {
								if(!noyes(text[DeleteFileQ])) {
									if(remove(path) != 0)
										errormsg(WHERE, ERR_REMOVE, path);
									else {
										SAFEPRINTF(tmp, "deleted %s", path);
										logline(nulstr,path); 
									} 
								} 
							}
							else if(remove(str))    /* always remove if not sysop */
								bprintf(text[CouldntRemoveFile],str);
						}
					}
					if(dir_op(dirnum) || useron.exempt&FLAG('R')) {
						i=cfg.lib[cfg.dir[f->dir]->lib]->offline_dir;
						if(i!=dirnum && i!=INVALID_DIR
							&& !findfile(&cfg, i, f->name, NULL)) {
							sprintf(str,text[AddToOfflineDirQ]
								,f->name,cfg.lib[cfg.dir[i]->lib]->sname,cfg.dir[i]->sname);
							if(yesno(str)) {
								addfile(&cfg, i, f, f->extdesc, /* client: */NULL);
							} 
						} 
					}
					if(dir_op(dirnum) || stricmp(f->from, useron.alias)) {
						if(noyes(text[RemoveCreditsQ]))
	/* Fall through */      break; 
					}
				case 'C':   /* remove credits only */
					removefcdt(f);
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
				case 'P':	/* previous */
				case '-':
				case '\b':
					m--;
					if(m)
						m--;
					break;
				case 'Q':   /* quit */
					found=-1;
					done=1;
					break; 
			} 
		}
		else if(mode==FI_DOWNLOAD || mode==FI_USERXFER) {
			getfilepath(&cfg, f, path);
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
			SYNC;
			mnemonics(text[ProtocolBatchQuitOrNext]);
			sprintf(str,"B%cN\r",text[YNQP][2]);
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->dlcmd[0]
					&& chk_ar(cfg.prot[i]->ar,&useron,&client)) {
					sprintf(str + strlen(str), "%c", cfg.prot[i]->mnemonic);
				}
	//		  ungetkey(useron.prot);
			ch=(char)getkeys(str,0);
			if(ch==text[YNQP][2]) {
				found=-1;
				done=1; 
			}
			else if(ch=='B') {
				if(!addtobatdl(f)) {
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
						getfilepath(&cfg, f, str);
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
		}
		if(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?')) 
			break; 
	}
	freefiles(file_list, file_count);
	smb_close(&smb);
	return(found);
}

/****************************************************************************/
/* Prints one file's information on a single line to a file stream 'fp'		*/
/****************************************************************************/
void sbbs_t::listfiletofile(file_t* f, FILE* fp)
{
	char fname[13];	/* This is one of the only 8.3 filename formats left! (used for display purposes only) */
	fprintf(fp, "%-*s %10lu %s\r\n", (int)sizeof(fname)-1, format_filename(f->name, fname, sizeof(fname)-1, /* pad: */TRUE)
		,(ulong)getfilesize(&cfg, f), f->desc);
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
