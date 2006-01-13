/* str_util.c */

/* Synchronet string utility routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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

	for(i=j=0;str[i] && j<(int)sizeof(tmp)-1;i++) {
		if(str[i]==CTRL_A && str[i+1]!=0)
			i++;
		else if((uchar)str[i]>=' ')
			tmp[j++]=str[i];
	}
	if(i!=j) {
		tmp[j]=0;
		strcpy(str,tmp);
	}
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
		else if(j && str[i]<=' ' && tmp[j-1]==' ')
			continue;
		else if(i && !isalnum(str[i]) && str[i]==str[i-1])
			continue;
		else if((uchar)str[i]>=' ')
			tmp[j++]=str[i];
		else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
			tmp[j++]=' ';
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
char* DLLCALL ultoac(ulong l, char *string)
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
/* Truncate string at first occurance of char in specified character set	*/
/****************************************************************************/
char* DLLCALL truncstr(char* str, const char* set)
{
	char* p;

	p=strpbrk(str,set);
	if(p!=NULL && *p!=0)
		*p=0;

	return(p);
}

/****************************************************************************/
/* rot13 encoder/decoder - courtesy of Mike Acar							*/
/****************************************************************************/
char* DLLCALL rot13(char* str)
{
	char ch, cap;
	char* p;
  
	p=str;
	while((ch=*p)!=0) {
		cap = ch & 32;
		ch &= ~cap;
		ch = ((ch >= 'A') && (ch <= 'Z') ? ((ch - 'A' + 13) % 26 + 'A') : ch) | cap;
		*(p++)=ch;
    }

	return(str);
}

/****************************************************************************/
/* Puts a backslash on path strings if not just a drive letter and colon	*/
/****************************************************************************/
void backslashcolon(char *str)
{
    int i;

	i=strlen(str);
	if(i && !IS_PATH_DELIM(str[i-1]) && str[i-1]!=':') {
		str[i]=PATH_DELIM; 
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
/* Returns 1 if a is a valid ctrl-a code, 0 if it isn't.                    */
/****************************************************************************/
BOOL DLLCALL validattr(char a)
{

	switch(toupper(a)) {
		case '+':	/* push attr	*/
		case '-':   /* pop attr		*/
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

char* replace_named_values(const char* src
					   ,char* buf
					   ,size_t buflen	/* includes '\0' terminator */
					   ,char* escape_seq
					   ,named_string_t* string_list
					   ,named_int_t* int_list
					   ,BOOL case_sensitive)
{
	char	val[32];
	size_t	i;
	size_t	esc_len=0;
	size_t	name_len;
	size_t	value_len;
	char*	p = buf;
	int (*cmp)(const char*, const char*, size_t);

	if(case_sensitive)
		cmp=strncmp;
	else
		cmp=strnicmp;

	if(escape_seq!=NULL)
		esc_len = strlen(escape_seq);

	while(*src && (size_t)(p-buf) < buflen-1) {
		if(esc_len) {
			if(cmp(src, escape_seq, esc_len)!=0) {
				*p++ = *src++;
				continue;
			}
			src += esc_len;	/* skip the escape seq */
		}
		if(string_list) {
			for(i=0; string_list[i].name!=NULL /* terminator */; i++) {
				name_len = strlen(string_list[i].name);
				if(cmp(src, string_list[i].name, name_len)==0) {
					value_len = strlen(string_list[i].value);
					if((p-buf)+value_len > buflen-1)	/* buffer overflow? */
						value_len = (buflen-1)-(p-buf);	/* truncate value */
					memcpy(p, string_list[i].value, value_len);
					p += value_len;
					src += name_len;
					break;
				}
			}
			if(string_list[i].name!=NULL) /* variable match */
				continue;
		}
		if(int_list) {
			for(i=0; int_list[i].name!=NULL /* terminator */; i++) {
				name_len = strlen(int_list[i].name);
				if(cmp(src, int_list[i].name, name_len)==0) {
					SAFEPRINTF(val,"%d",int_list[i].value);
					value_len = strlen(val);
					if((p-buf)+value_len > buflen-1)	/* buffer overflow? */
						value_len = (buflen-1)-(p-buf);	/* truncate value */
					memcpy(p, val, value_len);
					p += value_len;
					src += name_len;
					break;
				}
			}
			if(int_list[i].name!=NULL) /* variable match */
				continue;
		}

		*p++ = *src++;
	}
	*p=0;	/* terminate string in destination buffer */

	return(buf);
}

char* replace_keyed_values(const char* src
					   ,char* buf
					   ,size_t buflen	/* includes '\0' terminator */
					   ,char esc_char
					   ,keyed_string_t* string_list
					   ,keyed_int_t* int_list
					   ,BOOL case_sensitive)
{
	char	val[32];
	size_t	i;
	size_t	value_len;
	char*	p = buf;


	while(*src && (size_t)(p-buf) < buflen-1) {
		if(esc_char) {
			if(*src != esc_char) {
				*p++ = *src++;
				continue;
			}
			src ++;	/* skip the escape char */
		}
		if(string_list) {
			for(i=0; string_list[i].key!=0 /* terminator */; i++) {
				if((case_sensitive && *src == string_list[i].key)
					|| ((!case_sensitive) && toupper(*src) == toupper(string_list[i].key))) {
					value_len = strlen(string_list[i].value);
					if((p-buf)+value_len > buflen-1)	/* buffer overflow? */
						value_len = (buflen-1)-(p-buf);	/* truncate value */
					memcpy(p, string_list[i].value, value_len);
					p += value_len;
					src++;
					break;
				}
			}
			if(string_list[i].key!=0) /* variable match */
				continue;
		}
		if(int_list) {
			for(i=0; int_list[i].key!=0 /* terminator */; i++) {
				if((case_sensitive && *src == int_list[i].key)
					|| ((!case_sensitive) && toupper(*src) == toupper(int_list[i].key))) {
					SAFEPRINTF(val,"%d",int_list[i].value);
					value_len = strlen(val);
					if((p-buf)+value_len > buflen-1)	/* buffer overflow? */
						value_len = (buflen-1)-(p-buf);	/* truncate value */
					memcpy(p, val, value_len);
					p += value_len;
					src++;
					break;
				}
			}
			if(int_list[i].key!=0) /* variable match */
				continue;
		}

		*p++ = *src++;
	}
	*p=0;	/* terminate string in destination buffer */

	return(buf);
}


#if 0	/* replace_*_values test */

void main(void)
{
	char buf[128];
	keyed_string_t strs[] = {
		{ '+', "plus" },
		{ '=', "equals" },
		{ 0 }
	};
	keyed_int_t ints[] = {
		{ 'o', 1 },
		{ 't', 2 },
		{ 'h', 3 },
		{ NULL }
	};

	printf("'%s'\n", replace_keyed_values("$o $+ $t $= $h", buf, sizeof(buf), '$'
		,strs, ints, FALSE));

}

#endif

#if 0 /* to be moved here from xtrn.cpp */

char* quoted_string(const char* str, char* buf, size_t maxlen)
{
	if(strchr(str,' ')==NULL)
		return((char*)str);
	safe_snprintf(buf,maxlen,"\"%s\"",str);
	return(buf);
}

#endif

#if 0 /* I think is is a misguided idea :-(  */

char* sbbs_cmdstr(const char* src
					,char* buf
					,size_t buflen	/* includes '\0' terminator */
					,scfg_t* scfg
					,user_t* user
					,int node_num
					,int minutes
					,int rows
					,int timeleft
					,SOCKET socket_descriptor
					,char* protocol
					,char* ip_address
					,char* fpath
					,char* fspec
					)
{
	const char* nulstr = "";
	char alias_buf[LEN_ALIAS+1];
	char fpath_buf[MAX_PATH+1];
	char fspec_buf[MAX_PATH+1];
	char sysop_buf[sizeof(scfg->sys_op)];

	keyed_string_t str_list[] = {
		/* user alias */
		{ 'a',	user!=NULL ? quoted_string(user->alias, alias_buf, sizeof(alias_buf)) : nulstr },
		{ 'A',	user!=NULL ? user->alias : nulstr },
		
		/* connection */
		{ 'c',	protocol },
		{ 'C',	protocol },

		/* file path */
		{ 'f',	quoted_string(fpath, fpath_buf, sizeof(fpath_buf)) },
		{ 'F',	fpath },

		/* temp dir */
		{ 'g',	scfg->temp_dir },
		{ 'G',	scfg->temp_dir },

		/* IP address */
		{ 'h',	ip_address },
		{ 'H',	ip_address },

		/* data dir */
		{ 'j',	scfg->data_dir },
		{ 'J',	scfg->data_dir },

		/* ctrl dir */
		{ 'k',	scfg->ctrl_dir },
		{ 'K',	scfg->ctrl_dir },

		/* node dir */
		{ 'n',	scfg->node_dir },
		{ 'N',	scfg->node_dir },

		/* sysop */
		{ 'o',	quoted_string(scfg->sys_op, sysop_buf, sizeof(sysop_buf)) },
		{ 'O',	scfg->sys_op },

		/* protocol */
		{ 'p',	protocol },
		{ 'P',	protocol },

		/* system QWK-ID */
		{ 'q',	scfg->sys_id },
		{ 'Q',	scfg->sys_id },

		/* file spec */
		{ 's',	quoted_string(fspec, fspec_buf, sizeof(fspec_buf)) },
		{ 'S',	fspec },

		/* UART I/O Address (in hex) 'f' for FOSSIL */
		{ 'u',	"f" },
		{ 'U',	"f" },

		/* text dir */
		{ 'z',	scfg->text_dir },
		{ 'Z',	scfg->text_dir },

		/* exec dir */
		{ '!',	scfg->exec_dir },
		{ '@',
#ifndef __unix__
			scfg->exec_dir
#else
			nulstr
#endif
		},

		/* .exe (on Windows) */
		{ '.',
#ifndef __unix__
		".exe"
#else
		nulstr
#endif
		},

		/* terminator */
		{ 0 }	
	};
	keyed_int_t int_list[] = {
		/* node number */
		{ '#',	node_num },

		/* DTE rate */
		{ 'b',	38400 },	
		{ 'B',	38400 },

		/* DCE rate */
		{ 'd',	30000 },
		{ 'D',	30000 },

		/* Estimated Rate (cps) */
		{ 'e',	3000 },
		{ 'E',	3000 },

		{ 'h',	socket_descriptor },
		{ 'H',	socket_descriptor },

		{ 'l',	user==NULL ? 0 : scfg->level_linespermsg[user->level] },
		{ 'L',	user==NULL ? 0 : scfg->level_linespermsg[user->level] },

		{ 'm',	minutes },
		{ 'M',	minutes },

		{ 'r',	rows },
		{ 'R',	rows },

		/* Time left in seconds */
		{ 't',	timeleft },
		{ 'T',	timeleft },

		/* Credits */
		{ '$',	user==NULL ? 0 : (user->cdt+user->freecdt) },

		/* terminator */
		{ 0 }	
	};


	return replace_keyed_values(src, buf, buflen, '%', str_list, int_list, TRUE);
}

#endif