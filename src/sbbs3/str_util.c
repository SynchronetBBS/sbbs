/* str_util.c */

/* Synchronet string utility routines */

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

/****************************************************************************/
/* Removes ctrl-a codes from the string 'instr'                             */
/****************************************************************************/
char* DLLCALL remove_ctrl_a(char *instr, char *outstr)
{
	char str[1024],*p;
	uint i,j;

	for(i=j=0;instr[i] && j<sizeof(str)-1;i++) {
		if(instr[i]==CTRL_A && instr[i+1]!=0)
			i++;
		else str[j++]=instr[i]; 
	}
	str[j]=0;
	if(outstr!=NULL)
		p=outstr;
	else
		p=instr;
	strcpy(p,str);
	return(p);
}

char* DLLCALL strip_ctrl(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i] && j<(int)sizeof(tmp)-1;i++)
		if(str[i]==CTRL_A && str[i+1]!=0)
			i++;
		else if((uchar)str[i]>=SP)
			tmp[j++]=str[i];
	tmp[j]=0;
	strcpy(str,tmp);
	return(str);
}

char* DLLCALL strip_exascii(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i] && j<(int)sizeof(tmp)-1;i++)
		if(!(str[i]&0x80))
			tmp[j++]=str[i];
	tmp[j]=0;
	strcpy(str,tmp);
	return(str);
}

char* DLLCALL prep_file_desc(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i];i++)
		if(str[i]==CTRL_A && str[i+1]!=0)
			i++;
		else if(j && str[i]<=SP && tmp[j-1]==SP)
			continue;
		else if(i && !isalnum(str[i]) && str[i]==str[i-1])
			continue;
		else if((uchar)str[i]>=SP)
			tmp[j++]=str[i];
		else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
			tmp[j++]=SP;
	tmp[j]=0;
	strcpy(str,tmp);
	return(str);
}

/****************************************************************************/
/* Pattern matching string search of 'insearchof' in 'fname'.				*/
/****************************************************************************/
BOOL DLLCALL findstr(char* insearchof, char* fname)
{
	char*	p;
	char	str[128];
	char	search[81];
	int		c;
	int		i;
	BOOL	found;
	FILE*	stream;

	if((stream=fopen(fname,"r"))==NULL)
		return(FALSE); 

	SAFECOPY(search,insearchof);
	strupr(search);

	found=FALSE;

	while(!feof(stream) && !ferror(stream) && !found) {
		if(!fgets(str,sizeof(str),stream))
			break;
		
		found=FALSE;

		p=str;	
		while(*p && *p<=' ') p++; /* Skip white-space */

		if(*p==';')		/* comment */
			continue;

		if(*p=='!')	{	/* !match */
			found=TRUE;
			p++;
		}

		truncsp(p);
		c=strlen(p);
		if(c) {
			c--;
			strupr(p);
			if(p[c]=='~') {
				p[c]=0;
				if(strstr(search,p))
					found=!found; 
			}

			else if(p[c]=='^' || p[c]=='*') {
				p[c]=0;
				if(!strncmp(p,search,c))
					found=!found; 
			}

			else if(p[0]=='*') {
				i=strlen(search);
				if(i<c)
					continue;
				if(!strncmp(p+1,search+(i-c),c))
					found=!found; 
			}

			else if(!strcmp(p,search))
				found=!found; 
		} 
	}
	fclose(stream);
	return(found);
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for matches			*/
/* Returns TRUE if found in list, FALSE if not.								*/
/****************************************************************************/
BOOL DLLCALL trashcan(scfg_t* cfg, char* insearchof, char* name)
{
	char fname[MAX_PATH+1];

	sprintf(fname,"%s%s.can",cfg->text_dir,name);
	return(findstr(insearchof,fname));
}

/****************************************************************************/
/* Returns the number of characters in 'str' not counting ctrl-ax codes		*/
/* or the null terminator													*/
/****************************************************************************/
int bstrlen(char *str)
{
	int i=0;

	while(*str) {
		if(*str==CTRL_A)
			str++;
		else
			i++;
		if(!(*str)) break;
		str++; }
	return(i);
}

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas.																	*/
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
	char str[256];
	int i,j,k;

	ultoa(l,str,10);
	i=strlen(str)-1;
	j=i/3+1+i;
	string[j--]=0;
	for(k=1;i>-1;k++) {
		string[j--]=str[i--];
		if(j>0 && !(k%3))
			string[j--]=','; }
	return(string);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
void DLLCALL truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
}

/****************************************************************************/
/* Truncate string at first occurance of char in specified character set	*/
/****************************************************************************/
char* DLLCALL truncstr(char* str, const char* set)
{
	char* p;

	p=strpbrk(str,set);
	if(p!=NULL)
		*p=0;

	return(p);
}

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

	i=strlen(str);
	if(i && str[i-1]!='\\' && str[i-1]!='/') {
		str[i]=BACKSLASH; 
		str[i+1]=0; 
	}
}

/****************************************************************************/
/* Puts a backslash on path strings if not just a drive letter and colon	*/
/****************************************************************************/
void backslashcolon(char *str)
{
    int i;

	i=strlen(str);
	if(i && str[i-1]!='\\' && str[i-1]!='/' && str[i-1]!=':') {
		str[i]=BACKSLASH; 
		str[i+1]=0; 
	}
}

/****************************************************************************/
/* Compares pointers to pointers to char. Used in conjuction with qsort()   */
/****************************************************************************/
int pstrcmp(char **str1, char **str2)
{
	return(strcmp(*str1,*str2));
}

/****************************************************************************/
/* Returns the number of characters that are the same between str1 and str2 */
/****************************************************************************/
int strsame(char *str1, char *str2)
{
	int i,j=0;

	for(i=0;str1[i];i++)
		if(str1[i]==str2[i]) j++;
	return(j);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(faddr_t* addr, char* outstr)
{
	static char str[64];
    char point[25];

	if(addr==NULL)
		return("0:0/0");
	sprintf(str,"%hu:%hu/%hu",addr->zone,addr->net,addr->node);
	if(addr->point) {
		sprintf(point,".%hu",addr->point);
		strcat(str,point); }
	if(outstr==NULL)
		return(str);
	strcpy(outstr,str);
	return(outstr);
}

char* DLLCALL net_addr(net_t* net)
{
	if(net->type==NET_FIDO)
		return(faddrtoa((faddr_t*)net->addr,NULL));
	return(net->addr);
}

static ulong msgid_serialno(smbmsg_t* msg)
{
	return (msg->idx.time<<5) | (msg->idx.number&0x1f);
}

/****************************************************************************/
/* Returns a FidoNet (FTS-9) message-ID										*/
/****************************************************************************/
char* DLLCALL ftn_msgid(sub_t *sub, smbmsg_t* msg)
{
	static char msgid[256];

	snprintf(msgid,sizeof(msgid)
		,"%s %08lX %lu.%s %08lX\r"
		,faddrtoa(&sub->faddr,NULL)
		,msgid_serialno(msg)
		,msg->idx.number
		,sub->code
		,msgid_serialno(msg)
		);

	return(msgid);
}

/****************************************************************************/
/* Return a general purpose (RFC-822) message-ID							*/
/****************************************************************************/
char* DLLCALL gen_msgid(scfg_t* cfg, uint subnum, smbmsg_t* msg)
{
	static char msgid[256];

	if(subnum>=cfg->total_subs)
		snprintf(msgid,sizeof(msgid)
			,"<%08lX.%lu@%s>"
			,msg->idx.time
			,msg->idx.number
			,cfg->sys_inetaddr);
	else
		snprintf(msgid,sizeof(msgid)
			,"<%08lX.%lu.%s@%s>"
			,msg->idx.time
			,msg->idx.number
			,cfg->sub[subnum]->code
			,cfg->sys_inetaddr);

	return(msgid);
}

/****************************************************************************/
/* Returns string for 2 digit hex+ numbers up to 575						*/
/****************************************************************************/
char *hexplus(uint num, char *str)
{
	sprintf(str,"%03x",num);
	str[0]=num/0x100 ? 'f'+(num/0x10)-0xf : str[1];
	str[1]=str[2];
	str[2]=0;
	return(str);
}

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/* by Steve Deppe (Ille Homine Albe)										*/
/****************************************************************************/
ulong ahtoul(char *str)
{
    ulong l,val=0;

	while((l=(*str++)|0x20)!=0x20)
		val=(l&0xf)+(l>>6&1)*9+val*16;
	return(val);
}

/****************************************************************************/
/* Converts hex-plus string to integer										*/
/****************************************************************************/
uint hptoi(char *str)
{
	char tmp[128];
	uint i;

	if(!str[1] || toupper(str[0])<='F')
		return(ahtoul(str));
	strcpy(tmp,str);
	tmp[0]='F';
	i=ahtoul(tmp)+((toupper(str[0])-'F')*0x10);
	return(i);
}


/****************************************************************************/
/* Updates 16-bit "rcrc" with character 'ch'                                */
/****************************************************************************/
void ucrc16(uchar ch, ushort *rcrc) 
{
	ushort i, cy;
    uchar nch=ch;
 
	for (i=0; i<8; i++) {
		cy=*rcrc & 0x8000;
		*rcrc<<=1;
		if (nch & 0x80) *rcrc |= 1;
		nch<<=1;
		if (cy) *rcrc ^= 0x1021; }
}

/****************************************************************************/
/* Returns CRC-16 of ASCIIZ string (not including terminating NULL)			*/
/****************************************************************************/
ushort DLLCALL crc16(char *str)
{
	int 	i=0;
	ushort	crc=0;

	ucrc16(0,&crc);
	while(str[i])
		ucrc16(str[i++],&crc);
	ucrc16(0,&crc);
	ucrc16(0,&crc);
	return(crc);
}

/****************************************************************************/
/* Returns 1 if a is a valid ctrl-a code, 0 if it isn't.                    */
/****************************************************************************/
BOOL DLLCALL validattr(char a)
{

	switch(toupper(a)) {
		case '-':   /* clear        */
		case '_':   /* clear        */
		case 'B':   /* blue     fg  */
		case 'C':   /* cyan     fg  */
		case 'G':   /* green    fg  */
		case 'H':   /* high     fg  */
		case 'I':   /* blink        */
		case 'K':   /* black    fg  */
		case 'L':   /* cls          */
		case 'M':   /* magenta  fg  */
		case 'N':   /* normal       */
		case 'P':   /* pause        */
		case 'R':   /* red      fg  */
		case 'W':   /* white    fg  */
		case 'Y':   /* yellow   fg  */
		case '0':   /* black    bg  */
		case '1':   /* red      bg  */
		case '2':   /* green    bg  */
		case '3':   /* brown    bg  */
		case '4':   /* blue     bg  */
		case '5':   /* magenta  bg  */
		case '6':   /* cyan     bg  */
		case '7':   /* white    bg  */
			return(TRUE); 
	}
	return(FALSE);
}

/****************************************************************************/
/* Strips invalid Ctrl-Ax sequences from str                                */
/* Returns number of ^A's in line                                           */
/****************************************************************************/
size_t DLLCALL strip_invalid_attr(char *strin)
{
    char str[1024];
    size_t a,c,d;

	for(a=c=d=0;strin[c] && d<sizeof(str)-1;c++) {
		if(strin[c]==CTRL_A && strin[c+1]!=0) {
			a++;
			if(!validattr(strin[c+1])) {
				c++;
				continue; 
			} 
		}
		str[d++]=strin[c]; 
	}
	str[d]=0;
	strcpy(strin,str);
	return(a);
}

ushort DLLCALL subject_crc(char *subj)
{
	char str[512];

	while(!strnicmp(subj,"RE:",3)) {
		subj+=3;
		while(*subj==SP)
			subj++; 
	}

	SAFECOPY(str,subj);
	strlwr(str);
	return(crc16(str));
}
