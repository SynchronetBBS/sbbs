#line 1 "SCFGLIB2.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "scfglib.h"

#ifdef SCFG 	/* SCFG allocate max length */
	#define readline_alloc(l,s,m,i) readline(l,s,m,i)
#else
	char *readline_alloc(long *offset, char *outstr, int maxline
		, FILE *instream);
	#define readline_alloc(l,s,m,i) s=readline_alloc(l,s,m,i)
	#define get_alloc(o,s,l,i) s=get_alloc(o,s,l,i)
#endif


#ifndef NO_FILE_CFG

/****************************************************************************/
/* Reads in LIBS.CNF and initializes the associated variables				*/
/****************************************************************************/
void read_file_cfg(read_cfg_text_t txt)
{
	char	str[256],fname[13],c,cmd[LEN_CMD+1];
	int 	file;
	short	i,j,k,l,n;
	long	offset=0,t;
	FILE	*instream;

#ifndef SCFG

sprintf(data_dir_dirs,"%sDIRS\\",data_dir);

#endif

strcpy(fname,"FILE.CNF");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
	bail(1); }

lprintf(txt.reading,fname);

get_int(min_dspace,instream);
get_int(max_batup,instream);

#ifdef SBBS
if(max_batup) {
    if((batup_desc=(char **)MALLOC(sizeof(char *)*max_batup))==NULL)
        allocerr(txt,offset,fname,sizeof(char *)*max_batup);
    if((batup_name=(char **)MALLOC(sizeof(char *)*max_batup))==NULL)
        allocerr(txt,offset,fname,sizeof(char *)*max_batup);
    if((batup_misc=(char *)MALLOC(sizeof(char)*max_batup))==NULL)
        allocerr(txt,offset,fname,sizeof(char)*max_batup);
    if((batup_dir=(uint *)MALLOC(sizeof(uint)*max_batup))==NULL)
        allocerr(txt,offset,fname,sizeof(uint)*max_batup);
	if((batup_alt=(ushort *)MALLOC(sizeof(ushort)*max_batup))==NULL)
		allocerr(txt,offset,fname,sizeof(ushort)*max_batup);
    for(i=0;i<max_batup;i++) {
        if((batup_desc[i]=(char *)MALLOC(59))==NULL)
            allocerr(txt,offset,fname,59);
        if((batup_name[i]=(char *)MALLOC(13))==NULL)
            allocerr(txt,offset,fname,13); } }
#endif
get_int(max_batdn,instream);

#ifdef SBBS
if(max_batdn) {
    if((batdn_name=(char **)MALLOC(sizeof(char *)*max_batdn))==NULL)
        allocerr(txt,offset,fname,sizeof(char *)*max_batdn);
    if((batdn_dir=(uint *)MALLOC(sizeof(uint)*max_batdn))==NULL)
        allocerr(txt,offset,fname,sizeof(uint)*max_batdn);
    if((batdn_offset=(long *)MALLOC(sizeof(long)*max_batdn))==NULL)
        allocerr(txt,offset,fname,sizeof(long)*max_batdn);
    if((batdn_size=(ulong *)MALLOC(sizeof(ulong)*max_batdn))==NULL)
        allocerr(txt,offset,fname,sizeof(ulong)*max_batdn);
    if((batdn_cdt=(ulong *)MALLOC(sizeof(ulong)*max_batdn))==NULL)
        allocerr(txt,offset,fname,sizeof(ulong)*max_batdn);
	if((batdn_alt=(ushort *)MALLOC(sizeof(ushort)*max_batdn))==NULL)
		allocerr(txt,offset,fname,sizeof(ushort)*max_batdn);
    for(i=0;i<max_batdn;i++)
        if((batdn_name[i]=(char *)MALLOC(13))==NULL)
            allocerr(txt,offset,fname,13); }
#endif
get_int(max_userxfer,instream);

get_int(t,instream);	/* unused - was cdt_byte_value */
get_int(cdt_up_pct,instream);
get_int(cdt_dn_pct,instream);
get_int(t,instream);	/* unused - was temp_ext */
get_str(cmd,instream);	/* unused - was temp_cmd */
get_int(leech_pct,instream);
get_int(leech_sec,instream);

for(i=0;i<32;i++)
	get_int(n,instream);

/**************************/
/* Extractable File Types */
/**************************/

get_int(total_fextrs,instream);

if(total_fextrs) {
	if((fextr=(fextr_t **)MALLOC(sizeof(fextr_t *)*total_fextrs))==NULL)
		allocerr(txt,offset,fname,sizeof(fextr_t*)*total_fextrs); }
else
	fextr=NULL;

for(i=0; i<total_fextrs; i++) {
	if(feof(instream))
		break;
	if((fextr[i]=(fextr_t *)MALLOC(sizeof(fextr_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(fextr_t));
    memset(fextr[i],0,sizeof(fextr_t));
	get_str(fextr[i]->ext,instream);
	get_alloc(&offset,fextr[i]->cmd,LEN_CMD,instream);
#ifdef SCFG
	get_str(fextr[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	fextr[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
		get_int(n,instream);
	}
total_fextrs=i;

/***************************/
/* Compressable File Types */
/***************************/

get_int(total_fcomps,instream);

if(total_fcomps) {
	if((fcomp=(fcomp_t **)MALLOC(sizeof(fcomp_t *)*total_fcomps))==NULL)
		allocerr(txt,offset,fname,sizeof(fcomp_t*)*total_fcomps); }
else
	fcomp=NULL;

for(i=0; i<total_fcomps; i++) {
	if(feof(instream))
		break;
	if((fcomp[i]=(fcomp_t *)MALLOC(sizeof(fcomp_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(fcomp_t));
	memset(fcomp[i],0,sizeof(fcomp_t));
	get_str(fcomp[i]->ext,instream);
	get_alloc(&offset,fcomp[i]->cmd,LEN_CMD,instream);
#ifdef SCFG
	get_str(fcomp[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	fcomp[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
		get_int(n,instream);
	}
total_fcomps=i;

/***********************/
/* Viewable File Types */
/***********************/

get_int(total_fviews,instream);

if(total_fviews) {
	if((fview=(fview_t **)MALLOC(sizeof(fview_t *)*total_fviews))==NULL)
		allocerr(txt,offset,fname,sizeof(fview_t*)*total_fviews); }
else
	fview=NULL;

for(i=0; i<total_fviews; i++) {
    if(feof(instream)) break;
	if((fview[i]=(fview_t *)MALLOC(sizeof(fview_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(fview_t));
    memset(fview[i],0,sizeof(fview_t));
	get_str(fview[i]->ext,instream);
	get_alloc(&offset,fview[i]->cmd,LEN_CMD,instream);
#ifdef SCFG
	get_str(fview[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	fview[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
        get_int(n,instream);
    }
total_fviews=i;

/***********************/
/* Testable File Types */
/***********************/

get_int(total_ftests,instream);

if(total_ftests) {
	if((ftest=(ftest_t **)MALLOC(sizeof(ftest_t *)*total_ftests))==NULL)
		allocerr(txt,offset,fname,sizeof(ftest_t*)*total_ftests); }
else
	ftest=NULL;

for(i=0; i<total_ftests; i++) {
    if(feof(instream)) break;
    if((ftest[i]=(ftest_t *)MALLOC(sizeof(ftest_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(ftest_t));
    memset(ftest[i],0,sizeof(ftest_t));
	get_str(ftest[i]->ext,instream);
	get_alloc(&offset,ftest[i]->cmd,LEN_CMD,instream);
	get_alloc(&offset,ftest[i]->workstr,40,instream);
#ifdef SCFG
	get_str(ftest[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	ftest[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
        get_int(n,instream);
    }
total_ftests=i;

/*******************/
/* Download events */
/*******************/

get_int(total_dlevents,instream);

if(total_dlevents) {
	if((dlevent=(dlevent_t **)MALLOC(sizeof(dlevent_t *)*total_dlevents))
		==NULL)
		allocerr(txt,offset,fname,sizeof(dlevent_t*)*total_dlevents); }
else
	dlevent=NULL;

for(i=0; i<total_dlevents; i++) {
    if(feof(instream)) break;
	if((dlevent[i]=(dlevent_t *)MALLOC(sizeof(dlevent_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(dlevent_t));
	memset(dlevent[i],0,sizeof(dlevent_t));
	get_str(dlevent[i]->ext,instream);
	get_alloc(&offset,dlevent[i]->cmd,LEN_CMD,instream);
	get_alloc(&offset,dlevent[i]->workstr,40,instream);
#ifdef SCFG
	get_str(dlevent[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	dlevent[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
        get_int(n,instream);
    }
total_dlevents=i;


/***************************/
/* File Transfer Protocols */
/***************************/

get_int(total_prots,instream);

if(total_prots) {
	if((prot=(prot_t **)MALLOC(sizeof(prot_t *)*total_prots))==NULL)
		allocerr(txt,offset,fname,sizeof(prot_t*)*total_prots); }
else
	prot=NULL;

for(i=0;i<total_prots;i++) {
    if(feof(instream)) break;
	if((prot[i]=(prot_t *)MALLOC(sizeof(prot_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(prot_t));
    memset(prot[i],0,sizeof(prot_t));

	get_int(prot[i]->mnemonic,instream);
	get_alloc(&offset,prot[i]->name,25,instream);
	get_alloc(&offset,prot[i]->ulcmd,LEN_CMD,instream);
	get_alloc(&offset,prot[i]->dlcmd,LEN_CMD,instream);
	get_alloc(&offset,prot[i]->batulcmd,LEN_CMD,instream);
	get_alloc(&offset,prot[i]->batdlcmd,LEN_CMD,instream);
	get_alloc(&offset,prot[i]->blindcmd,LEN_CMD,instream);
	get_alloc(&offset,prot[i]->bicmd,LEN_CMD,instream);
	get_int(prot[i]->misc,instream);
#ifdef SCFG
	get_str(prot[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	prot[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
        get_int(n,instream);
	}

/************************/
/* Alternate File Paths */
/************************/

get_int(altpaths,instream);

if(altpaths) {
	if((altpath=(char **)MALLOC(sizeof(char *)*altpaths))==NULL)
		allocerr(txt,offset,fname,sizeof(char *)*altpaths); }
else
	altpath=NULL;

for(i=0;i<altpaths;i++) {
    if(feof(instream)) break;
	fread(str,LEN_DIR+1,1,instream);
	offset+=LEN_DIR+1;
    backslash(str);
#ifdef SCFG
    j=LEN_DIR+1;
#else
    j=strlen(str)+1;
#endif
	if((altpath[i]=(char *)MALLOC(j))==NULL)
        allocerr(txt,offset,fname,j);
	memset(altpath[i],0,j);
	strcpy(altpath[i],str);
	for(j=0;j<8;j++)
        get_int(n,instream);
	}

altpaths=i;

/******************/
/* File Libraries */
/******************/

get_int(total_libs,instream);

if(total_libs) {
	if((lib=(lib_t **)MALLOC(sizeof(lib_t *)*total_libs))==NULL)
		allocerr(txt,offset,fname,sizeof(lib_t *)*total_libs); }
else
	lib=NULL;

#ifdef SBBS

if(total_libs) {
	if((curdir=(uint *)MALLOC(sizeof(uint)*total_libs))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*total_libs);

	if((usrlib=(uint *)MALLOC(sizeof(uint)*total_libs))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*total_libs);

	if((usrdirs=(uint *)MALLOC(sizeof(uint)*total_libs))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*total_libs);

	if((usrdir=(uint **)MALLOC(sizeof(uint *)*total_libs))==NULL)
		allocerr(txt,offset,fname,sizeof(uint *)*total_libs); }

#endif
for(i=0;i<total_libs;i++) {
	if(feof(instream)) break;
	if((lib[i]=(lib_t *)MALLOC(sizeof(lib_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(lib_t));
    memset(lib[i],0,sizeof(lib_t));
	lib[i]->offline_dir=INVALID_DIR;

	get_alloc(&offset,lib[i]->lname,LEN_GLNAME,instream);
	get_alloc(&offset,lib[i]->sname,LEN_GSNAME,instream);

#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
	if(!strcmp(lib[i]->lname,lib[i]->sname) && lib[i]->sname!=scfgnulstr) {
		FREE(lib[i]->sname);
		lib[i]->sname=lib[i]->lname; }
#endif

#ifdef SCFG
	get_str(lib[i]->ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	lib[i]->ar=arstr(0,str);
#endif
	for(j=0;j<48;j++)
		get_int(n,instream);
	}
total_libs=i;

/********************/
/* File Directories */
/********************/

sysop_dir=user_dir=upload_dir=INVALID_DIR;
get_int(total_dirs,instream);

if(total_dirs) {
	if((dir=(dir_t **)MALLOC(sizeof(dir_t *)*(total_dirs+1)))==NULL)
		allocerr(txt,offset,fname,sizeof(dir_t *)*(total_dirs+1)); }
else
	dir=NULL;

for(i=0;i<total_dirs;i++) {
	if(feof(instream)) break;
	if((dir[i]=(dir_t *)MALLOC(sizeof(dir_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(dir_t));
	memset(dir[i],0,sizeof(dir_t));

	get_int(dir[i]->lib,instream);
	get_alloc(&offset,dir[i]->lname,LEN_SLNAME,instream);
	get_alloc(&offset,dir[i]->sname,LEN_SSNAME,instream);

#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
	if(!strcmp(dir[i]->lname,dir[i]->sname) && dir[i]->sname!=scfgnulstr) {
		FREE(dir[i]->sname);
		dir[i]->sname=dir[i]->lname; }
#endif

	if(!strcmpi(dir[i]->sname,"SYSOP"))			/* Sysop upload directory */
		sysop_dir=i;
	else if(!strcmpi(dir[i]->sname,"USER"))		/* User to User xfer dir */
		user_dir=i;
	else if(!strcmpi(dir[i]->sname,"UPLOADS"))  /* Upload directory */
		upload_dir=i;
	else if(!strcmpi(dir[i]->sname,"OFFLINE"))	/* Offline files dir */
		lib[dir[i]->lib]->offline_dir=i;

	get_str(dir[i]->code,instream);

#ifdef SCFG
	get_str(dir[i]->data_dir,instream);
#else
	fread(str,LEN_DIR+1,1,instream);   /* substitute data dir */
	offset+=LEN_DIR+1;
	if(str[0]) {
		backslash(str);
		if((dir[i]->data_dir=(char *)MALLOC(strlen(str)+1))==NULL)
			allocerr(txt,offset,fname,strlen(str)+1);
		strcpy(dir[i]->data_dir,str); }
	else
		dir[i]->data_dir=data_dir_dirs;
#endif

#ifdef SCFG
	get_str(dir[i]->ar,instream);
	get_str(dir[i]->ul_ar,instream);
	get_str(dir[i]->dl_ar,instream);
	get_str(dir[i]->op_ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	dir[i]->ar=arstr(0,str);
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	dir[i]->ul_ar=arstr(0,str);
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	dir[i]->dl_ar=arstr(0,str);
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	dir[i]->op_ar=arstr(0,str);
#endif
	fread(str,1,LEN_DIR+1,instream);
	offset+=LEN_DIR+1;
	if(!str[0]) 				   /* no path specified */
		sprintf(str,"%sDIRS\\%s\\",data_dir,dir[i]->code);

#ifndef SCFG
	if((dir[i]->path=(char *)MALLOC(strlen(str)+1))==NULL)
		allocerr(txt,offset,fname,strlen(str)+1);
#endif
	strcpy(dir[i]->path,str);
	get_alloc(&offset,dir[i]->upload_sem,LEN_DIR,instream);
	get_int(dir[i]->maxfiles,instream);
	if(dir[i]->maxfiles>MAX_FILES)
		dir[i]->maxfiles=MAX_FILES;
	get_alloc(&offset,dir[i]->exts,40,instream);
	get_int(dir[i]->misc,instream);
	get_int(dir[i]->seqdev,instream);
	get_int(dir[i]->sort,instream);
#ifdef SCFG
	get_str(dir[i]->ex_ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	dir[i]->ex_ar=arstr(0,str);
#endif
	get_int(dir[i]->maxage,instream);
	get_int(dir[i]->up_pct,instream);
	get_int(dir[i]->dn_pct,instream);
	get_int(c,instream);
	for(j=0;j<24;j++)
		get_int(n,instream); }

total_dirs=i;

#ifndef NO_TEXT_CFG 		/* This must be the last section in CFG file */

/**********************/
/* Text File Sections */
/**********************/

get_int(total_txtsecs,instream);


if(total_txtsecs) {
	if((txtsec=(txtsec_t **)MALLOC(sizeof(txtsec_t *)*total_txtsecs))==NULL)
		allocerr(txt,offset,fname,sizeof(txtsec_t *)*total_txtsecs); }
else
	txtsec=NULL;

for(i=0;i<total_txtsecs;i++) {
    if(feof(instream)) break;
    if((txtsec[i]=(txtsec_t *)MALLOC(sizeof(txtsec_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(txtsec_t));
	memset(txtsec[i],0,sizeof(txtsec_t));

    get_alloc(&offset,txtsec[i]->name,40,instream);
    get_str(txtsec[i]->code,instream);
#ifdef SCFG
    get_str(txtsec[i]->ar,instream);
#else
    fread(str,LEN_ARSTR+1,1,instream);
    offset+=LEN_ARSTR+1;
	txtsec[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
		get_int(n,instream);
	}
total_txtsecs=i;

#endif

fclose(instream);
lprintf(txt.readit,fname);
#ifdef SBBS
for(i=l=0;i<total_libs;i++) {
	for(j=k=0;j<total_dirs;j++)
		if(dir[j]->lib==i)
			k++;
	if(k>l) l=k; }	/* l = largest number of dirs in a lib */
for(i=0;i<total_libs;i++)
	if(l && (usrdir[i]=(uint *)MALLOC(sizeof(uint)*l))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*l);
#endif
}

#endif


#ifndef NO_XTRN_CFG

/****************************************************************************/
/* Reads in XTRN.CNF and initializes the associated variables				*/
/****************************************************************************/
void read_xtrn_cfg(read_cfg_text_t txt)
{
	char	str[256],fname[13],*p,c;
	int 	file;
	short	i,j,n;
	long	offset=0;
	FILE	*instream;

strcpy(fname,"XTRN.CNF");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
	bail(1); }

lprintf(txt.reading,fname);

/*************/
/* Swap list */
/*************/

get_int(total_swaps,instream);

if(total_swaps) {
	if((swap=(swap_t **)MALLOC(sizeof(swap_t *)*total_swaps))==NULL)
		allocerr(txt,offset,fname,sizeof(swap_t *)*total_swaps); }
else
	swap=NULL;

for(i=0;i<total_swaps;i++) {
	if(feof(instream)) break;
	if((swap[i]=(swap_t *)MALLOC(sizeof(swap_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(swap_t));
	get_alloc(&offset,swap[i]->cmd,LEN_CMD,instream); }
total_swaps=i;

/********************/
/* External Editors */
/********************/

get_int(total_xedits,instream);

if(total_xedits) {
	if((xedit=(xedit_t **)MALLOC(sizeof(xedit_t *)*total_xedits))==NULL)
		allocerr(txt,offset,fname,sizeof(xedit_t *)*total_xedits); }
else
	xedit=NULL;

for(i=0;i<total_xedits;i++) {
	if(feof(instream)) break;
	if((xedit[i]=(xedit_t *)MALLOC(sizeof(xedit_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(xedit_t));
	memset(xedit[i],0,sizeof(xedit_t));

	get_alloc(&offset,xedit[i]->name,40,instream);
	get_str(xedit[i]->code,instream);
	get_alloc(&offset,xedit[i]->lcmd,LEN_CMD,instream);
	get_alloc(&offset,xedit[i]->rcmd,LEN_CMD,instream);

#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
	if(!strcmp(xedit[i]->lcmd,xedit[i]->rcmd) && xedit[i]->rcmd!=scfgnulstr) {
		FREE(xedit[i]->rcmd);
		xedit[i]->rcmd=xedit[i]->lcmd; }
#endif

	get_int(xedit[i]->misc,instream);
#ifdef SCFG
	get_str(xedit[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	xedit[i]->ar=arstr(0,str);
#endif
	get_int(xedit[i]->type,instream);
	get_int(c,instream);
	for(j=0;j<7;j++)
		get_int(n,instream);
	}
total_xedits=i;


/*****************************/
/* External Program Sections */
/*****************************/

get_int(total_xtrnsecs,instream);

if(total_xtrnsecs) {
	if((xtrnsec=(xtrnsec_t **)MALLOC(sizeof(xtrnsec_t *)*total_xtrnsecs))
		==NULL)
		allocerr(txt,offset,fname,sizeof(xtrnsec_t *)*total_xtrnsecs); }
else
	xtrnsec=NULL;

for(i=0;i<total_xtrnsecs;i++) {
	if(feof(instream)) break;
	if((xtrnsec[i]=(xtrnsec_t *)MALLOC(sizeof(xtrnsec_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(xtrnsec_t));
	memset(xtrnsec[i],0,sizeof(xtrnsec_t));

	get_alloc(&offset,xtrnsec[i]->name,40,instream);
	get_str(xtrnsec[i]->code,instream);
#ifdef SCFG
	get_str(xtrnsec[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	xtrnsec[i]->ar=arstr(0,str);
#endif
	for(j=0;j<8;j++)
        get_int(n,instream);
	}
total_xtrnsecs=i;


/*********************/
/* External Programs */
/*********************/

get_int(total_xtrns,instream);

if(total_xtrns) {
	if((xtrn=(xtrn_t **)MALLOC(sizeof(xtrn_t *)*total_xtrns))==NULL)
		allocerr(txt,offset,fname,sizeof(xtrn_t *)*total_xtrns); }
else
	xtrn=NULL;

for(i=0;i<total_xtrns;i++) {
	if(feof(instream)) break;
	if((xtrn[i]=(xtrn_t *)MALLOC(sizeof(xtrn_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(xtrn_t));
	memset(xtrn[i],0,sizeof(xtrn_t));

	get_int(xtrn[i]->sec,instream);
	get_alloc(&offset,xtrn[i]->name,40,instream);
	get_str(xtrn[i]->code,instream);
#ifdef SCFG
	get_str(xtrn[i]->ar,instream);
	get_str(xtrn[i]->run_ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	xtrn[i]->ar=arstr(0,str);
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	xtrn[i]->run_ar=arstr(0,str);
#endif
	get_int(xtrn[i]->type,instream);
	get_int(xtrn[i]->misc,instream);
	get_int(xtrn[i]->event,instream);
	get_int(xtrn[i]->cost,instream);
	get_alloc(&offset,xtrn[i]->cmd,LEN_CMD,instream);
	get_alloc(&offset,xtrn[i]->clean,LEN_CMD,instream);
	fread(str,LEN_DIR+1,1,instream);
	offset+=LEN_DIR+1;
	if(strcmp(str+1,":\\") && str[strlen(str)-1]=='\\') // C:\ is valid
		str[strlen(str)-1]=0;		// C:\SBBS\XTRN\GAME\ is not
#ifdef SBBS
	if(!str[0]) 			  /* Minimum path of '.' */
		strcpy(str,".");
	if((xtrn[i]->path=(char *)MALLOC(strlen(str)+1))==NULL)
		allocerr(txt,offset,fname,strlen(str)+1);
#endif
	strcpy(xtrn[i]->path,str);
	get_int(xtrn[i]->textra,instream);
	get_int(xtrn[i]->maxtime,instream);
	for(j=0;j<7;j++)
        get_int(n,instream);
	}
total_xtrns=i;


/****************/
/* Timed Events */
/****************/

get_int(total_events,instream);

if(total_events) {
	if((event=(event_t **)MALLOC(sizeof(event_t *)*total_events))==NULL)
		allocerr(txt,offset,fname,sizeof(event_t *)*total_events); }
else
	event=NULL;

for(i=0;i<total_events;i++) {
	if(feof(instream)) break;
	if((event[i]=(event_t *)MALLOC(sizeof(event_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(event_t));
	memset(event[i],0,sizeof(event_t));

	get_str(event[i]->code,instream);
	get_alloc(&offset,event[i]->cmd,LEN_CMD,instream);
	get_int(event[i]->days,instream);
	get_int(event[i]->time,instream);
	get_int(event[i]->node,instream);
	get_int(event[i]->misc,instream);
	get_alloc(&offset,event[i]->dir,LEN_DIR,instream);
	if(event[i]->dir[0] 									// blank is valid
		&& strcmp(event[i]->dir+1,":\\")                    // C:\ is valid
		&& event[i]->dir[strlen(event[i]->dir)-1]=='\\')    // C:\DIR\ !valid
		event[i]->dir[strlen(event[i]->dir)-1]=0;			// Remove \
	for(j=0;j<8;j++)
        get_int(n,instream);
	}
total_events=i;

#if defined(SCFG) || defined(__OS2__)

/********************/
/* DOS Program list */
/********************/

get_int(total_os2pgms,instream);

if(total_os2pgms) {
	if((os2pgm=(os2pgm_t **)MALLOC(sizeof(os2pgm_t *)*total_os2pgms))==NULL)
		allocerr(txt,offset,fname,sizeof(os2pgm_t *)*total_os2pgms); }
else
	os2pgm=NULL;

for(i=0;i<total_os2pgms;i++) {
	if(feof(instream)) break;
	if((os2pgm[i]=(os2pgm_t *)MALLOC(sizeof(os2pgm_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(os2pgm_t));
	get_alloc(&offset,os2pgm[i]->name,12,instream);
	os2pgm[i]->misc=0; }
total_os2pgms=i;
for(i=0;i<total_os2pgms;i++) {
    if(feof(instream)) break;
	get_int(os2pgm[i]->misc,instream); }

#endif	// Don't add anything non-OS2 specific after this (without moding ^^ )


fclose(instream);
lprintf(txt.readit,fname);
}

#endif


#ifndef NO_CHAT_CFG

/****************************************************************************/
/* Reads in CHAT.CNF and initializes the associated variables				*/
/****************************************************************************/
void read_chat_cfg(read_cfg_text_t txt)
{
	char	str[256],fname[13],*p;
	int 	file;
	short	i,j,n;
	long	offset=0;
	FILE	*instream;

strcpy(fname,"CHAT.CNF");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
	bail(1); }

lprintf(txt.reading,fname);

/*********/
/* Gurus */
/*********/

get_int(total_gurus,instream);

if(total_gurus) {
    if((guru=(guru_t **)MALLOC(sizeof(guru_t *)*total_gurus))==NULL)
        allocerr(txt,offset,fname,sizeof(guru_t *)*total_gurus); }
else
    guru=NULL;

for(i=0;i<total_gurus;i++) {
    if(feof(instream)) break;
    if((guru[i]=(guru_t *)MALLOC(sizeof(guru_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(guru_t));
    memset(guru[i],0,sizeof(guru_t));

    get_alloc(&offset,guru[i]->name,25,instream);
    get_str(guru[i]->code,instream);

#ifdef SCFG
    get_str(guru[i]->ar,instream);
#else
    fread(str,LEN_ARSTR+1,1,instream);
    offset+=LEN_ARSTR+1;
	guru[i]->ar=arstr(0,str);
#endif
    for(j=0;j<8;j++)
		get_int(n,instream);
	}
total_chans=i;


/********************/
/* Chat Action Sets */
/********************/

get_int(total_actsets,instream);

if(total_actsets) {
	if((actset=(actset_t **)MALLOC(sizeof(actset_t *)*total_actsets))==NULL)
		allocerr(txt,offset,fname,sizeof(actset_t *)*total_actsets); }
else
	actset=NULL;

for(i=0;i<total_actsets;i++) {
	if(feof(instream)) break;
	if((actset[i]=(actset_t *)MALLOC(sizeof(actset_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(actset_t));
	get_alloc(&offset,actset[i]->name,25,instream);
	}
total_actsets=i;


/****************/
/* Chat Actions */
/****************/

get_int(total_chatacts,instream);

if(total_chatacts) {
	if((chatact=(chatact_t **)MALLOC(sizeof(chatact_t *)*total_chatacts))
		==NULL)
		allocerr(txt,offset,fname,sizeof(chatact_t *)*total_chatacts); }
else
	chatact=NULL;

for(i=0;i<total_chatacts;i++) {
	if(feof(instream)) break;
	if((chatact[i]=(chatact_t *)MALLOC(sizeof(chatact_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(chatact_t));
	memset(chatact[i],0,sizeof(chatact_t));

	get_int(chatact[i]->actset,instream);
	get_alloc(&offset,chatact[i]->cmd,LEN_CHATACTCMD,instream);
	get_alloc(&offset,chatact[i]->out,LEN_CHATACTOUT,instream);
	for(j=0;j<8;j++)
		get_int(n,instream);
	}

total_chatacts=i;


/***************************/
/* Multinode Chat Channels */
/***************************/

get_int(total_chans,instream);

if(total_chans) {
	if((chan=(chan_t **)MALLOC(sizeof(chan_t *)*total_chans))==NULL)
		allocerr(txt,offset,fname,sizeof(chan_t *)*total_chans); }
else
	chan=NULL;

for(i=0;i<total_chans;i++) {
	if(feof(instream)) break;
	if((chan[i]=(chan_t *)MALLOC(sizeof(chan_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(chan_t));
	memset(chan[i],0,sizeof(chan_t));

	get_int(chan[i]->actset,instream);
	get_alloc(&offset,chan[i]->name,25,instream);

	get_str(chan[i]->code,instream);

#ifdef SCFG
	get_str(chan[i]->ar,instream);
#else
	fread(str,LEN_ARSTR+1,1,instream);
	offset+=LEN_ARSTR+1;
	chan[i]->ar=arstr(0,str);
#endif
	get_int(chan[i]->cost,instream);
	get_int(chan[i]->guru,instream);
	get_int(chan[i]->misc,instream);
	for(j=0;j<8;j++)
		get_int(n,instream); }
total_chans=i;


/**************/
/* Chat Pages */
/**************/

get_int(total_pages,instream);

if(total_pages) {
    if((page=(page_t **)MALLOC(sizeof(page_t *)*total_pages))==NULL)
        allocerr(txt,offset,fname,sizeof(page_t *)*total_pages); }
else
    page=NULL;

for(i=0;i<total_pages;i++) {
    if(feof(instream)) break;
    if((page[i]=(page_t *)MALLOC(sizeof(page_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(page_t));
    memset(page[i],0,sizeof(page_t));

    get_alloc(&offset,page[i]->cmd,LEN_CMD,instream);

#ifdef SCFG
    get_str(page[i]->ar,instream);
#else
    fread(str,LEN_ARSTR+1,1,instream);
    offset+=LEN_ARSTR+1;
	page[i]->ar=arstr(0,str);
#endif
    get_int(page[i]->misc,instream);
    for(j=0;j<8;j++)
        get_int(n,instream);
    }
total_pages=i;


fclose(instream);
lprintf(txt.readit,fname);
}

#endif

/****************************************************************************/
/* Read one line of up two 256 characters from the file pointed to by       */
/* 'stream' and put upto 'maxlen' number of character into 'outstr' and     */
/* truncate spaces off end of 'outstr'.                                     */
/****************************************************************************/
char *readline(long *offset, char *outstr, int maxlen, FILE *instream)
{
	char str[257];

if(fgets(str,256,instream)==NULL)
	return(NULL);
sprintf(outstr,"%.*s",maxlen,str);
truncsp(outstr);
(*offset)+=maxlen;
return(outstr);
}

#undef readline_alloc

char *readline_alloc(long *offset, char *outstr, int maxline, FILE *instream)
{
	char str[257];

readline(offset,str,maxline,instream);
if((outstr=(char *)MALLOC(strlen(str)+1))==NULL)
	return(NULL);
strcpy(outstr,str);
return(outstr);
}

/****************************************************************************/
/* Turns char string of flags into a long									*/
/****************************************************************************/
long aftol(char *str)
{
	char c=0;
	ulong l=0UL;

strupr(str);
while(str[c]) {
	if(str[c]>='A' && str[c]<='Z')
		l|=FLAG(str[c]);
	c++; }
return(l);
}

/*****************************************************************************/
/* Converts a long into an ASCII Flag string (A-Z) that represents bits 0-25 */
/*****************************************************************************/
char *ltoaf(long l,char *str)
{
	char c=0;

while(c<26) {
	if(l&(long)(1L<<c))
		str[c]='A'+c;
	else str[c]=SP;
	c++; }
str[c]=0;
return(str);
}

/****************************************************************************/
/* Returns the actual attribute code from a string of ATTR characters       */
/****************************************************************************/
int attrstr(char *str)
{
	ulong l=0;
	short atr;

atr=LIGHTGRAY;
while(str[l]) {
	switch(str[l]) {
		case 'H': 	/* High intensity */
			atr|=HIGH;
			break;
		case 'I':	/* Blink */
			atr|=BLINK;
			break;
		case 'K':	/* Black */
			atr=(atr&0xf8)|BLACK;
			break;
		case 'W':	/* White */
			atr=(atr&0xf8)|LIGHTGRAY;
			break;
		case '0':	/* Black Background */
			atr=(atr&0x8f)|(BLACK<<4);
			break;
		case '7':	/* White Background */
			atr=(atr&0x8f)|(LIGHTGRAY<<4);
			break;
		case 'R':
			atr=(atr&0xf8)|RED;
			break;
		case 'G':
			atr=(atr&0xf8)|GREEN;
			break;
		case 'Y':   /* Yellow */
			atr=(atr&0xf8)|BROWN;
			break;
		case 'B':
			atr=(atr&0xf8)|BLUE;
			break;
		case 'M':
			atr=(atr&0xf8)|MAGENTA;
			break;
		case 'C':
			atr=(atr&0xf8)|CYAN;
			break;
		case '1':	/* Red Background */
			atr=(atr&0x8f)|(RED<<4);
			break;
		case '2':	/* Green Background */
			atr=(atr&0x8f)|(GREEN<<4);
			break;
		case '3':	/* Yellow Background */
			atr=(atr&0x8f)|(BROWN<<4);
			break;
		case '4':	/* Blue Background */
			atr=(atr&0x8f)|(BLUE<<4);
			break;
		case '5':	/* Magenta Background */
			atr=(atr&0x8f)|(MAGENTA<<4);
			break;
		case '6':	/* Cyan Background */
			atr=(atr&0x8f)|(CYAN<<4);
			break; }
	l++; }
return(atr);
}


