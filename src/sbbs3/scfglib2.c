/* scfglib2.c */

/* Synchronet configuration library routines */

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
#include "scfglib.h"

#ifdef SCFG 	/* SCFG allocate max length */
	#define readline_alloc(l,s,m,i) readline(l,s,m,i)
#else
	char *readline_alloc(long *offset, char *outstr, int maxline
		, FILE *instream);
	#define readline_alloc(l,s,m,i) s=readline_alloc(l,s,m,i)
	#define get_alloc(o,s,l,i) s=get_alloc(o,s,l,i)
#endif

extern const char * scfgnulstr;

#ifndef NO_FILE_CFG

/****************************************************************************/
/* Reads in LIBS.CNF and initializes the associated variables				*/
/****************************************************************************/
BOOL read_file_cfg(scfg_t* cfg, read_cfg_text_t* txt)
{
	char	str[256],fname[13],c,cmd[LEN_CMD+1];
	short	i,j,n;
	long	offset=0,t;
	FILE	*instream;

#ifndef SCFG

	sprintf(cfg->data_dir_dirs,"%sDIRS/",cfg->data_dir);

#endif

	strcpy(fname,"FILE.CNF");
	sprintf(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fopen(str,"rb" /*O_RDONLY*/))==NULL) {
		lprintf(txt->openerr,str);
		return(FALSE); }

	if(txt->reading && txt->reading[0])
		lprintf(txt->reading,fname);

	get_int(cfg->min_dspace,instream);
	get_int(cfg->max_batup,instream);

	get_int(cfg->max_batdn,instream);

	get_int(cfg->max_userxfer,instream);

	get_int(t,instream);	/* unused - was cdt_byte_value */
	get_int(cfg->cdt_up_pct,instream);
	get_int(cfg->cdt_dn_pct,instream);
	get_int(t,instream);	/* unused - was temp_ext */
	get_str(cmd,instream);	/* unused - was temp_cmd */
	get_int(cfg->leech_pct,instream);
	get_int(cfg->leech_sec,instream);

	for(i=0;i<32;i++)
		get_int(n,instream);

	/**************************/
	/* Extractable File Types */
	/**************************/

	get_int(cfg->total_fextrs,instream);

	if(cfg->total_fextrs) {
		if((cfg->fextr=(fextr_t **)MALLOC(sizeof(fextr_t *)*cfg->total_fextrs))==NULL)
			return allocerr(txt,offset,fname,sizeof(fextr_t*)*cfg->total_fextrs); }
	else
		cfg->fextr=NULL;

	for(i=0; i<cfg->total_fextrs; i++) {
		if(feof(instream))
			break;
		if((cfg->fextr[i]=(fextr_t *)MALLOC(sizeof(fextr_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(fextr_t));
		memset(cfg->fextr[i],0,sizeof(fextr_t));
		get_str(cfg->fextr[i]->ext,instream);
		get_alloc(&offset,cfg->fextr[i]->cmd,LEN_CMD,instream);
	#ifdef SCFG
		get_str(cfg->fextr[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->fextr[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_fextrs=i;

	/***************************/
	/* Compressable File Types */
	/***************************/

	get_int(cfg->total_fcomps,instream);

	if(cfg->total_fcomps) {
		if((cfg->fcomp=(fcomp_t **)MALLOC(sizeof(fcomp_t *)*cfg->total_fcomps))==NULL)
			return allocerr(txt,offset,fname,sizeof(fcomp_t*)*cfg->total_fcomps); }
	else
		cfg->fcomp=NULL;

	for(i=0; i<cfg->total_fcomps; i++) {
		if(feof(instream))
			break;
		if((cfg->fcomp[i]=(fcomp_t *)MALLOC(sizeof(fcomp_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(fcomp_t));
		memset(cfg->fcomp[i],0,sizeof(fcomp_t));
		get_str(cfg->fcomp[i]->ext,instream);
		get_alloc(&offset,cfg->fcomp[i]->cmd,LEN_CMD,instream);
	#ifdef SCFG
		get_str(cfg->fcomp[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->fcomp[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_fcomps=i;

	/***********************/
	/* Viewable File Types */
	/***********************/

	get_int(cfg->total_fviews,instream);

	if(cfg->total_fviews) {
		if((cfg->fview=(fview_t **)MALLOC(sizeof(fview_t *)*cfg->total_fviews))==NULL)
			return allocerr(txt,offset,fname,sizeof(fview_t*)*cfg->total_fviews); }
	else
		cfg->fview=NULL;

	for(i=0; i<cfg->total_fviews; i++) {
		if(feof(instream)) break;
		if((cfg->fview[i]=(fview_t *)MALLOC(sizeof(fview_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(fview_t));
		memset(cfg->fview[i],0,sizeof(fview_t));
		get_str(cfg->fview[i]->ext,instream);
		get_alloc(&offset,cfg->fview[i]->cmd,LEN_CMD,instream);
	#ifdef SCFG
		get_str(cfg->fview[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->fview[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_fviews=i;

	/***********************/
	/* Testable File Types */
	/***********************/

	get_int(cfg->total_ftests,instream);

	if(cfg->total_ftests) {
		if((cfg->ftest=(ftest_t **)MALLOC(sizeof(ftest_t *)*cfg->total_ftests))==NULL)
			return allocerr(txt,offset,fname,sizeof(ftest_t*)*cfg->total_ftests); }
	else
		cfg->ftest=NULL;

	for(i=0; i<cfg->total_ftests; i++) {
		if(feof(instream)) break;
		if((cfg->ftest[i]=(ftest_t *)MALLOC(sizeof(ftest_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(ftest_t));
		memset(cfg->ftest[i],0,sizeof(ftest_t));
		get_str(cfg->ftest[i]->ext,instream);
		get_alloc(&offset,cfg->ftest[i]->cmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->ftest[i]->workstr,40,instream);
	#ifdef SCFG
		get_str(cfg->ftest[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->ftest[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_ftests=i;

	/*******************/
	/* Download events */
	/*******************/

	get_int(cfg->total_dlevents,instream);

	if(cfg->total_dlevents) {
		if((cfg->dlevent=(dlevent_t **)MALLOC(sizeof(dlevent_t *)*cfg->total_dlevents))
			==NULL)
			return allocerr(txt,offset,fname,sizeof(dlevent_t*)*cfg->total_dlevents); }
	else
		cfg->dlevent=NULL;

	for(i=0; i<cfg->total_dlevents; i++) {
		if(feof(instream)) break;
		if((cfg->dlevent[i]=(dlevent_t *)MALLOC(sizeof(dlevent_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(dlevent_t));
		memset(cfg->dlevent[i],0,sizeof(dlevent_t));
		get_str(cfg->dlevent[i]->ext,instream);
		get_alloc(&offset,cfg->dlevent[i]->cmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->dlevent[i]->workstr,40,instream);
	#ifdef SCFG
		get_str(cfg->dlevent[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->dlevent[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_dlevents=i;


	/***************************/
	/* File Transfer Protocols */
	/***************************/

	get_int(cfg->total_prots,instream);

	if(cfg->total_prots) {
		if((cfg->prot=(prot_t **)MALLOC(sizeof(prot_t *)*cfg->total_prots))==NULL)
			return allocerr(txt,offset,fname,sizeof(prot_t*)*cfg->total_prots); }
	else
		cfg->prot=NULL;

	for(i=0;i<cfg->total_prots;i++) {
		if(feof(instream)) break;
		if((cfg->prot[i]=(prot_t *)MALLOC(sizeof(prot_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(prot_t));
		memset(cfg->prot[i],0,sizeof(prot_t));

		get_int(cfg->prot[i]->mnemonic,instream);
		get_alloc(&offset,cfg->prot[i]->name,25,instream);
		get_alloc(&offset,cfg->prot[i]->ulcmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->prot[i]->dlcmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->prot[i]->batulcmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->prot[i]->batdlcmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->prot[i]->blindcmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->prot[i]->bicmd,LEN_CMD,instream);
		get_int(cfg->prot[i]->misc,instream);
	#ifdef SCFG
		get_str(cfg->prot[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->prot[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}

	/************************/
	/* Alternate File Paths */
	/************************/

	get_int(cfg->altpaths,instream);

	if(cfg->altpaths) {
		if((cfg->altpath=(char **)MALLOC(sizeof(char *)*cfg->altpaths))==NULL)
			return allocerr(txt,offset,fname,sizeof(char *)*cfg->altpaths); }
	else
		cfg->altpath=NULL;

	for(i=0;i<cfg->altpaths;i++) {
		if(feof(instream)) break;
		fread(str,LEN_DIR+1,1,instream);
		offset+=LEN_DIR+1;
		backslash(str);
	#ifdef SCFG
		j=LEN_DIR+1;
	#else
		j=strlen(str)+1;
	#endif
		if((cfg->altpath[i]=(char *)MALLOC(j))==NULL)
			return allocerr(txt,offset,fname,j);
		memset(cfg->altpath[i],0,j);
		strcpy(cfg->altpath[i],str);
		for(j=0;j<8;j++)
			get_int(n,instream);
		}

	cfg->altpaths=i;

	/******************/
	/* File Libraries */
	/******************/

	get_int(cfg->total_libs,instream);

	if(cfg->total_libs) {
		if((cfg->lib=(lib_t **)MALLOC(sizeof(lib_t *)*cfg->total_libs))==NULL)
			return allocerr(txt,offset,fname,sizeof(lib_t *)*cfg->total_libs); }
	else
		cfg->lib=NULL;

	for(i=0;i<cfg->total_libs;i++) {
		if(feof(instream)) break;
		if((cfg->lib[i]=(lib_t *)MALLOC(sizeof(lib_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(lib_t));
		memset(cfg->lib[i],0,sizeof(lib_t));
		cfg->lib[i]->offline_dir=INVALID_DIR;

		get_alloc(&offset,cfg->lib[i]->lname,LEN_GLNAME,instream);
		get_alloc(&offset,cfg->lib[i]->sname,LEN_GSNAME,instream);

	#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
		if(!strcmp(cfg->lib[i]->lname,cfg->lib[i]->sname) && cfg->lib[i]->sname!=scfgnulstr) {
			FREE(cfg->lib[i]->sname);
			cfg->lib[i]->sname=cfg->lib[i]->lname; }
	#endif

	#ifdef SCFG
		get_str(cfg->lib[i]->ar,instream);
	#else
		fread(str,1,LEN_ARSTR+1,instream);
		offset+=LEN_ARSTR+1;
		cfg->lib[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<48;j++)
			get_int(n,instream);
		}
	cfg->total_libs=i;

	/********************/
	/* File Directories */
	/********************/

	cfg->sysop_dir=cfg->user_dir=cfg->upload_dir=INVALID_DIR;
	get_int(cfg->total_dirs,instream);

	if(cfg->total_dirs) {
		if((cfg->dir=(dir_t **)MALLOC(sizeof(dir_t *)*(cfg->total_dirs+1)))==NULL)
			return allocerr(txt,offset,fname,sizeof(dir_t *)*(cfg->total_dirs+1)); }
	else
		cfg->dir=NULL;

	for(i=0;i<cfg->total_dirs;i++) {
		if(feof(instream)) break;
		if((cfg->dir[i]=(dir_t *)MALLOC(sizeof(dir_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(dir_t));
		memset(cfg->dir[i],0,sizeof(dir_t));

		get_int(cfg->dir[i]->lib,instream);
		get_alloc(&offset,cfg->dir[i]->lname,LEN_SLNAME,instream);
		get_alloc(&offset,cfg->dir[i]->sname,LEN_SSNAME,instream);

	#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
		if(!strcmp(cfg->dir[i]->lname,cfg->dir[i]->sname) && cfg->dir[i]->sname!=scfgnulstr) {
			FREE(cfg->dir[i]->sname);
			cfg->dir[i]->sname=cfg->dir[i]->lname; }
	#endif

		if(!stricmp(cfg->dir[i]->sname,"SYSOP"))			/* Sysop upload directory */
			cfg->sysop_dir=i;
		else if(!stricmp(cfg->dir[i]->sname,"USER"))		/* User to User xfer dir */
			cfg->user_dir=i;
		else if(!stricmp(cfg->dir[i]->sname,"UPLOADS"))  /* Upload directory */
			cfg->upload_dir=i;
		else if(!stricmp(cfg->dir[i]->sname,"OFFLINE"))	/* Offline files dir */
			cfg->lib[cfg->dir[i]->lib]->offline_dir=i;

		get_str(cfg->dir[i]->code,instream);

	#ifdef SCFG
		get_str(cfg->dir[i]->data_dir,instream);
	#else
		fread(str,LEN_DIR+1,1,instream);   /* substitute data dir */
		offset+=LEN_DIR+1;
		if(str[0]) {
			prep_path(cfg->ctrl_dir, str);
			if((cfg->dir[i]->data_dir=(char *)MALLOC(strlen(str)+1))==NULL)
				return allocerr(txt,offset,fname,strlen(str)+1);
			strcpy(cfg->dir[i]->data_dir,str); }
		else
			cfg->dir[i]->data_dir=cfg->data_dir_dirs;
	#endif

	#ifdef SCFG
		get_str(cfg->dir[i]->ar,instream);
		get_str(cfg->dir[i]->ul_ar,instream);
		get_str(cfg->dir[i]->dl_ar,instream);
		get_str(cfg->dir[i]->op_ar,instream);
	#else
		fread(str,1,LEN_ARSTR+1,instream);
		offset+=LEN_ARSTR+1;
		cfg->dir[i]->ar=arstr(0,str,cfg);
		fread(str,1,LEN_ARSTR+1,instream);
		offset+=LEN_ARSTR+1;
		cfg->dir[i]->ul_ar=arstr(0,str,cfg);
		fread(str,1,LEN_ARSTR+1,instream);
		offset+=LEN_ARSTR+1;
		cfg->dir[i]->dl_ar=arstr(0,str,cfg);
		fread(str,1,LEN_ARSTR+1,instream);
		offset+=LEN_ARSTR+1;
		cfg->dir[i]->op_ar=arstr(0,str,cfg);
	#endif
		fread(str,1,LEN_DIR+1,instream);
		offset+=LEN_DIR+1;
		if(!str[0]) 				   /* no path specified */
			sprintf(str,"%sDIRS/%s/",cfg->data_dir,cfg->dir[i]->code);
		prep_path(cfg->ctrl_dir, str);

	#ifndef SCFG
		if((cfg->dir[i]->path=(char *)MALLOC(strlen(str)+1))==NULL)
			return allocerr(txt,offset,fname,strlen(str)+1);
	#endif
		strcpy(cfg->dir[i]->path,str);
		get_alloc(&offset,cfg->dir[i]->upload_sem,LEN_DIR,instream);
		get_int(cfg->dir[i]->maxfiles,instream);
		if(cfg->dir[i]->maxfiles>MAX_FILES)
			cfg->dir[i]->maxfiles=MAX_FILES;
		get_alloc(&offset,cfg->dir[i]->exts,40,instream);
		get_int(cfg->dir[i]->misc,instream);
		get_int(cfg->dir[i]->seqdev,instream);
		get_int(cfg->dir[i]->sort,instream);
	#ifdef SCFG
		get_str(cfg->dir[i]->ex_ar,instream);
	#else
		fread(str,1,LEN_ARSTR+1,instream);
		offset+=LEN_ARSTR+1;
		cfg->dir[i]->ex_ar=arstr(0,str,cfg);
	#endif
		get_int(cfg->dir[i]->maxage,instream);
		get_int(cfg->dir[i]->up_pct,instream);
		get_int(cfg->dir[i]->dn_pct,instream);
		get_int(c,instream);
		for(j=0;j<24;j++)
			get_int(n,instream); }

	cfg->total_dirs=i;

	#ifndef NO_TEXT_CFG 		/* This must be the last section in CFG file */

	/**********************/
	/* Text File Sections */
	/**********************/

	get_int(cfg->total_txtsecs,instream);


	if(cfg->total_txtsecs) {
		if((cfg->txtsec=(txtsec_t **)MALLOC(sizeof(txtsec_t *)*cfg->total_txtsecs))==NULL)
			return allocerr(txt,offset,fname,sizeof(txtsec_t *)*cfg->total_txtsecs); }
	else
		cfg->txtsec=NULL;

	for(i=0;i<cfg->total_txtsecs;i++) {
		if(feof(instream)) break;
		if((cfg->txtsec[i]=(txtsec_t *)MALLOC(sizeof(txtsec_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(txtsec_t));
		memset(cfg->txtsec[i],0,sizeof(txtsec_t));

		get_alloc(&offset,cfg->txtsec[i]->name,40,instream);
		get_str(cfg->txtsec[i]->code,instream);
	#ifdef SCFG
		get_str(cfg->txtsec[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->txtsec[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_txtsecs=i;

	#endif

	fclose(instream);
	if(txt->readit && txt->readit[0])
		lprintf(txt->readit,fname);

	return (TRUE);
}

#endif


#ifndef NO_XTRN_CFG

/****************************************************************************/
/* Reads in XTRN.CNF and initializes the associated variables				*/
/****************************************************************************/
BOOL read_xtrn_cfg(scfg_t* cfg, read_cfg_text_t* txt)
{
	char	str[256],fname[13],c;
	short	i,j,n;
	long	offset=0;
	FILE	*instream;

	strcpy(fname,"XTRN.CNF");
	sprintf(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fopen(str,"rb" /*O_RDONLY*/))==NULL) {
		lprintf(txt->openerr,str);
		return(FALSE); }

	if(txt->reading && txt->reading[0])
		lprintf(txt->reading,fname);

	/*************/
	/* Swap list */
	/*************/

	get_int(cfg->total_swaps,instream);

	if(cfg->total_swaps) {
		if((cfg->swap=(swap_t **)MALLOC(sizeof(swap_t *)*cfg->total_swaps))==NULL)
			return allocerr(txt,offset,fname,sizeof(swap_t *)*cfg->total_swaps); }
	else
		cfg->swap=NULL;

	for(i=0;i<cfg->total_swaps;i++) {
		if(feof(instream)) break;
		if((cfg->swap[i]=(swap_t *)MALLOC(sizeof(swap_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(swap_t));
		get_alloc(&offset,cfg->swap[i]->cmd,LEN_CMD,instream); }
	cfg->total_swaps=i;

	/********************/
	/* External Editors */
	/********************/

	get_int(cfg->total_xedits,instream);

	if(cfg->total_xedits) {
		if((cfg->xedit=(xedit_t **)MALLOC(sizeof(xedit_t *)*cfg->total_xedits))==NULL)
			return allocerr(txt,offset,fname,sizeof(xedit_t *)*cfg->total_xedits); }
	else
		cfg->xedit=NULL;

	for(i=0;i<cfg->total_xedits;i++) {
		if(feof(instream)) break;
		if((cfg->xedit[i]=(xedit_t *)MALLOC(sizeof(xedit_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(xedit_t));
		memset(cfg->xedit[i],0,sizeof(xedit_t));

		get_alloc(&offset,cfg->xedit[i]->name,40,instream);
		get_str(cfg->xedit[i]->code,instream);
		get_alloc(&offset,cfg->xedit[i]->lcmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->xedit[i]->rcmd,LEN_CMD,instream);

	#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
		if(!strcmp(cfg->xedit[i]->lcmd,cfg->xedit[i]->rcmd) && cfg->xedit[i]->rcmd!=scfgnulstr) {
			FREE(cfg->xedit[i]->rcmd);
			cfg->xedit[i]->rcmd=cfg->xedit[i]->lcmd; }
	#endif

		get_int(cfg->xedit[i]->misc,instream);
	#ifdef SCFG
		get_str(cfg->xedit[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->xedit[i]->ar=arstr(0,str,cfg);
	#endif
		get_int(cfg->xedit[i]->type,instream);
		get_int(c,instream);
		for(j=0;j<7;j++)
			get_int(n,instream);
		}
	cfg->total_xedits=i;


	/*****************************/
	/* External Program Sections */
	/*****************************/

	get_int(cfg->total_xtrnsecs,instream);

	if(cfg->total_xtrnsecs) {
		if((cfg->xtrnsec=(xtrnsec_t **)MALLOC(sizeof(xtrnsec_t *)*cfg->total_xtrnsecs))
			==NULL)
			return allocerr(txt,offset,fname,sizeof(xtrnsec_t *)*cfg->total_xtrnsecs); }
	else
		cfg->xtrnsec=NULL;

	for(i=0;i<cfg->total_xtrnsecs;i++) {
		if(feof(instream)) break;
		if((cfg->xtrnsec[i]=(xtrnsec_t *)MALLOC(sizeof(xtrnsec_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(xtrnsec_t));
		memset(cfg->xtrnsec[i],0,sizeof(xtrnsec_t));

		get_alloc(&offset,cfg->xtrnsec[i]->name,40,instream);
		get_str(cfg->xtrnsec[i]->code,instream);
	#ifdef SCFG
		get_str(cfg->xtrnsec[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->xtrnsec[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_xtrnsecs=i;


	/*********************/
	/* External Programs */
	/*********************/

	get_int(cfg->total_xtrns,instream);

	if(cfg->total_xtrns) {
		if((cfg->xtrn=(xtrn_t **)MALLOC(sizeof(xtrn_t *)*cfg->total_xtrns))==NULL)
			return allocerr(txt,offset,fname,sizeof(xtrn_t *)*cfg->total_xtrns); }
	else
		cfg->xtrn=NULL;

	for(i=0;i<cfg->total_xtrns;i++) {
		if(feof(instream)) break;
		if((cfg->xtrn[i]=(xtrn_t *)MALLOC(sizeof(xtrn_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(xtrn_t));
		memset(cfg->xtrn[i],0,sizeof(xtrn_t));

		get_int(cfg->xtrn[i]->sec,instream);
		get_alloc(&offset,cfg->xtrn[i]->name,40,instream);
		get_str(cfg->xtrn[i]->code,instream);
	#ifdef SCFG
		get_str(cfg->xtrn[i]->ar,instream);
		get_str(cfg->xtrn[i]->run_ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->xtrn[i]->ar=arstr(0,str,cfg);
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->xtrn[i]->run_ar=arstr(0,str,cfg);
	#endif
		get_int(cfg->xtrn[i]->type,instream);
		get_int(cfg->xtrn[i]->misc,instream);
		get_int(cfg->xtrn[i]->event,instream);
		get_int(cfg->xtrn[i]->cost,instream);
		get_alloc(&offset,cfg->xtrn[i]->cmd,LEN_CMD,instream);
		get_alloc(&offset,cfg->xtrn[i]->clean,LEN_CMD,instream);
		fread(str,LEN_DIR+1,1,instream);
		offset+=LEN_DIR+1;
		if(strcmp(str+1,":\\") && strcmp(str+1,":/") && 
			(str[strlen(str)-1]=='\\' || str[strlen(str)-1]=='/')) // C:\ is valid
			str[strlen(str)-1]=0;		// C:\SBBS\XTRN\GAME\ is not
	#ifdef SBBS
		if(!str[0]) 			  /* Minimum path of '.' */
			strcpy(str,".");
		prep_path(cfg->ctrl_dir, str);
		if((cfg->xtrn[i]->path=(char *)MALLOC(strlen(str)+1))==NULL)
			return allocerr(txt,offset,fname,strlen(str)+1);
	#endif
		strcpy(cfg->xtrn[i]->path,str);
		get_int(cfg->xtrn[i]->textra,instream);
		get_int(cfg->xtrn[i]->maxtime,instream);
		for(j=0;j<7;j++)
			get_int(n,instream);
		}
	cfg->total_xtrns=i;


	/****************/
	/* Timed Events */
	/****************/

	get_int(cfg->total_events,instream);

	if(cfg->total_events) {
		if((cfg->event=(event_t **)MALLOC(sizeof(event_t *)*cfg->total_events))==NULL)
			return allocerr(txt,offset,fname,sizeof(event_t *)*cfg->total_events); }
	else
		cfg->event=NULL;

	for(i=0;i<cfg->total_events;i++) {
		if(feof(instream)) break;
		if((cfg->event[i]=(event_t *)MALLOC(sizeof(event_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(event_t));
		memset(cfg->event[i],0,sizeof(event_t));

		get_str(cfg->event[i]->code,instream);
		get_alloc(&offset,cfg->event[i]->cmd,LEN_CMD,instream);
		get_int(cfg->event[i]->days,instream);
		get_int(cfg->event[i]->time,instream);
		get_int(cfg->event[i]->node,instream);
		get_int(cfg->event[i]->misc,instream);
		fread(str,LEN_DIR+1,1,instream);
		offset+=LEN_DIR+1;
	#ifdef SBBS
		prep_path(cfg->ctrl_dir, str);
		if((cfg->event[i]->dir=(char *)MALLOC(strlen(str)+1))==NULL)
			return allocerr(txt,offset,fname,strlen(str)+1);
	#endif
		strcpy(cfg->event[i]->dir,str);

		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_events=i;

	/********************************/
	/* Native (32-bit) Program list */
	/********************************/

	get_int(cfg->total_natvpgms,instream);

	if(cfg->total_natvpgms) {
		if((cfg->natvpgm=(natvpgm_t **)MALLOC(sizeof(natvpgm_t *)*cfg->total_natvpgms))==NULL)
			return allocerr(txt,offset,fname,sizeof(natvpgm_t *)*cfg->total_natvpgms); }
	else
		cfg->natvpgm=NULL;

	for(i=0;i<cfg->total_natvpgms;i++) {
		if(feof(instream)) break;
		if((cfg->natvpgm[i]=(natvpgm_t *)MALLOC(sizeof(natvpgm_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(natvpgm_t));
		get_alloc(&offset,cfg->natvpgm[i]->name,12,instream);
		cfg->natvpgm[i]->misc=0; }
	cfg->total_natvpgms=i;
	for(i=0;i<cfg->total_natvpgms;i++) {
		if(feof(instream)) break;
		get_int(cfg->natvpgm[i]->misc,instream); }


	fclose(instream);
	if(txt->readit && txt->readit[0])
		lprintf(txt->readit,fname);

	return(TRUE);
}

#endif


#ifndef NO_CHAT_CFG

/****************************************************************************/
/* Reads in CHAT.CNF and initializes the associated variables				*/
/****************************************************************************/
BOOL read_chat_cfg(scfg_t* cfg, read_cfg_text_t* txt)
{
	char	str[256],fname[13];
	short	i,j,n;
	long	offset=0;
	FILE	*instream;

	strcpy(fname,"CHAT.CNF");
	sprintf(str,"%s%s",cfg->ctrl_dir,fname);
	if((instream=fopen(str,"rb"/* O_RDONLY */))==NULL) {
		lprintf(txt->openerr,str);
		return(FALSE); }

	if(txt->reading && txt->reading[0])
		lprintf(txt->reading,fname);

	/*********/
	/* Gurus */
	/*********/

	get_int(cfg->total_gurus,instream);

	if(cfg->total_gurus) {
		if((cfg->guru=(guru_t **)MALLOC(sizeof(guru_t *)*cfg->total_gurus))==NULL)
			return allocerr(txt,offset,fname,sizeof(guru_t *)*cfg->total_gurus); }
	else
		cfg->guru=NULL;

	for(i=0;i<cfg->total_gurus;i++) {
		if(feof(instream)) break;
		if((cfg->guru[i]=(guru_t *)MALLOC(sizeof(guru_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(guru_t));
		memset(cfg->guru[i],0,sizeof(guru_t));

		get_alloc(&offset,cfg->guru[i]->name,25,instream);
		get_str(cfg->guru[i]->code,instream);

	#ifdef SCFG
		get_str(cfg->guru[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->guru[i]->ar=arstr(0,str,cfg);
	#endif
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_chans=i;


	/********************/
	/* Chat Action Sets */
	/********************/

	get_int(cfg->total_actsets,instream);

	if(cfg->total_actsets) {
		if((cfg->actset=(actset_t **)MALLOC(sizeof(actset_t *)*cfg->total_actsets))==NULL)
			return allocerr(txt,offset,fname,sizeof(actset_t *)*cfg->total_actsets); }
	else
		cfg->actset=NULL;

	for(i=0;i<cfg->total_actsets;i++) {
		if(feof(instream)) break;
		if((cfg->actset[i]=(actset_t *)MALLOC(sizeof(actset_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(actset_t));
		get_alloc(&offset,cfg->actset[i]->name,25,instream);
		}
	cfg->total_actsets=i;


	/****************/
	/* Chat Actions */
	/****************/

	get_int(cfg->total_chatacts,instream);

	if(cfg->total_chatacts) {
		if((cfg->chatact=(chatact_t **)MALLOC(sizeof(chatact_t *)*cfg->total_chatacts))
			==NULL)
			return allocerr(txt,offset,fname,sizeof(chatact_t *)*cfg->total_chatacts); }
	else
		cfg->chatact=NULL;

	for(i=0;i<cfg->total_chatacts;i++) {
		if(feof(instream)) break;
		if((cfg->chatact[i]=(chatact_t *)MALLOC(sizeof(chatact_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(chatact_t));
		memset(cfg->chatact[i],0,sizeof(chatact_t));

		get_int(cfg->chatact[i]->actset,instream);
		get_alloc(&offset,cfg->chatact[i]->cmd,LEN_CHATACTCMD,instream);
		get_alloc(&offset,cfg->chatact[i]->out,LEN_CHATACTOUT,instream);
		for(j=0;j<8;j++)
			get_int(n,instream);
		}

	cfg->total_chatacts=i;


	/***************************/
	/* Multinode Chat Channels */
	/***************************/

	get_int(cfg->total_chans,instream);

	if(cfg->total_chans) {
		if((cfg->chan=(chan_t **)MALLOC(sizeof(chan_t *)*cfg->total_chans))==NULL)
			return allocerr(txt,offset,fname,sizeof(chan_t *)*cfg->total_chans); }
	else
		cfg->chan=NULL;

	for(i=0;i<cfg->total_chans;i++) {
		if(feof(instream)) break;
		if((cfg->chan[i]=(chan_t *)MALLOC(sizeof(chan_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(chan_t));
		memset(cfg->chan[i],0,sizeof(chan_t));

		get_int(cfg->chan[i]->actset,instream);
		get_alloc(&offset,cfg->chan[i]->name,25,instream);

		get_str(cfg->chan[i]->code,instream);

	#ifdef SCFG
		get_str(cfg->chan[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->chan[i]->ar=arstr(0,str,cfg);
	#endif
		get_int(cfg->chan[i]->cost,instream);
		get_int(cfg->chan[i]->guru,instream);
		get_int(cfg->chan[i]->misc,instream);
		for(j=0;j<8;j++)
			get_int(n,instream); }
	cfg->total_chans=i;


	/**************/
	/* Chat Pages */
	/**************/

	get_int(cfg->total_pages,instream);

	if(cfg->total_pages) {
		if((cfg->page=(page_t **)MALLOC(sizeof(page_t *)*cfg->total_pages))==NULL)
			return allocerr(txt,offset,fname,sizeof(page_t *)*cfg->total_pages); }
	else
		cfg->page=NULL;

	for(i=0;i<cfg->total_pages;i++) {
		if(feof(instream)) break;
		if((cfg->page[i]=(page_t *)MALLOC(sizeof(page_t)))==NULL)
			return allocerr(txt,offset,fname,sizeof(page_t));
		memset(cfg->page[i],0,sizeof(page_t));

		get_alloc(&offset,cfg->page[i]->cmd,LEN_CMD,instream);

	#ifdef SCFG
		get_str(cfg->page[i]->ar,instream);
	#else
		fread(str,LEN_ARSTR+1,1,instream);
		offset+=LEN_ARSTR+1;
		cfg->page[i]->ar=arstr(0,str,cfg);
	#endif
		get_int(cfg->page[i]->misc,instream);
		for(j=0;j<8;j++)
			get_int(n,instream);
		}
	cfg->total_pages=i;


	fclose(instream);
	if(txt->readit && txt->readit[0])
		lprintf(txt->readit,fname);

	return(TRUE);
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
uchar attrstr(char *str)
{
	uchar atr;
	ulong l=0;

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
				atr=(uchar)((atr&0x8f)|(BLACK<<4));
				break;
			case '7':	/* White Background */
				atr=(uchar)((atr&0x8f)|(LIGHTGRAY<<4));
				break;
			case 'R':
				atr=(uchar)((atr&0xf8)|RED);
				break;
			case 'G':
				atr=(uchar)((atr&0xf8)|GREEN);
				break;
			case 'Y':   /* Yellow */
				atr=(uchar)((atr&0xf8)|BROWN);
				break;
			case 'B':
				atr=(uchar)((atr&0xf8)|BLUE);
				break;
			case 'M':
				atr=(uchar)((atr&0xf8)|MAGENTA);
				break;
			case 'C':
				atr=(uchar)((atr&0xf8)|CYAN);
				break;
			case '1':	/* Red Background */
				atr=(uchar)((atr&0x8f)|(RED<<4));
				break;
			case '2':	/* Green Background */
				atr=(uchar)((atr&0x8f)|(GREEN<<4));
				break;
			case '3':	/* Yellow Background */
				atr=(uchar)((atr&0x8f)|(BROWN<<4));
				break;
			case '4':	/* Blue Background */
				atr=(uchar)((atr&0x8f)|(BLUE<<4));
				break;
			case '5':	/* Magenta Background */
				atr=(uchar)((atr&0x8f)|(MAGENTA<<4));
				break;
			case '6':	/* Cyan Background */
				atr=(uchar)((atr&0x8f)|(CYAN<<4));
				break; }
		l++; }
	return(atr);
}

void free_file_cfg(scfg_t* cfg)
{
	uint i;

	for(i=0;i<cfg->total_fextrs;i++)
		FREE_AND_NULL(cfg->fextr[i]);
	FREE_AND_NULL(cfg->fextr);

	for(i=0;i<cfg->total_fcomps;i++)
		FREE_AND_NULL(cfg->fcomp[i]);
	FREE_AND_NULL(cfg->fcomp);

	for(i=0;i<cfg->total_fviews;i++)
		FREE_AND_NULL(cfg->fview[i]);
	FREE_AND_NULL(cfg->fview);

	for(i=0;i<cfg->total_ftests;i++)
		FREE_AND_NULL(cfg->ftest[i]);
	FREE_AND_NULL(cfg->ftest);

	for(i=0;i<cfg->total_dlevents;i++)
		FREE_AND_NULL(cfg->dlevent[i]);
	FREE_AND_NULL(cfg->dlevent);

	for(i=0;i<cfg->total_prots;i++)
		FREE_AND_NULL(cfg->prot[i]);
	FREE_AND_NULL(cfg->prot);

	for(i=0;i<cfg->altpaths;i++)
		FREE_AND_NULL(cfg->altpath[i]);
	FREE_AND_NULL(cfg->altpath);

	for(i=0;i<cfg->total_libs;i++)
		FREE_AND_NULL(cfg->lib[i]);
	FREE_AND_NULL(cfg->lib);

	for(i=0;i<cfg->total_dirs;i++)
		FREE_AND_NULL(cfg->dir[i]);
	FREE_AND_NULL(cfg->dir);

	for(i=0;i<cfg->total_txtsecs;i++)
		FREE_AND_NULL(cfg->txtsec[i]);
	FREE_AND_NULL(cfg->txtsec);
}

void free_chat_cfg(scfg_t* cfg)
{
	int i;

	for(i=0;i<cfg->total_actsets;i++)
		FREE_AND_NULL(cfg->actset[i]);
	FREE_AND_NULL(cfg->actset);

	for(i=0;i<cfg->total_chatacts;i++)
		FREE_AND_NULL(cfg->chatact[i]);
	FREE_AND_NULL(cfg->chatact);

	for(i=0;i<cfg->total_chans;i++)
		FREE_AND_NULL(cfg->chan[i]);
	FREE_AND_NULL(cfg->chan);

	for(i=0;i<cfg->total_gurus;i++)
		FREE_AND_NULL(cfg->guru[i]);
	FREE_AND_NULL(cfg->guru);

	for(i=0;i<cfg->total_pages;i++)
		FREE_AND_NULL(cfg->page[i]);
	FREE_AND_NULL(cfg->page);

}

void free_xtrn_cfg(scfg_t* cfg)
{
	int i;

	for(i=0;i<cfg->total_swaps;i++)
		FREE_AND_NULL(cfg->swap[i]);
	FREE_AND_NULL(cfg->swap);

	for(i=0;i<cfg->total_xedits;i++)
		FREE_AND_NULL(cfg->xedit[i]);
	FREE_AND_NULL(cfg->xedit);

	for(i=0;i<cfg->total_xtrnsecs;i++)
		FREE_AND_NULL(cfg->xtrnsec[i]);
	FREE_AND_NULL(cfg->xtrnsec);

	for(i=0;i<cfg->total_xtrns;i++)
		FREE_AND_NULL(cfg->xtrn[i]);
	FREE_AND_NULL(cfg->xtrn);

	for(i=0;i<cfg->total_events;i++)
		FREE_AND_NULL(cfg->event[i]);
	FREE_AND_NULL(cfg->event);

}
