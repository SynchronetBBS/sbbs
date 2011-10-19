/*****************************************************************************
 *
 * File ..................: v3_mci.c
 * Purpose ...............: Formatted output
 * Last modification date : 16-Mar-2001
 *
 *****************************************************************************
 * Copyright (C) 1999-2001
 *
 * Darryl Perry		FIDO:		1:211/105
 * Sacramento, CA.
 * USA
 *
 * This file is part of Virtual BBS.
 *
 * This Game is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Virtual BBS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Virutal BBS; see the file COPYING.  If not, write to the
 * Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if defined(_MSC_VER)
#include "vc_strings.h"
#elif defined(__BORLANDC__)
#include "bc_str.h"
#else
#include <strings.h>
#endif

#include "vbbs.h"
#include "v3_defs.h"
#include "v3_mci.h"
#include "v3_basic.h"

char A[24][81];

#define NUL '\0'

size_t commafmt_s32(char* buf,      /* Buffer for formatted string  */
                int bufsize,        /* Size of buffer               */
                s32 N)              /* Number to convert            */
{
      int len = 1, posn = 1, sign = 1;
      char *ptr = buf + bufsize - 1;

      if (2 > bufsize)
      {
ABORT:      *buf = NUL;
            return 0;
      }

      *ptr-- = NUL;
      --bufsize;
      if (0L > N)
      {
            sign = -1;
            N = -N;
      }

      for ( ; len <= bufsize; ++len, ++posn)
      {
            *ptr-- = (char)((N % 10L) + '0');
            if (0L == (N /= 10L))
                  break;
            if (0 == (posn % 3))
            {
                  *ptr-- = ',';
                  ++len;
            }
            if (len >= bufsize)
                  goto ABORT;
      }

      if (0 > sign)
      {
            if (0 == bufsize)
                  goto ABORT;
            *ptr-- = '-';
            ++len;
      }

      strcpy(buf, ++ptr);
      return (size_t)len;
}

size_t commafmt_u32(char* buf,		/* Buffer for formatted string  */
                int bufsize,        /* Size of buffer               */
                u32 N)              /* Number to convert            */
{
      int len = 1, posn = 1;
      char *ptr = buf + bufsize - 1;

      if (2 > bufsize)
      {
ABORT:      *buf = NUL;
            return 0;
      }

      *ptr-- = NUL;
      --bufsize;

      for ( ; len <= bufsize; ++len, ++posn)
      {
            *ptr-- = (char)((N % 10L) + '0');
            if (0L == (N /= 10L))
                  break;
            if (0 == (posn % 3))
            {
                  *ptr-- = ',';
                  ++len;
            }
            if (len >= bufsize)
                  goto ABORT;
      }

      strcpy(buf, ++ptr);
      return (size_t)len;
}


void text(char *lnum)
{
	FILE *fptr;
	char ttxt[161];
	char path[_MAX_PATH];

	sprintf(path,"%stext.dat",gamedir);

	fptr=fopen(path,"rt");
	if(fptr==NULL) {
		mci("~SMUnable to open text.dat~SM~SM");
		}
	else {
		while(fgets(ttxt,161,fptr)!=NULL){
			if(strncasecmp(lnum,ttxt,strlen(lnum))==0){
				stripCR(ttxt);
				memmove(ttxt,ttxt+5,strlen(ttxt)-4);
				mci(ttxt);
				}
			}
		}
	fclose(fptr);
}

void mcigettext(char *lnum, char *outtext)
{
	FILE *fptr;
	char path[_MAX_PATH];

	sprintf(path,"%stext.dat",gamedir);
	fptr=fopen(path,"rt");
	if(fptr==NULL) {
		mci("~SMUnable to open %s~SM~SM",path);
		}
	else {
		while(fgets(outtext,161,fptr)!=NULL){
			if(strncasecmp(lnum,outtext,strlen(lnum))==0){
				stripCR(outtext);
				memcpy(outtext,outtext+5,strlen(outtext)-4);
				break;
				}
			}
		}
	fclose(fptr);
}

void mcimacros(char *ttxt)
{
	mciAnd(ttxt);
}

void mci(char *fmt, ...)
{
	va_list argptr;
	char buffer[512];

	va_start(argptr, fmt);
	vsprintf(buffer, fmt, argptr);
	va_end(argptr);

	mciEseries(buffer);
	mcimacros(buffer);
	mcihidden(buffer);
	mcicolor(buffer,od_control.user_ansi);
	mcifuncts(buffer);
}

void stripCR(char *ttxt)
{
	while(strstr(ttxt,"\n"))	strrepl(ttxt,500,"\n","");
	while(strstr(ttxt,"\r"))	strrepl(ttxt,500,"\r","");
}


char *strrepl(char *Str, size_t BufSiz, const char *OldStr, const char *NewStr)
{
	int OldLen, NewLen;
	char *p, *q;

	if(NULL == (p = strstr(Str, OldStr)))
		return Str;
	OldLen = strlen(OldStr);
	NewLen = strlen(NewStr);
	if ((strlen(Str) + NewLen - OldLen + 1) > BufSiz)
		return NULL;
	memmove(q = p+NewLen, p+OldLen, strlen(p+OldLen)+1);
	memcpy(p, NewStr, NewLen);
	return q;
}

void mciAnd(char *ttxt)
{
	while(strstr(ttxt,"~&0"))	strrepl(ttxt,500,"~&0",A[0]);
	while(strstr(ttxt,"~&1"))	strrepl(ttxt,500,"~&1",A[1]);
	while(strstr(ttxt,"~&2"))	strrepl(ttxt,500,"~&2",A[2]);
	while(strstr(ttxt,"~&3"))	strrepl(ttxt,500,"~&3",A[3]);
	while(strstr(ttxt,"~&4"))	strrepl(ttxt,500,"~&4",A[4]);
	while(strstr(ttxt,"~&5"))	strrepl(ttxt,500,"~&5",A[5]);
	while(strstr(ttxt,"~&6"))	strrepl(ttxt,500,"~&6",A[6]);
	while(strstr(ttxt,"~&7"))	strrepl(ttxt,500,"~&7",A[7]);
	while(strstr(ttxt,"~&8"))	strrepl(ttxt,500,"~&8",A[8]);
	while(strstr(ttxt,"~&9"))	strrepl(ttxt,500,"~&9",A[9]);
	while(strstr(ttxt,"~&A"))	strrepl(ttxt,500,"~&A",A[10]);
	while(strstr(ttxt,"~&B"))	strrepl(ttxt,500,"~&B",A[11]);
	while(strstr(ttxt,"~&C"))	strrepl(ttxt,500,"~&C",A[12]);
	while(strstr(ttxt,"~&D"))	strrepl(ttxt,500,"~&D",A[13]);
	while(strstr(ttxt,"~&E"))	strrepl(ttxt,500,"~&E",A[14]);
	while(strstr(ttxt,"~&F"))	strrepl(ttxt,500,"~&F",A[15]);
	while(strstr(ttxt,"~&G"))	strrepl(ttxt,500,"~&G",A[16]);
	while(strstr(ttxt,"~&H"))	strrepl(ttxt,500,"~&H",A[17]);
	while(strstr(ttxt,"~&I"))	strrepl(ttxt,500,"~&I",A[18]);
	while(strstr(ttxt,"~&J"))	strrepl(ttxt,500,"~&J",A[19]);
	while(strstr(ttxt,"~&K"))	strrepl(ttxt,500,"~&K",A[20]);
	while(strstr(ttxt,"~&L"))	strrepl(ttxt,500,"~&L",A[21]);
	while(strstr(ttxt,"~&M"))	strrepl(ttxt,500,"~&M",A[22]);
	while(strstr(ttxt,"~&N"))	strrepl(ttxt,500,"~&N",A[23]);
}


void mciEseries(char *ttxt)
{
	char *p,t1[512],ch=0,dump[41];
	int i,num=0,num1=0;

	while(strstr(ttxt,"~EC"))	strrepl(ttxt,500,"~EC","\x1b[");

	while ((p=strstr(ttxt,"~EE")) != 0)	/*  Create pattern of spaces to len n */
	{
		if(isdigit(*(p+3))){
			num=*(p+3)-'0';
			}
		if(isdigit(*(p+4))){
			num*=10;
			num+= (*(p+4)-'0');
			}

		for(num1=0;num1<num;num1++) t1[num1]=' ';

		t1[num1]='\0';
		sprintf(dump,"~EE%c%d",ch,num);
		strrepl(ttxt,500,dump,t1);
	}

	while((p=strstr(ttxt,"~EF")) != 0)	/*  Create fill pattern of c to len n */
	{
		ch=*(p+3);
		if(isdigit(*(p+4))){
			num=*(p+4)-'0';
			}
		if(isdigit(*(p+5))){
			num*=10;
			num+= (*(p+5)-'0');
			}

		for(num1=0;num1<num;num1++) t1[num1]=ch;

		t1[num1]='\0';
		sprintf(dump,"~EF%c%d",ch,num);
		strrepl(ttxt,500,dump,t1);
	}

	while((p=strstr(ttxt,"~EL")) != 0)
	{
		i=3;num=0;
		while(isdigit( *(p+i) ) ){
			num=(num*10) + (*(p+i)-'0');
			i++;
			}

		t1[0]=*(p+i); i++;
		t1[1]=*(p+i); i++;
		t1[2]=*(p+i); i++;
		t1[3]='\0';

		sprintf(dump,"~EL%d%s",num,t1);
		mcimacros(t1);
		mcistrset(t1,num);
		strrepl(ttxt,500,dump,t1);
	}

	while((p=strstr(ttxt,"~EC")) != 0) /* Center & Justify, expand to n */
	{
		dump[0]=*(p+3); dump[1]=*(p+4); dump[2]='\0'; num=atoi(dump);
		t1[0]=*(p+5); t1[1]=*(p+6); t1[2]=*(p+7); t1[3]='\0';

		sprintf(dump,"~EK%d%s",num,t1);
		mcimacros(t1);
		mcistrcset(t1,num);
		strrepl(ttxt,500,dump,t1);
	}
	while((p=strstr(ttxt,"~EN")) != 0)	/* Strip all spaces*/
	{
		t1[0]=*(p+3); t1[1]=*(p+4); t1[2]=*(p+5); t1[3]='\0';

		sprintf(dump,"~EN%s",t1);
		mcimacros(t1);
		while(strstr(t1," "))	strrepl(t1,500," ","");
		strrepl(ttxt,500,dump,t1);
	}

	while((p=strstr(ttxt,"~ER")) != 0)
	{
		i=3;num=0;
		while(isdigit( *(p+i) ) ){
			num=(num*10) + (*(p+i)-'0');
			i++;
			}

		t1[0]=*(p+i); i++;
		t1[1]=*(p+i); i++;
		t1[2]=*(p+i); i++;
		t1[3]='\0';

		sprintf(dump,"~ER%d%s",num,t1);
		mcimacros(t1);
		mcistrrset(t1,num);
		strrepl(ttxt,500,dump,t1);
	}
	while((p=strstr(ttxt,"~EU")) != 0)	/* Convert to upper case */
	{
		t1[0]=*(p+3); t1[1]=*(p+4); t1[2]=*(p+5); t1[3]='\0';

		sprintf(dump,"~EU%s",t1);
		mcimacros(t1);
		strupr(t1);
		strrepl(ttxt,500,dump,t1);
	}
	while((p=strstr(ttxt,"~EW")) != 0)	/* Convert to lower case */
	{
		t1[0]=*(p+3); t1[1]=*(p+4); t1[2]=*(p+5); t1[3]='\0';

		sprintf(dump,"~EW%s",t1);
		mcimacros(t1);
		strlwr(t1);
		strrepl(ttxt,500,dump,t1);
	}
	while((p=strstr(ttxt,"~ES")) != 0)
	{
		ttxt[p-ttxt]='\0';
		mcicolor(p+3,FALSE);
		strcat(ttxt,p+3);
	}
}

int mcistrlen(char *ttxt)
{
	int i, count, len;

	len = count = strlen(ttxt);
	for(i=0;i<len;i++)
	{
		if(ttxt[i]=='`')
		{
			count-=3;
		}
	}

	return(count);
}

int mcistrlenH(char *ttxt)
{
	int i, count, len;

	len = count = strlen(ttxt);
	for(i=0;i<len;i++)
	{
		if(ttxt[i]=='`')
		{
			count-=3;
		}
	}

	return(count);
}

int oct2dec(char oct)
{
	int x=0;
	if(isdigit(oct))	x=oct-'0';
	if(isalpha(oct))	x=oct-'A'+10;
	return(x);
}

void mcicolor(char *ttxt, int onoff)
{
	char *p;
	char mcistr[5];
	char outstr[21];
	int nFG, nBG, nIN, nBK, co[8] = { 0,4,2,6,1,5,3,7 };

	while ((p=strchr(ttxt,'`')) != 0)
	{
		if (isdigit(*(p+1)) || (*(p+1)>='A' && *(p+1)<='F') )
		{
			mcistr[0]='`';
			mcistr[1]=*(p+1);
			mcistr[2]=*(p+2);
			mcistr[3]='\0';

			nFG = nBG = nIN = nBK = 0;

			/* Deal with the background first */
			nBG = oct2dec(mcistr[1]);
			if (nBG>7) { nBG-=8; nBK=1; }

			nFG = oct2dec(mcistr[2]);
			if (nFG>7) { nFG-=8; nIN=1; }

			/* if onoff==TRUE, replace MCI codes with ANSI color */
			/* if onoff==FALSE, strip out the MCI codes */
			if(onoff)
			{
				if (nBK)
					sprintf(outstr,"\x1b[%d;%d;%d;%dm", nIN,nBK*5,co[nFG]+30,co[nBG]+40);
				else
					sprintf(outstr,"\x1b[%d;%d;%dm", nIN,co[nFG]+30,co[nBG]+40);
			}
			else
				strcpy(outstr,"");
			strrepl(ttxt,500,mcistr,outstr);
		}
		else
		{
			/* replace ` with ' if it is not part of an MCI code */
			strrepl(ttxt,500,"`","'");
		}
	}
}

void mcihidden(char *ttxt)
{
	int i;
	char outstr[81];

	while(strstr(ttxt,"~SL"))	strrepl(ttxt,500,"~SL","");	// end of line
	while(strstr(ttxt,"~SM"))	strrepl(ttxt,500,"~SM","\r\n");
	while(strstr(ttxt,"~SC"))	{
		if(od_control.user_ansi)
			strrepl(ttxt,500,"~SC","\x1b[2J\x1b[1;1H");
		else{
			for(i=0;i<20;i++)
				outstr[i]='\n';
			outstr[i]='\0';
			strrepl(ttxt,500,"~SC",outstr);
			}
		}
}

void mcifuncts(char *ttxt)
{
	int cnt=0;
	while(strstr(ttxt,"~SP"))	{
		strrepl(ttxt,500,"~SP","");
		cnt++;
	}
	od_disp_emu(ttxt, TRUE);
	while(cnt){
		paws();
		cnt--;
		}
}

void stripSC(char *ttxt)
{
	while(strstr(ttxt,"~SC"))	strrepl(ttxt,500,"~SC","");
}

void str_2A(int numa, char *ttxt)
{
	strcpy(A[numa], ttxt);
}

void s16_2A(int numa, s16 num)
{
	sprintf(A[numa], "%d", num);
}

void u16_2A(int numa, u16 num)
{
	sprintf(A[numa], "%u", num);
}

#ifdef VBBS_DOS
void s32_2A(int numa, s32 lnum)
{
	sprintf(A[numa], "%ld", lnum);
}

void u32_2A(int numa, u32 lnum)
{
	sprintf(A[numa], "%lu", lnum);
}
#else // VBBS_WIN32 && VBBS_LINUX
void s32_2A(int numa, s32 lnum)
{
	sprintf(A[numa], "%d", (int32_t)lnum);
}

void u32_2A(int numa, u32 lnum)
{
	sprintf(A[numa], "%u", (uint32_t)lnum);
}
#endif

void char_2A(int numa, char chr)
{
	sprintf(A[numa], "%c", chr);
}

void s32_2C(int numa, s32 lnum)
{
	commafmt_s32(A[numa], 21, lnum);
}

void u32_2C(int numa, u32 lnum)
{
	commafmt_u32(A[numa], 21, lnum);
}

