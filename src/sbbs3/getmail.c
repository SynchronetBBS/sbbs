/* getmail.cpp */

/* Synchronet DLL-exported mail-related routines */

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
/* Returns the number of pieces of mail waiting for usernumber              */
/* If sent is non-zero, it returns the number of mail sent by usernumber    */
/* If usernumber is 0, it returns all mail on the system                    */
/****************************************************************************/
int DLLCALL getmail(scfg_t* cfg, int usernumber, BOOL sent)
{
    char    str[128];
    int     i=0;
    long    l;
    idxrec_t idx;
	smb_t	smb;

	sprintf(smb.file,"%smail",cfg->data_dir);
	smb.retry_time=cfg->smb_retry_time;
	sprintf(str,"%s.sid",smb.file);
	l=flength(str);
	if(l<(long)sizeof(idxrec_t))
		return(0);
	if(!usernumber) 
		return(l/sizeof(idxrec_t)); 	/* Total system e-mail */
	smb.subnum=INVALID_SUB;
	if(smb_open(&smb)!=0) 
		return(0); 
	while(!smb_feof(smb.sid_fp)) {
		if(smb_fread(&idx,sizeof(idx),smb.sid_fp) != sizeof(idx))
			break;
		if(idx.number==0)	/* invalid message number, ignore */
			continue;
		if(idx.attr&MSG_DELETE)
			continue;
		if((!sent && idx.to==usernumber)
		 || (sent && idx.from==usernumber))
			i++; 
	}
	smb_close(&smb);
	return(i);
}


/***************************/
/* Delete file attachments */
/***************************/
void DLLCALL delfattach(scfg_t* cfg, smbmsg_t* msg)
{
    char str[MAX_PATH+1];
	char str2[MAX_PATH+1];
	char *tp,*sp,*p;

	if(msg->idx.to==0) {	/* netmail */
		sprintf(str,"%sfile/%04u.out/%s"
			,cfg->data_dir,msg->idx.from,getfname(msg->subj));
		remove(str);
		sprintf(str,"%sfile/%04u.out"
			,cfg->data_dir,msg->idx.from);
		rmdir(str);
		return;
	}
		
	strcpy(str,msg->subj);
	tp=str;
	while(1) {
		p=strchr(tp,SP);
		if(p) *p=0;
		sp=strrchr(tp,'/');              /* sp is slash pointer */
		if(!sp) sp=strrchr(tp,'\\');
		if(sp) tp=sp+1;
		sprintf(str2,"%sfile/%04u.in/%s"  /* str2 is path/fname */
			,cfg->data_dir,msg->idx.to,tp);
		remove(str2);
		if(!p)
			break;
		tp=p+1; }
	sprintf(str,"%sfile/%04u.in",cfg->data_dir,msg->idx.to);
	rmdir(str);                     /* remove the dir if it's empty */
}

/****************************************************************************/
/* Loads mail waiting for user number 'usernumber' into the mail array of   */
/* of pointers to mail_t (message numbers and attributes)                   */
/* smb_open(&smb) must be called prior										*/
/****************************************************************************/
mail_t* DLLCALL loadmail(smb_t* smb, ulong* msgs, uint usernumber
			   ,int which, long mode)
{
	ulong		l=0;
    idxrec_t    idx;
	mail_t*		mail=NULL;

	if(msgs==NULL)
		return(NULL);

	*msgs=0;

	if(smb==NULL)
		return(NULL);

	if(smb_locksmbhdr(smb)!=0)  				/* Be sure noone deletes or */
		return(NULL);							/* adds while we're reading */

	smb_rewind(smb->sid_fp);
	while(!smb_feof(smb->sid_fp)) {
		if(smb_fread(&idx,sizeof(idx),smb->sid_fp) != sizeof(idx))
			break;
		if(idx.number==0)	/* invalid message number, ignore */
			continue;
		if((which==MAIL_SENT && idx.from!=usernumber)
			|| (which==MAIL_YOUR && idx.to!=usernumber)
			|| (which==MAIL_ANY && idx.from!=usernumber && idx.to!=usernumber))
			continue;
		if(idx.attr&MSG_DELETE && !(mode&LM_INCDEL))	/* Don't included deleted msgs */
			continue;					
		if(mode&LM_UNREAD && idx.attr&MSG_READ)
			continue;
		if((mail=(mail_t *)REALLOC(mail,sizeof(mail_t)*(l+1)))
			==NULL) {
			smb_unlocksmbhdr(smb);
			return(NULL); 
		}
		mail[l]=idx;
		l++; 
	}
	smb_unlocksmbhdr(smb);
	*msgs=l;
	return(mail);
}

void DLLCALL freemail(mail_t* mail)
{
	FREE(mail);
}
