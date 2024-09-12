/* Synchronet hi-level data access routines */

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

static void ProgressLoadingMsgPtrs(void* cbdata, int count, int total)
{
	sbbs_t* sbbs = ((sbbs_t*)cbdata);
	sbbs->progress(sbbs->text[LoadingMsgPtrs], count, total);
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
	if(useron.number == 0 || useron.rest&FLAG('G'))
		return;
	char path[MAX_PATH + 1];
	msgptrs_filename(&cfg, useron.number, path, sizeof path);
	const uint access = O_RDWR | O_CREAT | O_TEXT;
	FILE* fp = fnopen(NULL, path, access);
	if (fp == NULL) {
		errormsg(WHERE, ERR_OPEN, path, access);
	} else {
		if(!putmsgptrs_fp(&cfg,&useron,subscan, fp))
			errormsg(WHERE, ERR_WRITE, "message pointers", 0);
		fclose(fp);
	}
}

void sbbs_t::reinit_msg_ptrs()
{
	for(int i = 0; i < cfg.total_subs; ++i) {
		subscan[i].ptr = subscan[i].sav_ptr;
		subscan[i].last = subscan[i].sav_last;
	}
}

static void ProgressSearchingUsers(void* cbdata, int count, int total)
{
	sbbs_t* sbbs = ((sbbs_t*)cbdata);
	sbbs->progress(sbbs->text[SearchingForDupes], count, total);
}

/****************************************************************************/
/* Checks for a duplicate user field starting at user record offset         */
/* 'offset', reading in 'datlen' chars, comparing to 'str' for each user    */
/* except 'usernumber' if it is non-zero, or starting at 'usernumber' if    */
/* 'next' is true. Comparison is NOT case sensitive.                        */
/* 'del' is true if the search is to include deleted/inactive users			*/
/* Returns the usernumber of the dupe if found, 0 if not                    */
/****************************************************************************/
uint sbbs_t::finduserstr(uint usernumber, enum user_field fnum, const char* str
    ,bool del, bool next)
{
	uint i=::finduserstr(&cfg, usernumber, fnum, str, del, next, online == ON_REMOTE ? ProgressSearchingUsers : NULL, this);
	if(online == ON_REMOTE)
		bputs(text[SearchedForDupes]);
	return(i);
}

bool sbbs_t::putuserstr(int usernumber, enum user_field fnum, const char *str)
{
	int result = ::putuserstr(&cfg, usernumber, fnum, str);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserdatetime(int usernumber, enum user_field fnum, time_t t)
{
	int result = ::putuserdatetime(&cfg, usernumber, fnum, (time32_t)t);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserflags(int usernumber, enum user_field fnum, uint32_t flags)
{
	int result = ::putuserflags(&cfg, usernumber, fnum, flags);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserhex32(int usernumber, enum user_field fnum, uint32_t value)
{
	int result = ::putuserhex32(&cfg, usernumber, fnum, value);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserdec32(int usernumber, enum user_field fnum, uint32_t value)
{
	int result = ::putuserdec32(&cfg, usernumber, fnum, value);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserdec64(int usernumber, enum user_field fnum, uint64_t value)
{
	int result = ::putuserdec64(&cfg, usernumber, fnum, value);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putusermisc(int usernumber, uint32_t value)
{
	int result = ::putusermisc(&cfg, usernumber, value);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserchat(int usernumber, uint32_t value)
{
	int result = ::putuserchat(&cfg, usernumber, value);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}

bool sbbs_t::putuserqwk(int usernumber, uint32_t value)
{
	int result = ::putuserqwk(&cfg, usernumber, value);
	if(result != 0) {
		errormsg(WHERE, ERR_WRITE, USER_DATA_FILENAME, result);
		return false;
	}
	return true;
}
