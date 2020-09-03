#include <ctype.h>

#include "dirwrap.h"
#include "uifc.h"
#include "ciolib.h"

#include "filepick.h"

enum {
	 DIR_LIST
	,FILE_LIST
	,MASK_FIELD
	,CURRENT_PATH
	,FIELD_LIST_TERM
};

void drawfpwindow(uifcapi_t *api)
{
	struct vmem_cell lbuf[512];
	int i;
	int j;
	int listheight=0;
	int height;
	int width;
	struct vmem_cell shade[512];

	width=SCRN_RIGHT-SCRN_LEFT+1;

	height=api->scrn_len-3;
	/* Make sure it's odd */
	if(!(width%2))
		width--;

	listheight=height-7;

        i=0;
	set_vmem(&lbuf[i++], '\xc9', api->hclr|(api->bclr<<4), 0);
	for(j=1;j<width-1;j++)
		set_vmem(&lbuf[i++], '\xcd', api->hclr|(api->bclr<<4), 0);
	if(api->mode&UIFC_MOUSE && width>6) {
		set_vmem(&lbuf[1], '[', api->hclr|(api->bclr<<4), 0);
		set_vmem(&lbuf[2], '\xfe', api->lclr|(api->bclr<<4), 0);
		set_vmem(&lbuf[3], ']', api->hclr|(api->bclr<<4), 0);
		set_vmem(&lbuf[4], '[', api->hclr|(api->bclr<<4), 0);
		set_vmem(&lbuf[5], '?', api->lclr|(api->bclr<<4), 0);
		set_vmem(&lbuf[6], ']', api->hclr|(api->bclr<<4), 0);
		api->buttony=SCRN_TOP;
		api->exitstart=SCRN_LEFT+1;
		api->exitend=SCRN_LEFT+3;
		api->helpstart=SCRN_LEFT+4;
		api->helpend=SCRN_LEFT+6;
	}
	set_vmem(&lbuf[i++], '\xbb', api->hclr|(api->bclr<<4), 0);
	vmem_puttext(SCRN_LEFT,SCRN_TOP,SCRN_LEFT+width-1,SCRN_TOP,lbuf);
	set_vmem_attr(&lbuf[2], api->hclr|(api->bclr<<4));
	set_vmem_attr(&lbuf[5], api->hclr|(api->bclr<<4));
	for(j=1;j<7;j++)
		lbuf[j].ch='\xcd';
	lbuf[0].ch='\xc8';
	lbuf[(width-1)].ch='\xbc';
	vmem_puttext(SCRN_LEFT,SCRN_TOP+height-1
		,SCRN_LEFT+width-1,SCRN_TOP+height-1,lbuf);
	lbuf[0].ch='\xcc';
	lbuf[(width-1)].ch='\xb9';
	lbuf[(width-1)/2].ch='\xcb';
	vmem_puttext(SCRN_LEFT,SCRN_TOP+2,SCRN_LEFT+width-1,SCRN_TOP+2,lbuf);
	lbuf[(width-1)/2].ch='\xca';
	vmem_puttext(SCRN_LEFT,SCRN_TOP+3+listheight
		,SCRN_LEFT+width-1,SCRN_TOP+3+listheight,lbuf);
	lbuf[0].ch='\xba';
	lbuf[(width-1)].ch='\xba';
	for(j=1;j<(width-1);j++)
		lbuf[j].ch=' ';
	vmem_puttext(SCRN_LEFT,SCRN_TOP+1,
		SCRN_LEFT+width-1,SCRN_TOP+1,lbuf);
	vmem_puttext(SCRN_LEFT,SCRN_TOP+height-2,
		SCRN_LEFT+width-1,SCRN_TOP+height-2,lbuf);
	vmem_puttext(SCRN_LEFT,SCRN_TOP+height-3,
		SCRN_LEFT+width-1,SCRN_TOP+height-3,lbuf);
	lbuf[(width-1)/2].ch='\xba';
	for(j=0;j<listheight;j++)
		vmem_puttext(SCRN_LEFT,SCRN_TOP+3+j
			,SCRN_LEFT+width-1,SCRN_TOP+3+j,lbuf);

	/* Shadow */
	if(api->bclr==BLUE) {
		vmem_gettext(SCRN_LEFT+width,SCRN_TOP+1,SCRN_LEFT+width+1
			,SCRN_TOP+(height-1),shade);
		for(j=0;j<512;j++)
			set_vmem_attr(&shade[j], DARKGRAY);
		vmem_puttext(SCRN_LEFT+width,SCRN_TOP+1,SCRN_LEFT+width+1
			,SCRN_TOP+(height-1),shade);
		vmem_gettext(SCRN_LEFT+2,SCRN_TOP+height,SCRN_LEFT+width+1
			,SCRN_TOP+height,shade);
		for(j=0;j<width;j++)
			set_vmem_attr(&shade[j], DARKGRAY);
		vmem_puttext(SCRN_LEFT+2,SCRN_TOP+height,SCRN_LEFT+width+1
			,SCRN_TOP+height,shade);
	}
}

void free_opt_list(char ***opts)
{
	char **p;

	if(*opts==NULL)
		return;
	for(p=*opts; *p && (*p)[0]; p++) {
		if(*p)
			FREE_AND_NULL((*p));
	}
	FREE_AND_NULL(*opts);
}

char *insensitive_mask(char *mask)
{
#ifdef __unix__
	char *in;
	char *out;
	static char nmask[MAX_PATH*4+1];

	out=nmask;
	for(in=mask; *in; in++) {
		if(isalpha(*in)) {
			*(out++)='[';
			*(out++)=tolower(*in);
			*(out++)=toupper(*in);
			*(out++)=']';
		}
		else
			*(out++)=*in;
	}
	*out=0;
	return(nmask);
#else
	return(mask);
#endif
}

char **get_file_opt_list(char **fns, int files, int dirsonly, int root)
{
	char **opts;
	int  i;
	int  j=0;

	opts=(char **)malloc((files+2)*sizeof(char *));
	if(opts==NULL)
		return(NULL);
	memset(opts, 0, (files+2)*sizeof(char *));
	if(dirsonly) {
		if(!root)
			opts[j++]=strdup("..");
	}
	for(i=0;i<files;i++) {
		if(isdir(fns[i])) {
			if(dirsonly)
				opts[j++]=strdup(getdirname(fns[i]));
		}
		else {
			if(!dirsonly)
				opts[j++]=strdup(getfname(fns[i]));
		}
	}
	opts[j]="";
	return(opts);
}

void display_current_path(uifcapi_t *api, char *path)
{
	char	dpath[MAX_PATH+2];
	size_t	width;
	int height;
#ifdef _WIN32
	char	*p;
#endif

	height=api->scrn_len-3;
	width=SCRN_RIGHT-SCRN_LEFT-3;
	SAFECOPY(dpath, path);
	while(strlen(dpath) > width) {
		/* Just remove paths from the start. */
		dpath[0]='.';
		dpath[1]='.';
		dpath[2]='.';
		memmove(dpath+3, dpath+(strlen(dpath)-width+4), width-1);
	}
	/* For Win32, convert all "confusing" / to \\ */
#ifdef _WIN32
	for(p=dpath; *p; p++) {
		if(*p=='/')
			*p='\\';
	}
	if (strncmp(dpath, "\\\\?\\", 4)==0)
		memmove(dpath, dpath+4, strlen(dpath+3));
#endif

	api->printf(SCRN_LEFT+2, SCRN_TOP+height-2, api->lclr|(api->bclr<<4), "%-*s", width, dpath);
}

int mousetofield(int currfield, int opts, int height, int width, int listheight, int listwidth, int *dcur, int *dbar, int *fcur, int *fbar)
{
	int newfield;
	int bardif;
	struct mouse_event mevnt;

	newfield=currfield;
	if(getmouse(&mevnt)==0) {
		if(mevnt.endx >= SCRN_LEFT + 1
				&& mevnt.endx <= SCRN_LEFT + listwidth
				&& mevnt.endy >= SCRN_TOP + 3
				&& mevnt.endy <= SCRN_TOP + 2 + listheight) {
			newfield = DIR_LIST;
			if(mevnt.endx == SCRN_LEFT + 1)
				ungetmouse(&mevnt);
			else {
				bardif = (mevnt.starty - SCRN_TOP - 3) - *dbar;
				*dbar += bardif;
				*dcur += bardif;
			}
		}
		if(mevnt.endx >= SCRN_LEFT + 1 + listwidth + 1
				&& mevnt.endx <= SCRN_LEFT + 1 + listwidth * 2
				&& mevnt.endy >= SCRN_TOP + 3
				&& mevnt.endy <= SCRN_TOP + 2 + listheight) {
			newfield = FILE_LIST;
			if(mevnt.endx == SCRN_LEFT + 1 + listwidth + 1)
				ungetmouse(&mevnt);
			else {
				bardif = (mevnt.starty - SCRN_TOP - 3) - *fbar;
				*fbar += bardif;
				*fcur += bardif;
			}
		}
		if(!(opts & UIFC_FP_MSKNOCHG)
				&& (mevnt.endx >= SCRN_LEFT + 1
					&& mevnt.endx <= SCRN_LEFT + width - 2
					&& mevnt.endy == SCRN_TOP + height - 3)) {
			newfield = MASK_FIELD;
			ungetmouse(&mevnt);
		}
		if(opts & UIFC_FP_ALLOWENTRY
				&& mevnt.endx >= SCRN_LEFT + 1
				&& mevnt.endx <= SCRN_LEFT + width - 2
				&& mevnt.endy == SCRN_TOP + height - 2) {
			newfield = CURRENT_PATH;
			ungetmouse(&mevnt);
		}
	}
	return(newfield);
}

int filepick(uifcapi_t *api, char *title, struct file_pick *fp, char *dir, char *msk, int opts)
{
	char	cfile[MAX_PATH*5+1];		/* Current full path to file */
	char	cpath[MAX_PATH+1];			/* Current path */
	char	drive[3];
	char	tdir[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ext[MAX_PATH+1];
	char	cmsk[MAX_PATH*4+1];			/* Current file mask */
	char	cglob[MAX_PATH*4+1];		/* File glob patter */
	char	dglob[MAX_PATH*4+1];		/* Directory glob pattern */
	char	*p;
	glob_t	fgl;						/* Files */
	glob_t	dgl;						/* Directories */
	int		dircur=0;
	int		dirbar=0;
	int		filecur=0;
	int		filebar=0;
	int		listwidth;
	char	**dir_list=NULL;
	char	**file_list=NULL;
	int		currfield;
	int		lastfield;
	int		i;
	int		root=0;						/* Is this the root of the file system? */
										/* On *nix, this just means no .. on Win32,
										 * Something should be done about drive letters. */
	int		reread=FALSE;
	int		lbclr;
	char	*lastpath=NULL;
	char	*tmplastpath=NULL;
	char	*tmppath=NULL;
	int		width;
	int		height;
	char	*YesNo[]={"Yes", "No", ""};
	int		finished=FALSE;
	int		retval=0;
	int		fieldmove;
	int		oldhu=hold_update;
	int		oldx=wherex();
	int		oldy=wherey();

	height=api->scrn_len-3;
	width=SCRN_RIGHT-SCRN_LEFT-3;

	lbclr=api->lbclr;

	/* No struct passed */
	if(fp==NULL)
		return(-1);

	/* Illegal options */
	if((opts & UIFC_FP_MULTI)==UIFC_FP_MULTI && (opts & (UIFC_FP_ALLOWENTRY|UIFC_FP_OVERPROMPT|UIFC_FP_CREATPROMPT)))
		return(-1);

	if (opts & UIFC_FP_DIRSEL)
		currfield = lastfield = DIR_LIST;
	else
		currfield = lastfield = FILE_LIST;

	fp->files=0;
	fp->selected=NULL;

	/* No initial path specified */
	if(dir==NULL || !dir[0])
		SAFECOPY(cpath,".");

	FULLPATH(cpath,((dir==NULL||dir[0]==0)?".":dir),sizeof(cpath));
	backslash(cpath);

	if(msk==NULL || msk[0]==0) {
		SAFECOPY(cmsk,"*");
	}
	else {
		SAFECOPY(cmsk,msk);
	}
	sprintf(cfile,"%s%s",cpath,cmsk);
	listwidth=SCRN_RIGHT-SCRN_LEFT+1;
	listwidth-=listwidth%2;
	listwidth-=3;
	listwidth/=2;
	/* Draw the file picker itself... */
	hold_update = TRUE;
	drawfpwindow(api);
	/* Display the title centered */
	i=strlen(title);
	if(i>width-4)
		i=width-4;
	api->printf(SCRN_LEFT+2, SCRN_TOP+1, api->hclr|(api->bclr<<4), "%*s%-*s", (width-i)/2-2, "", i, title);
	api->printf(SCRN_LEFT+2, SCRN_TOP+height-3, api->hclr|(api->bclr<<4), "Mask: ");
	while(!finished) {
		hold_update = TRUE;
		api->printf(SCRN_LEFT+8, SCRN_TOP+height-3, api->lclr|(api->bclr<<4), "%-*s", width-7, cmsk);
		tmppath=strdup(cpath);
		if(tmppath != NULL) {
#ifdef _WIN32
			if (tmppath[0] == 0 || (tmppath[5]==0 && tmppath[3]=='.' && tmppath[4]=='.' && tmppath[1]==':' && IS_PATH_DELIM(tmppath[2])))
				strcpy(cpath, "\\\\?\\");
			else if(strncmp(tmppath, "\\\\?\\", 4)==0 && tmppath[4])
				strcpy(cpath, tmppath+4);
			else
#endif
			{
				FULLPATH(cpath,tmppath,sizeof(cpath));
			}
		}
		FREE_AND_NULL(tmppath);

#ifdef __unix__
		if(cpath[0]==0) {
			cpath[0]='/';
			cpath[1]=0;
		}
#endif
		backslash(cpath);
		sprintf(cglob,"%s%s",cpath,(opts&UIFC_FP_MSKCASE)?cmsk:insensitive_mask(cmsk));
		sprintf(dglob,"%s*",cpath);
		switch(currfield) {
			case DIR_LIST:
				if(lastfield==DIR_LIST)
					sprintf(cfile,"%s%s",cpath,cmsk);
				break;
		}

#ifdef __unix__
		if(cpath[0]==0) {
			cpath[0]='/';
			cpath[1]=0;
		}
		if(cpath[1]==0)
			root=TRUE;
		else
			root=FALSE;
#endif

#ifdef _WIN32
		if (strcmp(cpath, "\\\\?\\")==0)
			root = TRUE;
		else
			root = FALSE;
#endif

#ifdef _WIN32
		// Hack together some hawtness
		if (root) {
			unsigned long drives = _getdrives();
			int j;
			char path[4];

			memset(&dgl, 0, sizeof(dgl));
			strcpy(path, "A:\\");
			dgl.gl_pathv=malloc(sizeof(char *)*('Z'-'A'+2));
			for (j=0; j<='Z'-'A'; j++) {
				if(drives & (1<<j)) {
					path[0]='A'+j;
					dgl.gl_pathv[dgl.gl_pathc++]=strdup(path);
				}
			}
		}
		else 
#endif
		{
			if(glob(dglob, GLOB_MARK, NULL, &dgl)!=0 && !isdir(cpath)) {
				if(lastpath==NULL) {
					fp->files=0;
					retval=-1;
					goto cleanup;
				}
				hold_update=FALSE;
				api->msg("Cannot read directory!");
				if (api->exit_flags & UIFC_XF_QUIT) {
					retval=fp->files=0;
					goto cleanup;
				}
				SAFECOPY(cpath, lastpath);
				FREE_AND_NULL(lastpath);
				currfield=lastfield;
				continue;
			}
		}
		if(glob(cglob, 0, NULL, &fgl)!=0)
			fgl.gl_pathc=0;
		api->list_height=api->scrn_len-3-7;
		dir_list=get_file_opt_list(dgl.gl_pathv, dgl.gl_pathc, TRUE, root);
		file_list=get_file_opt_list(fgl.gl_pathv, fgl.gl_pathc, FALSE, root);
		globfree(&dgl);
		globfree(&fgl);
		reread=FALSE;
		dircur=dirbar=filecur=filebar=0;
		while(!reread) {
			hold_update=TRUE;
			display_current_path(api, cfile);
			api->lbclr=api->lclr|(api->bclr<<4);
			api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_IMM|WIN_REDRAW,1,3,listwidth,&dircur,&dirbar,NULL,dir_list);
			api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_IMM|WIN_REDRAW,1+listwidth+1,3,listwidth,&filecur,&filebar,NULL,file_list);
			api->lbclr=lbclr;
			lastfield=currfield;
			fieldmove=0;
			hold_update = FALSE;
			switch(currfield) {
				case DIR_LIST:
					i=api->list(WIN_DYN|WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_EXTKEYS|WIN_UNGETMOUSE|WIN_REDRAW,1,3,listwidth,&dircur,&dirbar,NULL,dir_list);
					if(i==-1) {		/* ESC */
						retval=fp->files=0;
						goto cleanup;
					}
					if(i==-2-'\t' || i==-2-CIO_KEY_RIGHT)	/* TAB */
						fieldmove=1;
					if(i==-3842)	/* Backtab */
						fieldmove=-1;
					if(i==-2-CIO_KEY_MOUSE)
						currfield=mousetofield(currfield, opts, height, width, api->list_height, listwidth, &dircur, &dirbar, &filecur, &filebar);
					if(i>=0) {
						FREE_AND_NULL(lastpath);
						lastpath=strdup(cpath);
						strcat(cpath,dir_list[i]);
						reread=TRUE;
						sprintf(cfile,"%s%s",cpath,cmsk);
					}
					break;
				case FILE_LIST:
					i=api->list(WIN_DYN|WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_EXTKEYS|WIN_UNGETMOUSE|WIN_REDRAW,1+listwidth+1,3,listwidth,&filecur,&filebar,NULL,file_list);
					if(i==-1) {
						retval=fp->files=0;
						goto cleanup;
					}
					if(i>=0) {
						sprintf(cfile,"%s%s",cpath,file_list[i]);
						if((opts & UIFC_FP_MULTI)!=UIFC_FP_MULTI) {
							retval=fp->files=1;
							fp->selected=(char **)malloc(sizeof(char *));
							if(fp->selected==NULL) {
								fp->files=0;
								retval=-1;
								goto cleanup;
							}
							fp->selected[0]=strdup(cfile);
							if(fp->selected[0]==NULL) {
								FREE_AND_NULL(fp->selected);
								fp->files=0;
								retval=-1;
								goto cleanup;
							}
							api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_IMM|WIN_REDRAW,1+listwidth+1,3,listwidth,&filecur,&filebar,NULL,file_list);
							finished=reread=TRUE;
						}
					}
					if(i==-2-'\t')
						fieldmove=1;
					if(i==-3842 || i==-2-CIO_KEY_LEFT)	/* Backtab */
						fieldmove=-1;
					if(i==-2-CIO_KEY_MOUSE)
						currfield=mousetofield(currfield, opts, height, width, api->list_height, listwidth, &dircur, &dirbar, &filecur, &filebar);
					break;
				case CURRENT_PATH:
					FREE_AND_NULL(tmplastpath);
					tmplastpath=strdup(cpath);
#ifdef _WIN32
					if (strncmp(tmplastpath, "\\\\?\\", 4)==0)
						memmove(tmplastpath, tmplastpath+4, strlen(tmplastpath+3));
					if (strncmp(cfile, "\\\\?\\", 4)==0)
						memmove(cfile, cfile+4, strlen(cfile+3));
#endif
					api->getstrxy(SCRN_LEFT+2, SCRN_TOP+height-2, width-1, cfile, sizeof(cfile)-1, K_EDIT|K_TABEXIT|K_MOUSEEXIT, &i);
					if(i==ESC) {
						retval=fp->files=0;
						goto cleanup;
					}
					if((opts & (UIFC_FP_FILEEXIST|UIFC_FP_PATHEXIST)) && !fexist(cfile)) {
#ifdef _WIN32
						if (cfile[0])	// Allow zero-length path to mean "Drive list"
#endif
						{
							FREE_AND_NULL(tmplastpath);
							api->msg("No such path/file!");
							if (api->exit_flags & UIFC_XF_QUIT) {
								retval=fp->files=0;
								goto cleanup;
							}
							continue;
						}
					}
					if(isdir(cfile) && cfile[0])
						backslash(cfile);
					_splitpath(cfile, drive, tdir, fname, ext);
					sprintf(cpath,"%s%s",drive,tdir);
					if(!isdir(cpath)) {
#ifdef _WIN32
						if (cfile[0] && strcmp(cfile, cmsk))	// Allow zero-length path to mean "Drive list"
#endif
						{
							FREE_AND_NULL(tmplastpath);
							api->msg("No such path!");
							if (api->exit_flags & UIFC_XF_QUIT) {
								retval=fp->files=0;
								goto cleanup;
							}
							continue;
						}
					}
					if(i==CIO_KEY_MOUSE)
						currfield=mousetofield(currfield, opts, height, width, api->list_height, listwidth, &dircur, &dirbar, &filecur, &filebar);
					if(i==3840)
						fieldmove=-1;
					else {
						if(currfield == CURRENT_PATH)
							fieldmove=1;
					}
					sprintf(cfile,"%s%s%s%s",drive,tdir,fname,ext);
					if(strchr(fname,'*') !=NULL || strchr(fname,'?') !=NULL
						|| strchr(ext,'*') !=NULL || strchr(ext,'?') !=NULL
						|| ((isdir(cfile) || cfile[0]==0) && !(opts & UIFC_FP_DIRSEL) && (i=='\r' || i=='\n'))
						|| (!isdir(cfile) && i!='\r' && i!='\n')) {
						if(opts & UIFC_FP_MSKNOCHG) {
							sprintf(cfile,"%s%s%s",drive,tdir,cmsk);
							FREE_AND_NULL(tmplastpath);
							api->msg("File mask cannot be changed");
							if (api->exit_flags & UIFC_XF_QUIT) {
								retval=fp->files=0;
								goto cleanup;
							}
							continue;
						}
						else {
							if((!isdir(cfile)) && cpath[0] != 0)
								sprintf(cmsk, "%s%s", fname, ext);
							reread=TRUE;
						}
						break;
					}
					else {
						if((opts & UIFC_FP_MULTI)!=UIFC_FP_MULTI && (i=='\r' || i!='\n'))
						fieldmove=0;
					}
					if((currfield != CURRENT_PATH) || fieldmove)
						break;
					if(isdir(cfile)) {
						if((opts & UIFC_FP_MULTI)!=UIFC_FP_MULTI && i!='\t' && i != 3840) {
							if(opts & UIFC_FP_DIRSEL) {
								finished=reread=TRUE;
								retval=fp->files=1;
								fp->selected=(char **)malloc(sizeof(char *));
								if(fp->selected==NULL) {
									fp->files=0;
									retval=-1;
									goto cleanup;
								}
								fp->selected[0]=strdup(cfile);
								if(fp->selected[0]==NULL) {
									free(fp->selected);
									fp->files=0;
									retval=-1;
									goto cleanup;
								}
							}
						}
						SAFECOPY(cpath, cfile);
						backslash(cfile);
						strcat(cfile,cmsk);
					}
					if(tmplastpath != NULL) {
						if(strcmp(tmplastpath, cpath)) {
							reread=TRUE;
							FREE_AND_NULL(lastpath);
							lastpath=tmplastpath;
							tmplastpath=NULL;
						}
					}
					FREE_AND_NULL(tmplastpath);
					if((opts & UIFC_FP_MULTI)!=UIFC_FP_MULTI && i!='\t' && i!=3840 && cpath[0]) {
						retval=fp->files=1;
						fp->selected=(char **)malloc(sizeof(char *));
						if(fp->selected==NULL) {
							fp->files=0;
							retval=-1;
							goto cleanup;
						}
						fp->selected[0]=strdup(cfile);
						if(fp->selected[0]==NULL) {
							FREE_AND_NULL(fp->selected);
							fp->files=0;
							retval=-1;
							goto cleanup;
						}
						finished=reread=TRUE;
					}
					break;
				case MASK_FIELD:
					p=strdup(cmsk);
					api->getstrxy(SCRN_LEFT+8, SCRN_TOP+height-3, width-7, cmsk, sizeof(cmsk)-1, K_EDIT|K_TABEXIT|K_MOUSEEXIT, &i);
					if(i==CIO_KEY_MOUSE)
						currfield=mousetofield(currfield, opts, height, width, api->list_height, listwidth, &dircur, &dirbar, &filecur, &filebar);
					if(i==ESC || i==CIO_KEY_QUIT || (api->exit_flags & UIFC_XF_QUIT)) {
						FREE_AND_NULL(p);
						retval=fp->files=0;
						goto cleanup;
					}
					if(strcmp(cmsk, p)) {
						sprintf(cfile,"%s%s",cpath,cmsk);
						reread=TRUE;
					}
					FREE_AND_NULL(p);
					if(i==3840)
						fieldmove=-1;
					else
						fieldmove=1;
					break;
			}
			currfield+=fieldmove;
			if(currfield<0)
				currfield=FIELD_LIST_TERM-1;
			while(1) {
				if(currfield==MASK_FIELD && (opts & UIFC_FP_MSKNOCHG)) {
					currfield+=fieldmove;
					continue;
				}
				if(currfield==CURRENT_PATH && !(opts & UIFC_FP_ALLOWENTRY)) {
					currfield+=fieldmove;
					continue;
				}
				break;
			}
			if(currfield==FIELD_LIST_TERM)
				currfield=DIR_LIST;
		}
		free_opt_list(&file_list);
		free_opt_list(&dir_list);
		if(finished) {
			if((opts & UIFC_FP_OVERPROMPT) && fexist(cfile)) {
				if(api->list(WIN_MID|WIN_SAV, 0,0,0, &i, NULL, "File exists, overwrite?", YesNo)!=0) {
					if (api->exit_flags & UIFC_XF_QUIT) {
						retval=fp->files=0;
						goto cleanup;
					}
					finished=FALSE;
				}
			}
			if((opts & UIFC_FP_CREATPROMPT) && !fexist(cfile)) {
				if(api->list(WIN_MID|WIN_SAV, 0,0,0, &i, NULL, "File does not exist, create?", YesNo)!=0) {
					if (api->exit_flags & UIFC_XF_QUIT) {
						retval=fp->files=0;
						goto cleanup;
					}
					finished=FALSE;
				}
			}
		}
	}

cleanup:		/* Cleans up allocated variables returns from function */
	hold_update=oldhu;
	gotoxy(oldx,oldy);
	FREE_AND_NULL(lastpath);
	FREE_AND_NULL(tmppath);
	FREE_AND_NULL(tmplastpath);
	free_opt_list(&file_list);
	free_opt_list(&dir_list);
	return(retval);
}

int filepick_free(struct file_pick *fp)
{
	int i;

	for(i=0; i<fp->files; i++) {
		FREE_AND_NULL(fp->selected[i]);
	}
	FREE_AND_NULL(fp->selected);
	return(0);
}
