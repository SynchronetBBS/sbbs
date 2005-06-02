#include "dirwrap.h"
#include "uifc.h"
#include "ciolib.h"

#include "filepick.h"


int drawfpwindow(uifcapi_t *api)
{
	char lbuf[1024];
	int i;
	int j;
	int y;
	int listheight=0;
	int height;
	int width;
	char  hclr,lclr,bclr,cclr,lbclr;
	char  shade[1024];

	width=SCRN_RIGHT-SCRN_LEFT+1;
	if(!(api->mode&UIFC_COLOR) && (api->mode&UIFC_MONO))
        {
		bclr=BLACK;
		hclr=WHITE;
		lclr=LIGHTGRAY;
		cclr=LIGHTGRAY;
		lbclr=BLACK|(LIGHTGRAY<<4);     /* lightbar color */
	} else {
		bclr=BLUE;
		hclr=YELLOW;
		lclr=WHITE;
		cclr=CYAN;
		lbclr=BLUE|(LIGHTGRAY<<4);      /* lightbar color */
	}

	height=api->scrn_len-4;
	/* Make sure it's odd */
	if(!(width%2))
		width--;

	listheight=height-8;
	
        i=0;
	lbuf[i++]='\xc9';
	lbuf[i++]=hclr|(bclr<<4);
	for(j=1;j<width-1;j++) {
		lbuf[i++]='\xcd';
		lbuf[i++]=hclr|(bclr<<4);
	}
	if(api->mode&UIFC_MOUSE && width>6) {
		lbuf[2]='[';
		lbuf[3]=hclr|(bclr<<4);
		lbuf[4]=0xfe;
		lbuf[5]=lclr|(bclr<<4);
		lbuf[6]=']';
		lbuf[7]=hclr|(bclr<<4);
		lbuf[8]='[';
		lbuf[9]=hclr|(bclr<<4);
		lbuf[10]='?';
		lbuf[11]=lclr|(bclr<<4);
		lbuf[12]=']';
		lbuf[13]=hclr|(bclr<<4);
		api->buttony=SCRN_TOP;
		api->exitstart=SCRN_LEFT+1;
		api->exitend=SCRN_LEFT+3;
		api->helpstart=SCRN_LEFT+4;
		api->helpend=SCRN_LEFT+6;
	}
	lbuf[i++]='\xbb';
	lbuf[i]=hclr|(bclr<<4);
	puttext(SCRN_LEFT,SCRN_TOP,SCRN_LEFT+width-1,SCRN_TOP,lbuf);
	lbuf[5]=hclr|(bclr<<4);
	lbuf[11]=hclr|(bclr<<4);
	for(j=2;j<14;j+=2)
		lbuf[j]='\xcd';
	lbuf[0]='\xc8';
	lbuf[(width-1)*2]='\xbc';
	puttext(SCRN_LEFT,SCRN_TOP+height-1
		,SCRN_LEFT+width-1,SCRN_TOP+height-1,lbuf);
	lbuf[0]='\xcc';
	lbuf[(width-1)*2]='\xb9';
	lbuf[width+1]='\xcb';
	puttext(SCRN_LEFT,SCRN_TOP+2,SCRN_LEFT+width-1,SCRN_TOP+2,lbuf);
	lbuf[width+1]='\xca';
	puttext(SCRN_LEFT,SCRN_TOP+3+listheight
		,SCRN_LEFT+width-1,SCRN_TOP+3+listheight,lbuf);
	lbuf[0]='\xba';
	lbuf[(width-1)*2]='\xba';
	for(j=2;j<width*2;j+=2)
		lbuf[j]=' ';
	puttext(SCRN_LEFT,SCRN_TOP+1,
		SCRN_LEFT+width-1,SCRN_TOP+1,lbuf);
	puttext(SCRN_LEFT,SCRN_TOP+height-2,
		SCRN_LEFT+width-1,SCRN_TOP+height-2,lbuf);
	puttext(SCRN_LEFT,SCRN_TOP+height-3,
		SCRN_LEFT+width-1,SCRN_TOP+height-3,lbuf);
	puttext(SCRN_LEFT,SCRN_TOP+height-4,
		SCRN_LEFT+width-1,SCRN_TOP+height-4,lbuf);
	lbuf[width+1]='\xba';
	for(j=0;j<listheight;j++)
		puttext(SCRN_LEFT,SCRN_TOP+3+j
			,SCRN_LEFT+width-1,SCRN_TOP+3+j,lbuf);
	puttext(SCRN_LEFT,SCRN_TOP+3+listheight
		,SCRN_LEFT+width-1,SCRN_TOP+3+listheight,lbuf);
	

	/* Shadow */
	if(bclr==BLUE) {
		gettext(SCRN_LEFT+width,SCRN_TOP+1,SCRN_LEFT+width+1
			,SCRN_TOP+(height-1),shade);
		for(j=1;j<12;j+=2)
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

int filepick(uifcapi_t *api, char *title, struct file_pick *fp, char *dir, char *msk, int opts)
{
	char	cpath[MAX_PATH+1];
	char	cdir[MAX_PATH+1];
	char	cmsk[MAX_PATH*4+1];
	char	cglob[MAX_PATH*4+1];
	char	*p1;
	char	*p2;
	glob_t	fgl;
	glob_t	dgl;
	int		cur=0;
	int		bar=0;
	int		listwidth;
	int	listheight;

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
	p1=strrchr(cpath,'/');
#ifdef _WIN32
	p2=strrchr(cpath,'\\');
	if(p2>p1)
		p1=p2;
#endif
	if(p1!=NULL)
		SAFECOPY(cdir,p1);
	else
		SAFECOPY(cdir,"NONE");

	if(msk==NULL || msk[0]==0) {
		SAFECOPY(cmsk,"*");
	}
	else {
		SAFECOPY(cmsk,msk);
	}
	sprintf(cglob,"%s/%s",cpath,cmsk);

	listwidth=SCRN_RIGHT-SCRN_LEFT+1;
	listwidth-=listwidth%2;
	listwidth-=2;
	listwidth/=2;
	/* Draw the file picker itself... */

	while(1) {
		drawfpwindow(api);
		if(glob(cglob, 0, NULL, &fgl)!=0)
			return(-1);
		api->list_height=api->scrn_len-4-8;
		api->list(WIN_NOBRDR|WIN_FIXEDHEIGHT,1,3,listwidth,&cur,&bar,NULL,fgl.gl_pathv);
	}

	return(0);
}

int filepick_free(struct file_pick *fp)
{
	return(0);
}
