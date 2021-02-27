/* Copyright 2004 by Darryl Perry
 * 
 * This file is part of Space Dynasty Elite.
 * 
 * Space Dynasty Elite is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Foobar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Foobar; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "dynasty.h"
#include <OpenDoor.h>
#include "mci.h"

void text(char *lnum, ...)
{
/*
	va_list ap;

	text1(lnum,strng);

	va_start(ap,&strng);
	vsprintf(s,strng,ap);
	va_end(ap);
	mci(s);
*/
	
}

void text1(char *lnum, char *ttxt)
{
	FILE *fptr;
	char ttxt1[161];
	char ppath[81];

	strcpy(ppath,"data.txt");

	fptr=fopen(ppath,"rt");
	if(fptr==NULL) {
		nl();
		mci("Unable to open %s",ppath);nl();
		nl();
		}
	else {
		while(fgets(ttxt1,161,fptr)!=NULL){
			if(strncasecmp(lnum,ttxt1,strlen(lnum))==0){
				ttxt1[strlen(ttxt1)-1]=0;
				strcpy(ttxt,ttxt1+5);
				break;
				}
			}
		}
	fclose(fptr);
}

void runmci(char *ttxt,int showit)
{
	int doafter=FALSE;

	while(strstr(ttxt,"|"))		strrepl(ttxt,500,"|"," ");
	while(strstr(ttxt,"~EC"))	strrepl(ttxt,500,"~EC","\x1b[");
	while(strstr(ttxt,"~SM"))	strrepl(ttxt,500,"~SM","\r\n");
	while(strstr(ttxt,"~SC"))	strrepl(ttxt,500,"~SC","\x1b[2J\x1b[1;1H");
	if(showit){
		while(strstr(ttxt,"~SP")){	strrepl(ttxt,500,"~SP","");	doafter=1;}
		}
	else{
		while(strstr(ttxt,"~SP"))	strrepl(ttxt,500,"~SP","");	
		}

	while(strstr(ttxt,"`00"))	strrepl(ttxt,500,"`00","\x1b[0;30;40m");
	while(strstr(ttxt,"`01"))	strrepl(ttxt,500,"`01","\x1b[0;34;40m");
	while(strstr(ttxt,"`02"))	strrepl(ttxt,500,"`02","\x1b[0;32;40m");
	while(strstr(ttxt,"`03"))	strrepl(ttxt,500,"`03","\x1b[0;36;40m");
	while(strstr(ttxt,"`04"))	strrepl(ttxt,500,"`04","\x1b[0;31;40m");
	while(strstr(ttxt,"`05"))	strrepl(ttxt,500,"`05","\x1b[0;35;40m");
	while(strstr(ttxt,"`06"))	strrepl(ttxt,500,"`06","\x1b[0;33;40m");
	while(strstr(ttxt,"`07"))	strrepl(ttxt,500,"`07","\x1b[0;37;40m");
	while(strstr(ttxt,"`08"))	strrepl(ttxt,500,"`08","\x1b[1;30;40m");
	while(strstr(ttxt,"`09"))	strrepl(ttxt,500,"`09","\x1b[1;34;40m");
	while(strstr(ttxt,"`0A"))	strrepl(ttxt,500,"`0A","\x1b[1;32;40m");
	while(strstr(ttxt,"`0B"))	strrepl(ttxt,500,"`0B","\x1b[1;36;40m");
	while(strstr(ttxt,"`0C"))	strrepl(ttxt,500,"`0C","\x1b[1;31;40m");
	while(strstr(ttxt,"`0D"))	strrepl(ttxt,500,"`0D","\x1b[1;35;40m");
	while(strstr(ttxt,"`0E"))	strrepl(ttxt,500,"`0E","\x1b[1;33;40m");
	while(strstr(ttxt,"`0F"))	strrepl(ttxt,500,"`0F","\x1b[1;37;40m");
	
	while(strstr(ttxt,"`10"))	strrepl(ttxt,500,"`10","\x1b[0;30;44m");
	while(strstr(ttxt,"`11"))	strrepl(ttxt,500,"`11","\x1b[0;34;44m");
	while(strstr(ttxt,"`12"))	strrepl(ttxt,500,"`12","\x1b[0;32;44m");
	while(strstr(ttxt,"`13"))	strrepl(ttxt,500,"`13","\x1b[0;36;44m");
	while(strstr(ttxt,"`14"))	strrepl(ttxt,500,"`14","\x1b[0;31;44m");
	while(strstr(ttxt,"`15"))	strrepl(ttxt,500,"`15","\x1b[0;35;44m");
	while(strstr(ttxt,"`16"))	strrepl(ttxt,500,"`16","\x1b[0;33;44m");
	while(strstr(ttxt,"`17"))	strrepl(ttxt,500,"`17","\x1b[0;37;44m");
	while(strstr(ttxt,"`18"))	strrepl(ttxt,500,"`18","\x1b[1;30;44m");
	while(strstr(ttxt,"`19"))	strrepl(ttxt,500,"`19","\x1b[1;34;44m");
	while(strstr(ttxt,"`1A"))	strrepl(ttxt,500,"`1A","\x1b[1;32;44m");
	while(strstr(ttxt,"`1B"))	strrepl(ttxt,500,"`1B","\x1b[1;36;44m");
	while(strstr(ttxt,"`1C"))	strrepl(ttxt,500,"`1C","\x1b[1;31;44m");
	while(strstr(ttxt,"`1D"))	strrepl(ttxt,500,"`1D","\x1b[1;35;44m");
	while(strstr(ttxt,"`1E"))	strrepl(ttxt,500,"`1E","\x1b[1;33;44m");
	while(strstr(ttxt,"`1F"))	strrepl(ttxt,500,"`1F","\x1b[1;37;44m");

	while(strstr(ttxt,"`20"))	strrepl(ttxt,500,"`20","\x1b[0;30;42m");
	while(strstr(ttxt,"`21"))	strrepl(ttxt,500,"`21","\x1b[0;34;42m");
	while(strstr(ttxt,"`22"))	strrepl(ttxt,500,"`22","\x1b[0;32;42m");
	while(strstr(ttxt,"`23"))	strrepl(ttxt,500,"`23","\x1b[0;36;42m");
	while(strstr(ttxt,"`24"))	strrepl(ttxt,500,"`24","\x1b[0;31;42m");
	while(strstr(ttxt,"`25"))	strrepl(ttxt,500,"`25","\x1b[0;35;42m");
	while(strstr(ttxt,"`26"))	strrepl(ttxt,500,"`26","\x1b[0;33;42m");
	while(strstr(ttxt,"`27"))	strrepl(ttxt,500,"`27","\x1b[0;37;42m");
	while(strstr(ttxt,"`28"))	strrepl(ttxt,500,"`28","\x1b[1;30;42m");
	while(strstr(ttxt,"`29"))	strrepl(ttxt,500,"`29","\x1b[1;34;42m");
	while(strstr(ttxt,"`2A"))	strrepl(ttxt,500,"`2A","\x1b[1;32;42m");
	while(strstr(ttxt,"`2B"))	strrepl(ttxt,500,"`2B","\x1b[1;36;42m");
	while(strstr(ttxt,"`2C"))	strrepl(ttxt,500,"`2C","\x1b[1;31;42m");
	while(strstr(ttxt,"`2D"))	strrepl(ttxt,500,"`2D","\x1b[1;35;42m");
	while(strstr(ttxt,"`2E"))	strrepl(ttxt,500,"`2E","\x1b[1;33;42m");
	while(strstr(ttxt,"`2F"))	strrepl(ttxt,500,"`2F","\x1b[1;37;42m");

	while(strstr(ttxt,"`30"))	strrepl(ttxt,500,"`30","\x1b[0;30;46m");
	while(strstr(ttxt,"`31"))	strrepl(ttxt,500,"`31","\x1b[0;34;46m");
	while(strstr(ttxt,"`32"))	strrepl(ttxt,500,"`32","\x1b[0;32;46m");
	while(strstr(ttxt,"`33"))	strrepl(ttxt,500,"`33","\x1b[0;36;46m");
	while(strstr(ttxt,"`34"))	strrepl(ttxt,500,"`34","\x1b[0;31;46m");
	while(strstr(ttxt,"`35"))	strrepl(ttxt,500,"`35","\x1b[0;35;46m");
	while(strstr(ttxt,"`36"))	strrepl(ttxt,500,"`36","\x1b[0;33;46m");
	while(strstr(ttxt,"`37"))	strrepl(ttxt,500,"`37","\x1b[0;37;46m");
	while(strstr(ttxt,"`38"))	strrepl(ttxt,500,"`38","\x1b[1;30;46m");
	while(strstr(ttxt,"`39"))	strrepl(ttxt,500,"`39","\x1b[1;34;46m");
	while(strstr(ttxt,"`3A"))	strrepl(ttxt,500,"`3A","\x1b[1;32;46m");
	while(strstr(ttxt,"`3B"))	strrepl(ttxt,500,"`3B","\x1b[1;36;46m");
	while(strstr(ttxt,"`3C"))	strrepl(ttxt,500,"`3C","\x1b[1;31;46m");
	while(strstr(ttxt,"`3D"))	strrepl(ttxt,500,"`3D","\x1b[1;35;46m");
	while(strstr(ttxt,"`3E"))	strrepl(ttxt,500,"`3E","\x1b[1;33;46m");
	while(strstr(ttxt,"`3F"))	strrepl(ttxt,500,"`3F","\x1b[1;37;46m");

	while(strstr(ttxt,"`40"))	strrepl(ttxt,500,"`40","\x1b[0;30;41m");
	while(strstr(ttxt,"`41"))	strrepl(ttxt,500,"`41","\x1b[0;34;41m");
	while(strstr(ttxt,"`42"))	strrepl(ttxt,500,"`42","\x1b[0;32;41m");
	while(strstr(ttxt,"`43"))	strrepl(ttxt,500,"`43","\x1b[0;36;41m");
	while(strstr(ttxt,"`44"))	strrepl(ttxt,500,"`44","\x1b[0;31;41m");
	while(strstr(ttxt,"`45"))	strrepl(ttxt,500,"`45","\x1b[0;35;41m");
	while(strstr(ttxt,"`46"))	strrepl(ttxt,500,"`46","\x1b[0;33;41m");
	while(strstr(ttxt,"`47"))	strrepl(ttxt,500,"`47","\x1b[0;37;41m");
	while(strstr(ttxt,"`48"))	strrepl(ttxt,500,"`48","\x1b[1;30;41m");
	while(strstr(ttxt,"`49"))	strrepl(ttxt,500,"`49","\x1b[1;34;41m");
	while(strstr(ttxt,"`4A"))	strrepl(ttxt,500,"`4A","\x1b[1;32;41m");
	while(strstr(ttxt,"`4B"))	strrepl(ttxt,500,"`4B","\x1b[1;36;41m");
	while(strstr(ttxt,"`4C"))	strrepl(ttxt,500,"`4C","\x1b[1;31;41m");
	while(strstr(ttxt,"`4D"))	strrepl(ttxt,500,"`4D","\x1b[1;35;41m");
	while(strstr(ttxt,"`4E"))	strrepl(ttxt,500,"`4E","\x1b[1;33;41m");
	while(strstr(ttxt,"`4F"))	strrepl(ttxt,500,"`4F","\x1b[1;37;41m");

	while(strstr(ttxt,"`50"))	strrepl(ttxt,500,"`50","\x1b[0;30;45m");
	while(strstr(ttxt,"`51"))	strrepl(ttxt,500,"`51","\x1b[0;34;45m");
	while(strstr(ttxt,"`52"))	strrepl(ttxt,500,"`52","\x1b[0;32;45m");
	while(strstr(ttxt,"`53"))	strrepl(ttxt,500,"`53","\x1b[0;36;45m");
	while(strstr(ttxt,"`54"))	strrepl(ttxt,500,"`54","\x1b[0;31;45m");
	while(strstr(ttxt,"`55"))	strrepl(ttxt,500,"`55","\x1b[0;35;45m");
	while(strstr(ttxt,"`56"))	strrepl(ttxt,500,"`56","\x1b[0;33;45m");
	while(strstr(ttxt,"`57"))	strrepl(ttxt,500,"`57","\x1b[0;37;45m");
	while(strstr(ttxt,"`58"))	strrepl(ttxt,500,"`58","\x1b[1;30;45m");
	while(strstr(ttxt,"`59"))	strrepl(ttxt,500,"`59","\x1b[1;34;45m");
	while(strstr(ttxt,"`5A"))	strrepl(ttxt,500,"`5A","\x1b[1;32;45m");
	while(strstr(ttxt,"`5B"))	strrepl(ttxt,500,"`5B","\x1b[1;36;45m");
	while(strstr(ttxt,"`5C"))	strrepl(ttxt,500,"`5C","\x1b[1;31;45m");
	while(strstr(ttxt,"`5D"))	strrepl(ttxt,500,"`5D","\x1b[1;35;45m");
	while(strstr(ttxt,"`5E"))	strrepl(ttxt,500,"`5E","\x1b[1;33;45m");
	while(strstr(ttxt,"`5F"))	strrepl(ttxt,500,"`5F","\x1b[1;37;45m");
	
	while(strstr(ttxt,"`60"))	strrepl(ttxt,500,"`60","\x1b[0;30;43m");
	while(strstr(ttxt,"`61"))	strrepl(ttxt,500,"`61","\x1b[0;34;43m");
	while(strstr(ttxt,"`62"))	strrepl(ttxt,500,"`62","\x1b[0;32;43m");
	while(strstr(ttxt,"`63"))	strrepl(ttxt,500,"`63","\x1b[0;36;43m");
	while(strstr(ttxt,"`64"))	strrepl(ttxt,500,"`64","\x1b[0;31;43m");
	while(strstr(ttxt,"`65"))	strrepl(ttxt,500,"`65","\x1b[0;35;43m");
	while(strstr(ttxt,"`66"))	strrepl(ttxt,500,"`66","\x1b[0;33;43m");
	while(strstr(ttxt,"`67"))	strrepl(ttxt,500,"`67","\x1b[0;37;43m");
	while(strstr(ttxt,"`68"))	strrepl(ttxt,500,"`68","\x1b[1;30;43m");
	while(strstr(ttxt,"`69"))	strrepl(ttxt,500,"`69","\x1b[1;34;43m");
	while(strstr(ttxt,"`6A"))	strrepl(ttxt,500,"`6A","\x1b[1;32;43m");
	while(strstr(ttxt,"`6B"))	strrepl(ttxt,500,"`6B","\x1b[1;36;43m");
	while(strstr(ttxt,"`6C"))	strrepl(ttxt,500,"`6C","\x1b[1;31;43m");
	while(strstr(ttxt,"`6D"))	strrepl(ttxt,500,"`6D","\x1b[1;35;43m");
	while(strstr(ttxt,"`6E"))	strrepl(ttxt,500,"`6E","\x1b[1;33;43m");
	while(strstr(ttxt,"`6F"))	strrepl(ttxt,500,"`6F","\x1b[1;37;43m");
	
	while(strstr(ttxt,"`70"))	strrepl(ttxt,500,"`70","\x1b[0;30;47m");
	while(strstr(ttxt,"`71"))	strrepl(ttxt,500,"`71","\x1b[0;34;47m");
	while(strstr(ttxt,"`72"))	strrepl(ttxt,500,"`72","\x1b[0;32;47m");
	while(strstr(ttxt,"`73"))	strrepl(ttxt,500,"`73","\x1b[0;36;47m");
	while(strstr(ttxt,"`74"))	strrepl(ttxt,500,"`74","\x1b[0;31;47m");
	while(strstr(ttxt,"`75"))	strrepl(ttxt,500,"`75","\x1b[0;35;47m");
	while(strstr(ttxt,"`76"))	strrepl(ttxt,500,"`76","\x1b[0;33;47m");
	while(strstr(ttxt,"`77"))	strrepl(ttxt,500,"`77","\x1b[0;37;47m");
	while(strstr(ttxt,"`78"))	strrepl(ttxt,500,"`78","\x1b[1;30;47m");
	while(strstr(ttxt,"`79"))	strrepl(ttxt,500,"`79","\x1b[1;34;47m");
	while(strstr(ttxt,"`7A"))	strrepl(ttxt,500,"`7A","\x1b[1;32;47m");
	while(strstr(ttxt,"`7B"))	strrepl(ttxt,500,"`7B","\x1b[1;36;47m");
	while(strstr(ttxt,"`7C"))	strrepl(ttxt,500,"`7C","\x1b[1;31;47m");
	while(strstr(ttxt,"`7D"))	strrepl(ttxt,500,"`7D","\x1b[1;35;47m");
	while(strstr(ttxt,"`7E"))	strrepl(ttxt,500,"`7E","\x1b[1;33;47m");
	while(strstr(ttxt,"`7F"))	strrepl(ttxt,500,"`7F","\x1b[1;37;47m");

	while(strstr(ttxt,"`80"))	strrepl(ttxt,500,"`80","\x1b[0;5;30;47m");
	while(strstr(ttxt,"`81"))	strrepl(ttxt,500,"`81","\x1b[0;5;34;47m");
	while(strstr(ttxt,"`82"))	strrepl(ttxt,500,"`82","\x1b[0;5;32;47m");
	while(strstr(ttxt,"`83"))	strrepl(ttxt,500,"`83","\x1b[0;5;36;47m");
	while(strstr(ttxt,"`84"))	strrepl(ttxt,500,"`84","\x1b[0;5;31;47m");
	while(strstr(ttxt,"`85"))	strrepl(ttxt,500,"`85","\x1b[0;5;35;47m");
	while(strstr(ttxt,"`86"))	strrepl(ttxt,500,"`86","\x1b[0;5;33;47m");
	while(strstr(ttxt,"`87"))	strrepl(ttxt,500,"`87","\x1b[0;5;37;47m");
	while(strstr(ttxt,"`88"))	strrepl(ttxt,500,"`88","\x1b[1;5;30;47m");
	while(strstr(ttxt,"`89"))	strrepl(ttxt,500,"`89","\x1b[1;5;34;47m");
	while(strstr(ttxt,"`8A"))	strrepl(ttxt,500,"`8A","\x1b[1;5;32;47m");
	while(strstr(ttxt,"`8B"))	strrepl(ttxt,500,"`8B","\x1b[1;5;36;47m");
	while(strstr(ttxt,"`8C"))	strrepl(ttxt,500,"`8C","\x1b[1;5;31;47m");
	while(strstr(ttxt,"`8D"))	strrepl(ttxt,500,"`8D","\x1b[1;5;35;47m");
	while(strstr(ttxt,"`8E"))	strrepl(ttxt,500,"`8E","\x1b[1;5;33;47m");
	while(strstr(ttxt,"`8F"))	strrepl(ttxt,500,"`8F","\x1b[1;5;37;47m");

/*	if(showit<2) */
	if(showit)
/*		printf("%s",ttxt); */
		od_disp_emu(ttxt,TRUE);
	switch(doafter){
		case 0:	break;
		case 1:	pausescr();	break;
		}
}


char *strrepl(char *Str, size_t BufSiz, char *OldStr, char *NewStr)
{
	int OldLen, NewLen;
	char *p, *q;

	p = (char *)strstr(Str, OldStr);
	if(p==(char *)NULL)
		return Str;
	OldLen = strlen(OldStr);
	NewLen = strlen(NewStr);
	if ((strlen(Str) + NewLen - OldLen + 1) > BufSiz)
		return NULL;
	memmove(q = p+NewLen, p+OldLen, strlen(p+OldLen)+1);
	memcpy(p, NewStr, NewLen);
	return q;
}

void mci(char *strng, ...)
{
	char s[512],ttxt[512];
	va_list ap;

	va_start(ap,strng);
	vsprintf(s,strng,ap);
	va_end(ap);
	strcpy(ttxt,s);
	runmci(ttxt,TRUE);
}

void mcistr(char *ostr, char *strng, ...)
{
	char s[512];
	va_list ap;

	va_start(ap,strng);
	vsprintf(s,strng,ap);
	va_end(ap);
	strcpy(ostr,s);
	runmci(ostr,FALSE);
}

void mci2(char *strng, ...)
{
	char s[512],ttxt[512];
	va_list ap;

	va_start(ap,strng);
	vsprintf(s,strng,ap);
	va_end(ap);
	strcpy(ttxt,s);
	runmci(ttxt,FALSE);
}

int mcistrlen(char *ttxt)
{
	int i,count;

	count=strlen(ttxt);
	for(i=0;i<strlen(ttxt);i++) {
		if(ttxt[i]=='`'){
			count-=3;
			}
		}
	return(count);
}

void center(char *strng, ...)
{
	char s[512],ttxt[512];
	va_list ap;
	int i,cnt=0;

	va_start(ap,strng);
	vsprintf(s,strng,ap);
	va_end(ap);
	strcpy(ttxt,s);
	for(i=0;i<strlen(ttxt);i++)	
		if(ttxt[i]=='`' || ttxt[i]=='~')
				cnt++;
	cnt--;
	cnt*=3;
	for(i=0;i<40-((strlen(ttxt)-cnt)/2);i++)	mci(" ");
	runmci(ttxt,TRUE);
}
