/* misc.cpp */

/* Synchronet miscellaneous utility-type routines (exported) */

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
#include "crc32.h"

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	int file,share,count=0;

    if(access&O_DENYNONE) {
        share=SH_DENYNO;
        access&=~O_DENYNONE; }
    else if(access==O_RDONLY) share=SH_DENYWR;
    else share=SH_DENYRW;
	if(!(access&O_TEXT))
		access|=O_BINARY;
    while(((file=sopen(str,access,share))==-1)
        && (errno==EACCES || errno==EAGAIN) && count++<LOOP_NOPEN)
        if(count)
            mswait(100);
    return(file);
}
/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE * fnopen(int *fd, char *str, int access)
{
	char	mode[128];
	int		file;
	FILE *	stream;

    if((file=nopen(str,access))==-1)
        return(NULL);

    if(fd!=NULL)
        *fd=file;

    if(access&O_APPEND) {
        if(access&O_RDONLY)
            strcpy(mode,"a+");
        else
            strcpy(mode,"a"); 
	} else if(access&O_CREAT) {
		if(access&O_TRUNC)
			strcpy(mode,"w");
		else
			strcpy(mode,"w+");
	} else {
        if(access&O_WRONLY || (access&O_RDWR)==O_RDWR)
            strcpy(mode,"r+");
        else
            strcpy(mode,"r"); 
	}
    stream=fdopen(file,mode);
    if(stream==NULL) {
        close(file);
        return(NULL); 
	}
    setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);
    return(stream);
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

char* DLLCALL strip_ctrl(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i] && j<sizeof(tmp)-1;i++)
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

	for(i=j=0;str[i] && j<sizeof(tmp)-1;i++)
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
/* Returns in 'string' a character representation of the number in l with   */
/* commas.																	*/
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
	char str[256];
	char i,j,k;

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
void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
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
/* Returns CRC-32 of sequence of bytes (binary or ASCIIZ)					*/
/* Pass len of 0 to auto-determine ASCIIZ string length						*/
/* or non-zero for arbitrary binary data									*/
/****************************************************************************/
ulong crc32(char *buf, ulong len)
{
	ulong l,crc=0xffffffff;

	if(len==0) 
		len=strlen(buf);
	for(l=0;l<len;l++)
		crc=ucrc32(buf[l],crc);
	return(~crc);
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

#define MV_BUFLEN	4096

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


