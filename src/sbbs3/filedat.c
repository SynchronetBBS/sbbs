/* filedat.c */

/* Synchronet file database-related exported functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/****************************************************************************/
/* Gets filedata from dircode.DAT file										*/
/* Need fields .name ,.dir and .offset to get other info    				*/
/* Does not fill .dateuled or .datedled fields.                             */
/****************************************************************************/
BOOL DLLCALL getfiledat(scfg_t* cfg, file_t* f)
{
	char buf[F_LEN+1],str[256],tmp[128];
	int file;
	long length;

	sprintf(str,"%s%s.dat",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if(f->datoffset>length) {
		close(file);
		return(FALSE); 
	}
	if(length%F_LEN) {
		close(file);
		return(FALSE); 
	}
	lseek(file,f->datoffset,SEEK_SET);
	if(read(file,buf,F_LEN)!=F_LEN) {
		close(file);
		return(FALSE); 
	}
	close(file);
	getrec(buf,F_ALTPATH,2,str);
	f->altpath=hptoi(str);
	getrec(buf,F_CDT,LEN_FCDT,str);
	f->cdt=atol(str);

	if(!f->size) {					/* only read disk if this is null */
			sprintf(str,"%s%s"
				,f->altpath>0 && f->altpath<=cfg->altpaths ? cfg->altpath[f->altpath-1]
				: cfg->dir[f->dir]->path,unpadfname(f->name,tmp));
			f->size=flength(str);
			f->date=fdate(str);
	/*
			}
		else {
			f->size=f->cdt;
			f->date=0; }
	*/
			}
#if 0
	if((f->size>0L) && cur_cps)
		f->timetodl=(ushort)(f->size/(ulong)cur_cps);
	else
#endif
		f->timetodl=0;

	getrec(buf,F_DESC,LEN_FDESC,f->desc);
	getrec(buf,F_ULER,LEN_ALIAS,f->uler);
	getrec(buf,F_TIMESDLED,5,str);
	f->timesdled=atoi(str);
	getrec(buf,F_OPENCOUNT,3,str);
	f->opencount=atoi(str);
	if(buf[F_MISC]!=ETX)
		f->misc=buf[F_MISC]-SP;
	else
		f->misc=0;
	return(TRUE);
}

/****************************************************************************/
/* Puts filedata into DIR_code.DAT file                                     */
/* Called from removefiles                                                  */
/****************************************************************************/
BOOL DLLCALL putfiledat(scfg_t* cfg, file_t* f)
{
    char buf[F_LEN+1],str[256],tmp[128];
    int file;
    long length;

	putrec(buf,F_CDT,LEN_FCDT,ultoa(f->cdt,tmp,10));
	putrec(buf,F_DESC,LEN_FDESC,f->desc);
	putrec(buf,F_DESC+LEN_FDESC,2,crlf);
	putrec(buf,F_ULER,LEN_ALIAS+5,f->uler);
	putrec(buf,F_ULER+LEN_ALIAS+5,2,crlf);
	putrec(buf,F_TIMESDLED,5,ultoa(f->timesdled,tmp,10));
	putrec(buf,F_TIMESDLED+5,2,crlf);
	putrec(buf,F_OPENCOUNT,3,ultoa(f->opencount,tmp,10));
	putrec(buf,F_OPENCOUNT+3,2,crlf);
	buf[F_MISC]=f->misc+SP;
	putrec(buf,F_ALTPATH,2,hexplus(f->altpath,tmp));
	putrec(buf,F_ALTPATH+2,2,crlf);
	sprintf(str,"%s%s.dat",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_WRONLY|O_BINARY,SH_DENYRW))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if(length%F_LEN) {
		close(file);
		return(FALSE); 
	}
	if(f->datoffset>length) {
		close(file);
		return(FALSE); 
	}
	lseek(file,f->datoffset,SEEK_SET);
	if(write(file,buf,F_LEN)!=F_LEN) {
		close(file);
		return(FALSE); 
	}
	length=filelength(file);
	close(file);
	if(length%F_LEN) {
		return(FALSE);
	}
	return(TRUE);
}

/****************************************************************************/
/* Adds the data for struct filedat to the directory's data base.           */
/* changes the .datoffset field only                                        */
/* returns 1 if added successfully, 0 if not.								*/
/****************************************************************************/
BOOL DLLCALL addfiledat(scfg_t* cfg, file_t* f)
{
	char	str[256],fname[13],c,fdat[F_LEN+1];
	char	tmp[128];
	uchar	HUGE16 *ixbbuf,idx[3];
    int		i,file;
	long	l,length;
	time_t	now;
	time_t	uldate;

	/************************/
	/* Add data to DAT File */
	/************************/
	sprintf(str,"%s%s.dat",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDWR|O_BINARY|O_CREAT,SH_DENYRW,S_IREAD|S_IWRITE))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if(length==0L)
		l=0L;
	else {
		if(length%F_LEN) {
			close(file);
			return(FALSE); 
		}
		for(l=0;l<length;l+=F_LEN) {    /* Find empty slot */
			lseek(file,l,SEEK_SET);
			read(file,&c,1);
			if(c==ETX) break; 
		}
		if(l/F_LEN>=MAX_FILES || l/F_LEN>=cfg->dir[f->dir]->maxfiles) {
			close(file);
			return(FALSE); 
		} 
	}
	putrec(fdat,F_CDT,LEN_FCDT,ultoa(f->cdt,tmp,10));
	putrec(fdat,F_DESC,LEN_FDESC,f->desc);
	putrec(fdat,F_DESC+LEN_FDESC,2,crlf);
	putrec(fdat,F_ULER,LEN_ALIAS+5,f->uler);
	putrec(fdat,F_ULER+LEN_ALIAS+5,2,crlf);
	putrec(fdat,F_TIMESDLED,5,ultoa(f->timesdled,tmp,10));
	putrec(fdat,F_TIMESDLED+5,2,crlf);
	putrec(fdat,F_OPENCOUNT,3,ultoa(f->opencount,tmp,10));
	putrec(fdat,F_OPENCOUNT+3,2,crlf);
	fdat[F_MISC]=f->misc+SP;
	putrec(fdat,F_ALTPATH,2,hexplus(f->altpath,tmp));
	putrec(fdat,F_ALTPATH+2,2,crlf);
	f->datoffset=l;
	idx[0]=(uchar)(l&0xff);          /* Get offset within DAT file for IXB file */
	idx[1]=(uchar)((l>>8)&0xff);
	idx[2]=(uchar)((l>>16)&0xff);
	lseek(file,l,SEEK_SET);
	if(write(file,fdat,F_LEN)!=F_LEN) {
		close(file);
		return(FALSE); 
	}
	length=filelength(file);
	close(file);
	if(length%F_LEN) {
		return(FALSE);
	}

	/*******************************************/
	/* Update last upload date/time stamp file */
	/*******************************************/
	sprintf(str,"%s%s.dab",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_WRONLY|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))!=-1) {
		now=time(NULL);
		write(file,&now,4);
		close(file); 
	}

	/************************/
	/* Add data to IXB File */
	/************************/
	strcpy(fname,f->name);
	for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
		fname[i]=fname[i+1];
	sprintf(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDWR|O_CREAT|O_BINARY,SH_DENYRW,S_IREAD|S_IWRITE))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if(length) {    /* IXB file isn't empty */
		if(length%F_IXBSIZE) {
			close(file);
			return(FALSE); 
		}
		if((ixbbuf=(uchar *)MALLOC(length))==NULL) {
			close(file);
			return(FALSE); 
		}
		if(lread(file,ixbbuf,length)!=length) {
			close(file);
			FREE((char *)ixbbuf);
			return(FALSE); 
		}
	/************************************************/
	/* Sort by Name or Date, Assending or Decending */
	/************************************************/
		if(cfg->dir[f->dir]->sort==SORT_NAME_A || cfg->dir[f->dir]->sort==SORT_NAME_D) {
			for(l=0;l<length;l+=F_IXBSIZE) {
				for(i=0;i<12 && toupper(fname[i])==toupper(ixbbuf[l+i]);i++);
				if(i==12) {     /* file already in directory index */
					close(file);
					FREE((char *)ixbbuf);
					return(FALSE); 
				}
				if(cfg->dir[f->dir]->sort==SORT_NAME_A 
					&& toupper(fname[i])<toupper(ixbbuf[l+i]))
					break;
				if(cfg->dir[f->dir]->sort==SORT_NAME_D 
					&& toupper(fname[i])>toupper(ixbbuf[l+i]))
					break; 
			} 
		}
		else {  /* sort by date */
			for(l=0;l<length;l+=F_IXBSIZE) {
				uldate=(ixbbuf[l+14]|((long)ixbbuf[l+15]<<8)
					|((long)ixbbuf[l+16]<<16)|((long)ixbbuf[l+17]<<24));
				if(cfg->dir[f->dir]->sort==SORT_DATE_A && f->dateuled<uldate)
					break;
				if(cfg->dir[f->dir]->sort==SORT_DATE_D && f->dateuled>uldate)
					break; 
			} 
		}
		lseek(file,l,SEEK_SET);
		if(write(file,fname,11)!=11) {  /* Write filename to IXB file */
			close(file);
			FREE((char *)ixbbuf);
			return(FALSE); 
		}
		if(write(file,idx,3)!=3) {  /* Write DAT offset into IXB file */
			close(file);
			FREE((char *)ixbbuf);
			return(FALSE); 
		}
		write(file,&f->dateuled,sizeof(time_t));
		write(file,&f->datedled,4);              /* Write 0 for datedled */
		if(lwrite(file,&ixbbuf[l],length-l)!=length-l) { /* Write rest of IXB */
			close(file);
			FREE((char *)ixbbuf);
			return(FALSE); 
		}
		FREE((char *)ixbbuf); }
	else {              /* IXB file is empty... No files */
		if(write(file,fname,11)!=11) {  /* Write filename it IXB file */
			close(file);
			return(FALSE); 
		}
		if(write(file,idx,3)!=3) {  /* Write DAT offset into IXB file */
			close(file);
			return(FALSE); 
		}
		write(file,&f->dateuled,sizeof(time_t));
		write(file,&f->datedled,4); 
	}
	length=filelength(file);
	close(file);
	return(TRUE);
}

/****************************************************************************/
/* Gets file data from dircode.ixb file										*/
/* Need fields .name and .dir filled.                                       */
/* only fills .offset, .dateuled, and .datedled                             */
/****************************************************************************/
BOOL DLLCALL getfileixb(scfg_t* cfg, file_t* f)
{
	char			str[256],fname[13];
	uchar HUGE16 *	ixbbuf;
	int				file;
	long			l,length;

	sprintf(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		return(FALSE); 
	}
	if((ixbbuf=(uchar *)MALLOC(length))==NULL) {
		close(file);
		return(FALSE); 
	}
	if(lread(file,ixbbuf,length)!=length) {
		close(file);
		FREE((char *)ixbbuf);
		return(FALSE); 
	}
	close(file);
	strcpy(fname,f->name);
	for(l=8;l<12;l++)	/* Turn FILENAME.EXT into FILENAMEEXT */
		fname[l]=fname[l+1];
	for(l=0;l<length;l+=F_IXBSIZE) {
		sprintf(str,"%11.11s",ixbbuf+l);
		if(!stricmp(str,fname))
			break; 
	}
	if(l>=length) {
		FREE((char *)ixbbuf);
		return(FALSE); 
	}
	l+=11;
	f->datoffset=ixbbuf[l]|((long)ixbbuf[l+1]<<8)|((long)ixbbuf[l+2]<<16);
	f->dateuled=ixbbuf[l+3]|((long)ixbbuf[l+4]<<8)
		|((long)ixbbuf[l+5]<<16)|((long)ixbbuf[l+6]<<24);
	f->datedled=ixbbuf[l+7]|((long)ixbbuf[l+8]<<8)
		|((long)ixbbuf[l+9]<<16)|((long)ixbbuf[l+10]<<24);
	FREE((char *)ixbbuf);
	return(TRUE);
}

/****************************************************************************/
/* Removes DAT and IXB entries for the file in the struct 'f'               */
/****************************************************************************/
BOOL DLLCALL removefiledat(scfg_t* cfg, file_t* f)
{
	char	c,str[256],ixbname[12],HUGE16 *ixbbuf,fname[13];
    int		i,file;
	long	l,length;

	strcpy(fname,f->name);
	for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
		fname[i]=fname[i+1];
	sprintf(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if(!length) {
		close(file);
		return(FALSE); 
	}
	if((ixbbuf=(char *)MALLOC(length))==0) {
		close(file);
		return(FALSE); 
	}
	if(lread(file,ixbbuf,length)!=length) {
		close(file);
		FREE((char *)ixbbuf);
		return(FALSE); 
	}
	close(file);
	if((file=sopen(str,O_WRONLY|O_TRUNC|O_BINARY,SH_DENYRW))==-1) {
		return(FALSE); 
	}
	for(l=0;l<length;l+=F_IXBSIZE) {
		for(i=0;i<11;i++)
			ixbname[i]=ixbbuf[l+i];
		ixbname[i]=0;
		if(stricmp(ixbname,fname))
			if(lwrite(file,&ixbbuf[l],F_IXBSIZE)!=F_IXBSIZE) {
				close(file);
				FREE((char *)ixbbuf);
				return(FALSE); 
		} 
	}
	FREE((char *)ixbbuf);
	close(file);
	sprintf(str,"%s%s.dat",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=sopen(str,O_WRONLY|O_BINARY,SH_DENYRW))==-1) {
		return(FALSE); 
	}
	lseek(file,f->datoffset,SEEK_SET);
	c=ETX;          /* If first char of record is ETX, record is unused */
	if(write(file,&c,1)!=1) { /* So write a D_T on the first byte of the record */
		close(file);
		return(FALSE); 
	}
	close(file);
	if(f->dir==cfg->user_dir)  /* remove file from index */
		rmuserxfers(cfg,0,0,f->name);
	return(TRUE);
}

/****************************************************************************/
/* Checks  directory data file for 'filename' (must be padded). If found,   */
/* it returns the 1, else returns 0.                                        */
/* Called from upload and bulkupload                                        */
/****************************************************************************/
BOOL DLLCALL findfile(scfg_t* cfg, uint dirnum, char *filename)
{
	char str[256],fname[13],HUGE16 *ixbbuf;
    int i,file;
    long length,l;

	sprintf(fname,"%.12s",filename);
	strupr(fname);
	for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
		fname[i]=fname[i+1];
	sprintf(str,"%s%s.ixb",cfg->dir[dirnum]->data_dir,cfg->dir[dirnum]->code);
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) return(FALSE);
	length=filelength(file);
	if(!length) {
		close(file);
		return(FALSE); }
	if((ixbbuf=(char *)MALLOC(length))==NULL) {
		close(file);
		return(FALSE); }
	if(lread(file,ixbbuf,length)!=length) {
		close(file);
		FREE((char *)ixbbuf);
		return(FALSE); }
	close(file);
	for(l=0;l<length;l+=F_IXBSIZE) {
		for(i=0;i<11;i++)
			if(toupper(fname[i])!=toupper(ixbbuf[l+i])) break;
		if(i==11) break; }
	FREE((char *)ixbbuf);
	if(l!=length)
		return(TRUE);
	return(FALSE);
}

/****************************************************************************/
/* Turns FILE.EXT into FILE    .EXT                                         */
/****************************************************************************/
char* DLLCALL padfname(char *filename, char *str)
{
    int c,d;

	for(c=0;c<8;c++)
		if(filename[c]=='.' || !filename[c]) break;
		else str[c]=filename[c];
	d=c;
	if(filename[c]=='.') c++;
	while(d<8)
		str[d++]=' ';
	if(filename[c]>' ')	/* Change "FILE" to "FILE        " */
		str[d++]='.';	/* (don't add a dot if there's no extension) */
	else
		str[d++]=' ';
	while(d<12)
		if(!filename[c]) break;
		else str[d++]=filename[c++];
	while(d<12)
		str[d++]=' ';
	str[d]=0;
	return(str);
}

/****************************************************************************/
/* Turns FILE    .EXT into FILE.EXT                                         */
/****************************************************************************/
char* DLLCALL unpadfname(char *filename, char *str)
{
    int c,d;

	for(c=0,d=0;filename[c];c++)
		if(filename[c]!=SP) str[d++]=filename[c];
	str[d]=0;
	return(str);
}

/****************************************************************************/
/* Removes any files in the user transfer index (XFER.IXT) that match the   */
/* specifications of dest, or source user, or filename or any combination.  */
/****************************************************************************/
BOOL DLLCALL rmuserxfers(scfg_t* cfg, int fromuser, int destuser, char *fname)
{
    char str[256],*ixtbuf;
    int file;
    long l,length;

	sprintf(str,"%sxfer.ixt", cfg->data_dir);
	if(!fexist(str))
		return(FALSE);
	if(!flength(str)) {
		remove(str);
		return(FALSE); 
	}
	if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYWR))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	if((ixtbuf=(char *)MALLOC(length))==NULL) {
		close(file);
		return(FALSE); 
	}
	if(read(file,ixtbuf,length)!=length) {
		close(file);
		FREE(ixtbuf);
		return(FALSE); 
	}
	close(file);
	if((file=sopen(str,O_WRONLY|O_TRUNC|O_BINARY,SH_DENYRW))==-1) {
		FREE(ixtbuf);
		return(FALSE); 
	}
	for(l=0;l<length;l+=24) {
		if(fname!=NULL && fname[0]) {               /* fname specified */
			if(!strncmp(ixtbuf+l+5,fname,12)) {     /* this is the file */
				if(destuser && fromuser) {          /* both dest and from user */
					if(atoi(ixtbuf+l)==destuser && atoi(ixtbuf+l+18)==fromuser)
						continue; }                 /* both match */
				else if(fromuser) {                 /* from user */
					if(atoi(ixtbuf+l+18)==fromuser) /* matches */
						continue; }
				else if(destuser) {                 /* dest user */
					if(atoi(ixtbuf+l)==destuser)    /* matches */
						continue; }
				else continue; } }                  /* no users, so match */
		else if(destuser && fromuser) {
			if(atoi(ixtbuf+l+18)==fromuser && atoi(ixtbuf+l)==destuser)
				continue; }
		else if(destuser && atoi(ixtbuf+l)==destuser)
			continue;
		else if(fromuser && atoi(ixtbuf+l+18)==fromuser)
			continue;
		write(file,ixtbuf+l,24); }
	close(file);
	FREE(ixtbuf);

	return(TRUE);
}

void DLLCALL getextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext)
{
	char str[256];
	int file;

	memset(ext,0,F_EXBSIZE+1);
	sprintf(str,"%s%s.exb",cfg->dir[dirnum]->data_dir,cfg->dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY))==-1)
		return;
	lseek(file,(datoffset/F_LEN)*F_EXBSIZE,SEEK_SET);
	read(file,ext,F_EXBSIZE);
	close(file);
}

void DLLCALL putextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext)
{
	char str[256],nulbuf[F_EXBSIZE];
	int file;

	strip_invalid_attr(ext);	/* eliminate bogus ctrl-a codes */
	memset(nulbuf,0,sizeof(nulbuf));
	sprintf(str,"%s%s.exb",cfg->dir[dirnum]->data_dir,cfg->dir[dirnum]->code);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
		return;
	lseek(file,0L,SEEK_END);
	while(filelength(file)<(long)(datoffset/F_LEN)*F_EXBSIZE)
		write(file,nulbuf,sizeof(nulbuf));
	lseek(file,(datoffset/F_LEN)*F_EXBSIZE,SEEK_SET);
	write(file,ext,F_EXBSIZE);
	close(file);
}

/****************************************************************************/
/* Update the upload date for the file 'f'                                  */
/****************************************************************************/
int DLLCALL update_uldate(scfg_t* cfg, file_t* f)
{
	char str[256],fname[13];
	int i,file;
	long l,length;

	/*******************/
	/* Update IXB File */
	/*******************/
	sprintf(str,"%s%s.ixb",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=nopen(str,O_RDWR))==-1)
		return(errno); 
	length=filelength(file);
	if(length%F_IXBSIZE) {
		close(file);
		return(-1); 
	}
	strcpy(fname,f->name);
	for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
		fname[i]=fname[i+1];
	for(l=0;l<length;l+=F_IXBSIZE) {
		read(file,str,F_IXBSIZE);      /* Look for the filename in the IXB file */
		str[11]=0;
		if(!strcmp(fname,str)) break; }
	if(l>=length) {
		close(file);
		return(-2); 
	}
	lseek(file,l+14,SEEK_SET);
	write(file,&f->dateuled,4);
	close(file);

	/*******************************************/
	/* Update last upload date/time stamp file */
	/*******************************************/
	sprintf(str,"%s%s.dab",cfg->dir[f->dir]->data_dir,cfg->dir[f->dir]->code);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
		return(errno);

	write(file,&f->dateuled,4);
	close(file); 
	return(0);
}
