/* Synchronet text data-related routines (exported) */

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

#include "dat_rec.h"
#include "gen_defs.h"
#include <string.h>

/****************************************************************************/
/* Places into 'strout' CR or ETX terminated string starting at             */
/* 'start' and ending at 'start'+'length' or terminator from 'strin'        */
/****************************************************************************/
int getrec(const char *strin,int start,int length,char *strout)
{
    int i=0,stop;

	stop=start+length;
	while(start<stop) {
		if(strin[start]==ETX || strin[start]==CR || strin[start]==LF)
			break;
		strout[i++]=strin[start++]; 
	}
	strout[i]=0;
	return i;
}

/****************************************************************************/
/* Places into 'strout', 'strin' starting at 'start' and ending at          */
/* 'start'+'length'                                                         */
/****************************************************************************/
void putrec(char *strout,int start,int length, const char *strin)
{
    int i=0,j;

	j=strlen(strin);
	while(i<j && i<length)
		strout[start++]=strin[i++];
	while(i++<length)
		strout[start++]=ETX;
}
