/* Synchronet hi-level data access routines */

/* $Id: data_ovl.cpp,v 1.28 2019/02/17 06:20:26 rswindell Exp $ */

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

static void ProgressLoadingMsgPtrs(void* cbdata, int count, int total)
{
	sbbs_t* sbbs = ((sbbs_t*)cbdata);
	sbbs->progress(sbbs->text[LoadingMsgPtrs], count, total, 10);
}

/****************************************************************************/
/* Fills the 'ptr' element of the each element of the cfg.sub[] array of sub_t  */
/* and the sub_cfg and sub_ptr global variables                            */
/* Called from function main                                                */
/****************************************************************************/
void sbbs_t::getmsgptrs()
{
	if(!useron.number)
		return;
	::getmsgptrs(&cfg,&useron,subscan,online == ON_REMOTE ? ProgressLoadingMsgPtrs : NULL,this);
	if(online == ON_REMOTE)
		bputs(text[LoadedMsgPtrs]);
}

void sbbs_t::putmsgptrs()
{
	if(!::putmsgptrs(&cfg,&useron,subscan))
		errormsg(WHERE, ERR_WRITE, "message pointers", 0);
}

static void ProgressSearchingUsers(void* cbdata, int count, int total)
{
	sbbs_t* sbbs = ((sbbs_t*)cbdata);
	sbbs->progress(sbbs->text[SearchingForDupes], count, total, U_LEN*10);
}

/****************************************************************************/
/* Checks for a duplicate user field starting at user record offset         */
/* 'offset', reading in 'datlen' chars, comparing to 'str' for each user    */
/* except 'usernumber' if it is non-zero, or starting at 'usernumber' if    */
/* 'next' is true. Comparison is NOT case sensitive.                        */
/* 'del' is true if the search is to include deleted/inactive users			*/
/* Returns the usernumber of the dupe if found, 0 if not                    */
/****************************************************************************/
uint sbbs_t::userdatdupe(uint usernumber, uint offset, uint datlen, char *dat
    ,bool del, bool next)
{
	uint i=::userdatdupe(&cfg, usernumber, offset, datlen, dat, del, next, online == ON_REMOTE ? ProgressSearchingUsers : NULL, this);
	if(online == ON_REMOTE)
		bputs(text[SearchedForDupes]);
	return(i);
}
