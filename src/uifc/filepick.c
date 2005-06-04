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

int drawfpwindow(uifcapi_t *api)
{
	char lbuf[1024];
	int i;
	int j;
	int y;
	int listheight=0;
	int height;
	int width;
	char  shade[1024];

	width=SCRN_RIGHT-SCRN_LEFT+1;

	height=api->scrn_len-3;
	/* Make sure it's odd */
	if(!(width%2))
		width--;

	listheight=height-7;

        i=0;
	lbuf[i++]='\xc9';
	lbuf[i++]=api->hclr|(api->bclr<<4);
	for(j=1;j<width-1;j++) {
		lbuf[i++]='\xcd';
		lbuf[i++]=api->hclr|(api->bclr<<4);
	}
	if(api->mode&UIFC_MOUSE && width>6) {
		lbuf[2]='[';
		lbuf[3]=api->hclr|(api->bclr<<4);
		lbuf[4]=0xfe;
		lbuf[5]=api->lclr|(api->bclr<<4);
		lbuf[6]=']';
		lbuf[7]=api->hclr|(api->bclr<<4);
		lbuf[8]='[';
		lbuf[9]=api->hclr|(api->bclr<<4);
		lbuf[10]='?';
		lbuf[11]=api->lclr|(api->bclr<<4);
		lbuf[12]=']';
		lbuf[13]=api->hclr|(api->bclr<<4);
		api->buttony=SCRN_TOP;
		api->exitstart=SCRN_LEFT+1;
		api->exitend=SCRN_LEFT+3;
		api->helpstart=SCRN_LEFT+4;
		api->helpend=SCRN_LEFT+6;
	}
	lbuf[i++]='\xbb';
	lbuf[i]=api->hclr|(api->bclr<<4);
	puttext(SCRN_LEFT,SCRN_TOP,SCRN_LEFT+width-1,SCRN_TOP,lbuf);
	lbuf[5]=api->hclr|(api->bclr<<4);
	lbuf[11]=api->hclr|(api->bclr<<4);
	for(j=2;j<14;j+=2)
		lbuf[j]='\xcd';
	lbuf[0]='\xc8';
	lbuf[(width-1)*2]='\xbc';
	puttext(SCRN_LEFT,SCRN_TOP+height-1
		,SCRN_LEFT+width-1,SCRN_TOP+height-1,lbuf);
	lbuf[0]='\xcc';
	lbuf[(width-1)*2]='\xb9';
	lbuf[width-1]='\xcb';
	puttext(SCRN_LEFT,SCRN_TOP+2,SCRN_LEFT+width-1,SCRN_TOP+2,lbuf);
	lbuf[width-1]='\xca';
	puttext(SCRN_LEFT,SCRN_TOP+3+listheight
		,SCRN_LEFT+width-1,SCRN_TOP+3+listheight,lbuf);
	lbuf[0]='\xba';
	lbuf[(width-1)*2]='\xba';
	for(j=2;j<(width-1)*2;j+=2)
		lbuf[j]=' ';
	puttext(SCRN_LEFT,SCRN_TOP+1,
		SCRN_LEFT+width-1,SCRN_TOP+1,lbuf);
	puttext(SCRN_LEFT,SCRN_TOP+height-2,
		SCRN_LEFT+width-1,SCRN_TOP+height-2,lbuf);
	puttext(SCRN_LEFT,SCRN_TOP+height-3,
		SCRN_LEFT+width-1,SCRN_TOP+height-3,lbuf);
	lbuf[width-1]='\xba';
	for(j=0;j<listheight;j++)
		puttext(SCRN_LEFT,SCRN_TOP+3+j
			,SCRN_LEFT+width-1,SCRN_TOP+3+j,lbuf);
	

	/* Shadow */
	if(api->bclr==BLUE) {
		gettext(SCRN_LEFT+width,SCRN_TOP+1,SCRN_LEFT+width+1
			,SCRN_TOP+(height-1),shade);
		for(j=1;j<1024;j+=2)
			shade[j]=DARKGRAY;
		puttext(SCRN_LEFT+width,SCRN_TOP+1,SCRN_LEFT+width+1
			,SCRN_TOP+(height-1),shade);
		gettext(SCRN_LEFT+2,SCRN_TOP+height,SCRN_LEFT+width+1
			,SCRN_TOP+height,shade);
		for(j=1;j<width*2;j+=2)
			shade[j]=DARKGRAY;
		puttext(SCRN_LEFT+2,SCRN_TOP+height,SCRN_LEFT+width+1
			,SCRN_TOP+height,shade);
	}
}

void free_opt_list(char **opts)
{
	char **p;

	for(p=opts; *p[0]; p++) {
		free(*p);
	}
	free(opts);
}

char **get_file_opt_list(char **fns, int files, int dirsonly, int root)
{
	char **opts;
	char *opt;
	char *p;
	int  i;
	int  j=0;

	opts=(char **)malloc((files+2)*sizeof(char *));
	if(opts==NULL)
		return(NULL);
	if(dirsonly) {
		if(!root)
			opts[j++]=strdup("..");
	}
	for(i=0;i<files;i++) {
		if(isdir(fns[i])) {
			if(dirsonly)
				opts[j++]=strdup(getfname(fns[i]));
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
	int i,j;
	char *p;
	int width;
	int height;

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
#endif

	api->printf(SCRN_LEFT+2, SCRN_TOP+height-2, api->lclr|(api->bclr<<4), "%-*s", width, dpath);
}

int filepick(uifcapi_t *api, char *title, struct file_pick *fp, char *dir, char *msk, int opts)
{
	char	cfile[MAX_PATH*5+1];
	char	cpath[MAX_PATH+1];
	char	cdrive[3];
	char	cdir[MAX_PATH+1];
	char	cfname[MAX_PATH+1];
	char	cext[MAX_PATH+1];
	char	cname[MAX_PATH+1];
	char	cmsk[MAX_PATH*4+1];
	char	cglob[MAX_PATH*4+1];
	char	dglob[MAX_PATH*4+1];
	char	*p1;
	char	*p2;
	glob_t	fgl;
	glob_t	dgl;
	int		dircur=0;
	int		dirbar=0;
	int		filecur=0;
	int		filebar=0;
	int		listwidth;
	int	listheight;
	char	**dir_list;
	char	**file_list;
	int		currfield=DIR_LIST;
	int		lastfield=DIR_LIST;
	int		i;
	int		root=0;
	int		reread=FALSE;
	int		lbclr;
	char	*lastpath=NULL;
	char	*tmplastpath=NULL;
	char	*tmppath=NULL;
	int width;
	int height;

	height=api->scrn_len-3;
	width=SCRN_RIGHT-SCRN_LEFT-3;

	lbclr=api->lbclr;

	/* No struct passed */
	if(fp==NULL)
		return(-1);

	/* Illegal options */
	if((opts & UIFC_FP_MULTI) && (opts & (UIFC_FP_ALLOWENTRY|UIFC_FP_OVERPROMPT|UIFC_FP_CREATPROMPT)))
		return(-1);

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
	listwidth=SCRN_RIGHT-SCRN_LEFT+1;
	listwidth-=listwidth%2;
	listwidth-=3;
	listwidth/=2;
	/* Draw the file picker itself... */
	drawfpwindow(api);
	/* Display the title centered */
	i=strlen(title);
	if(i>width-4)
		i=width-4;
	api->printf(SCRN_LEFT+2, SCRN_TOP+1, api->hclr|(api->bclr<<4), "%*s%-*s", (width-i)/2-2, "", i, title);
	api->printf(SCRN_LEFT+2, SCRN_TOP+height-3, api->hclr|(api->bclr<<4), "Mask: ");
	api->printf(SCRN_LEFT+8, SCRN_TOP+height-3, api->lclr|(api->bclr<<4), "%-*s", width-7, msk);
	while(1) {
		tmppath=strdup(cpath);
		if(tmppath != NULL)
			FULLPATH(cpath,tmppath,sizeof(cpath));
		FREE_AND_NULL(tmppath);

		backslash(cpath);
		sprintf(cglob,"%s%s",cpath,cmsk);
		sprintf(dglob,"%s*",cpath,cmsk);
		switch(currfield) {
			case DIR_LIST:
				if(lastfield==DIR_LIST)
					sprintf(cfile,"%s%s",cpath,msk);
				break;
		}

#ifdef __unix__
		if(cpath[1]==0)
			root=TRUE;
		else
			root=FALSE;
#else
#error Need to do something about root paths (in get_file_opt_list() too!)
#endif
		if(glob(dglob, 0, NULL, &dgl)!=0) {
			if(lastpath==NULL)
				return(-1);
			api->msg("Cannot read directory!");
			SAFECOPY(cpath, lastpath);
			FREE_AND_NULL(lastpath);
			currfield=lastfield;
			continue;
		}
		if(glob(cglob, 0, NULL, &fgl)!=0)
			fgl.gl_pathc=0;
		api->list_height=api->scrn_len-3-7;
		dir_list=get_file_opt_list(dgl.gl_pathv, dgl.gl_pathc, TRUE, root);
		file_list=get_file_opt_list(fgl.gl_pathv, fgl.gl_pathc, FALSE, root);
		globfree(&dgl);
		globfree(&fgl);
		reread=FALSE;
		dircur=dirbar=filecur=filecur=0;
		while(!reread) {
			display_current_path(api, cfile);
			api->lbclr=api->lclr|(api->bclr<<4);
			api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_IMM|WIN_REDRAW,1,3,listwidth,&dircur,&dirbar,NULL,dir_list);
			api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_IMM|WIN_REDRAW,1+listwidth+1,3,listwidth,&filecur,&filebar,NULL,file_list);
			api->lbclr=lbclr;
			lastfield=currfield;
			switch(currfield) {
				case DIR_LIST:
					i=api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_EXTKEYS,1,3,listwidth,&dircur,&dirbar,NULL,dir_list);
					if(i==-2-'\t')
						currfield++;
					if(i>=0) {
						FREE_AND_NULL(lastpath);
						lastpath=strdup(cpath);
						strcat(cpath,dir_list[i]);
						reread=TRUE;
						sprintf(cfile,"%s%s",cpath,msk);
					}
					break;
				case FILE_LIST:
					i=api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT|WIN_EXTKEYS,1+listwidth+1,3,listwidth,&filecur,&filebar,NULL,file_list);
					if(i>=0)
						sprintf(cfile,"%s%s",cpath,file_list[i]);
					if(i==-2-'\t')	/* This is only until the text fields are here */
						currfield++;
					break;
				case CURRENT_PATH:
					FREE_AND_NULL(tmplastpath);
					tmplastpath=strdup(cpath);
					api->getstrxy(SCRN_LEFT+2, SCRN_TOP+height-2, width-1, cfile, sizeof(cfile)-1, K_EDIT|K_TABEXIT, &i);
					_splitpath(cfile, cdrive, cdir, cfname, cext);
					sprintf(cpath,"%s%s",cdrive,cdir);
					sprintf(cfile,"%s%s%s%s",cdrive,cdir,cfname,cext);
					if(isdir(cfile)) {
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
					currfield++;
					break;
				case MASK_FIELD:
					p1=strdup(cmsk);
					api->getstrxy(SCRN_LEFT+8, SCRN_TOP+height-3, width-7, cmsk, sizeof(cmsk)-1, K_EDIT|K_TABEXIT, &i);
					if(strcmp(cmsk, p1)) {
						sprintf(cfile,"%s%s",cpath,cmsk);
						reread=TRUE;
					}
					FREE_AND_NULL(p1);
					currfield++;
					break;
			}
			if(currfield==MASK_FIELD && (opts & UIFC_FP_MSKNOCHG))
				currfield++;
			if(currfield==FIELD_LIST_TERM)
				currfield=DIR_LIST;
		}
	}

	FREE_AND_NULL(lastpath);
	return(0);
}

int filepick_free(struct file_pick *fp)
{
	return(0);
}
