/* scfgmdm.c */

/* $Id: scfgmdm.c,v 1.6 2018/07/24 01:12:24 rswindell Exp $ */

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

#include "scfg.h"

int exec_mdm(char *fname);

void mdm_cfg(int mdmnum)
{

free(cfg.mdm_result);
cfg.mdm_result=NULL;

/*
strcpy(cfg.mdm_answ,"ATA");
strcpy(cfg.mdm_hang,"ATH");
strcpy(cfg.mdm_dial,"ATDT");
strcpy(cfg.mdm_offh,"ATM0H1");
strcpy(cfg.mdm_term,"ATE1V1");
strcpy(cfg.mdm_init,"AT&FS0=0S2=128E0V0X4&C1&D2");
*/
cfg.mdm_answ[0]=0;
cfg.mdm_hang[0]=0;
cfg.mdm_dial[0]=0;
cfg.mdm_offh[0]=0;
cfg.mdm_term[0]=0;
cfg.mdm_init[0]=0;
cfg.mdm_spec[0]=0;
cfg.mdm_results=0;
cfg.mdm_misc=(MDM_RTS|MDM_CTS);

exec_mdm(mdm_file[mdmnum]);

}

void cvttab(char *str)
{
	int i;

for(i=0;str[i];i++)
	if(str[i]==TAB)
		str[i]=' ';
}

int export_mdm(char *fname)
{
	char str[256];
	int file,i;
	time_t now;
	FILE *stream;

sprintf(str,"%s%s.MDM",cfg.ctrl_dir,fname);
if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1)
	return(0);
if((stream=fdopen(file,"wb"))==NULL) {
    close(file);
	return(0); }

now=time(NULL);
fprintf(stream,"# Exported from %s Node %u on %.24s\r\n\r\n"
	,cfg.sys_name,cfg.node_num,ctime(&now));
fprintf(stream,"COM_RATE\t%ld\r\n",cfg.com_rate);
fprintf(stream,"INIT_STR\t%s\r\n",cfg.mdm_init);
fprintf(stream,"DIAL_STR\t%s\r\n",cfg.mdm_dial);
fprintf(stream,"HANGUP_STR\t%s\r\n",cfg.mdm_hang);
fprintf(stream,"ANSWER_STR\t%s\r\n",cfg.mdm_answ);
fprintf(stream,"OFFHOOK_STR\t%s\r\n",cfg.mdm_offh);
fprintf(stream,"SPEC_INIT\t%s\r\n",cfg.mdm_spec);
fprintf(stream,"TERM_INIT\t%s\r\n",cfg.mdm_term);
fprintf(stream,"LOCKED_RATE\t%s\r\n",(cfg.mdm_misc&MDM_STAYHIGH) ? "YES":"NO");
fprintf(stream,"CALLER_ID\t%s\r\n",(cfg.mdm_misc&MDM_CALLERID) ? "YES":"NO");
fprintf(stream,"DROP_DTR\t%s\r\n",(cfg.mdm_misc&MDM_NODTR) ? "NO":"YES");
fprintf(stream,"FLOW_CONTROL\t%s\r\n"
	,(cfg.mdm_misc&(MDM_RTS|MDM_CTS)==(MDM_RTS|MDM_CTS)) ? "BOTH":
	(cfg.mdm_misc&MDM_CTS) ? "TRANSMIT" : (cfg.mdm_misc&MDM_RTS) ? "RECEIVE" : "NONE");
for(i=0;i<cfg.mdm_results;i++)
	fprintf(stream,"RESULT\t\t%u\t%u\t%u\t%s\r\n",cfg.mdm_result[i].code
		,cfg.mdm_result[i].cps,cfg.mdm_result[i].rate,cfg.mdm_result[i].str);

fclose(stream);

return(1);
}

int exec_mdm(char *fname)
{
	char str[256],msg[128],*p;
	int file,i,j;
	FILE *stream;

sprintf(str,"%s%s.MDM",cfg.ctrl_dir,fname);

if((file=open(str,O_RDONLY|O_BINARY|O_DENYALL))==-1)
	return(0);
if((stream=fdopen(file,"rb"))==NULL) {
    close(file);
    return(0); }

while(!feof(stream)) {
	if(!fgets(str,255,stream))
		break;
	cvttab(str);
    truncsp(str);
	p=str;
	while(*p && *p<=' ')   /* look for beginning of command */
		p++;
	if(!*p)
		continue;
	if(*p=='#')             /* remarks start with # */
        continue;

	if(!strnicmp(p,"COM_RATE",8)) {
		p+=8;
		while(*p==' ') p++;
		cfg.com_rate=atol(p);
		continue; }

	if(!strnicmp(p,"INIT_STR",8)) {
		p+=8;
		while(*p==' ') p++;
		sprintf(cfg.mdm_init,"%.63s",p);
        continue; }

	if(!strnicmp(p,"DIAL_STR",8)) {
		p+=8;
		while(*p==' ') p++;
		sprintf(cfg.mdm_dial,"%.63s",p);
        continue; }

	if(!strnicmp(p,"HANGUP_STR",10)) {
		p+=10;
		while(*p==' ') p++;
		sprintf(cfg.mdm_hang,"%.63s",p);
        continue; }

	if(!strnicmp(p,"ANSWER_STR",10)) {
		p+=10;
		while(*p==' ') p++;
		sprintf(cfg.mdm_answ,"%.63s",p);
        continue; }

	if(!strnicmp(p,"OFFHOOK_STR",11)) {
		p+=11;
		while(*p==' ') p++;
		sprintf(cfg.mdm_offh,"%.63s",p);
        continue; }

	if(!strnicmp(p,"SPEC_INIT",9)) {
		p+=9;
		while(*p==' ') p++;
		sprintf(cfg.mdm_spec,"%.63s",p);
        continue; }

	if(!strnicmp(p,"TERM_INIT",9)) {
		p+=9;
		while(*p==' ') p++;
		sprintf(cfg.mdm_term,"%.63s",p);
        continue; }

	if(!strnicmp(p,"LOCKED_RATE",11)) {
		p+=11;
		while(*p==' ') p++;
		if(!stricmp(p,"OFF") || !stricmp(p,"NO"))
			cfg.mdm_misc&=~MDM_STAYHIGH;
		else
			cfg.mdm_misc|=MDM_STAYHIGH;
        continue; }

	if(!strnicmp(p,"CALLER_ID",9)) {
		p+=9;
		while(*p==' ') p++;
		if(!stricmp(p,"YES") || !stricmp(p,"ON"))
			cfg.mdm_misc|=MDM_CALLERID;
		else
			cfg.mdm_misc&=~MDM_CALLERID;
        continue; }

	if(!strnicmp(p,"VERBAL_RESULTS",14)) {
		p+=14;
		while(*p==' ') p++;
		if(!stricmp(p,"YES") || !stricmp(p,"ON"))
			cfg.mdm_misc|=MDM_VERBAL;
		else
			cfg.mdm_misc&=~MDM_VERBAL;
        continue; }

	if(!strnicmp(p,"DROP_DTR",8)) {
		p+=8;
		while(*p==' ') p++;
		if(!stricmp(p,"OFF") || !stricmp(p,"NO"))
			cfg.mdm_misc|=MDM_NODTR;
		else
			cfg.mdm_misc&=~MDM_NODTR;
        continue; }

	if(!strnicmp(p,"FLOW_CONTROL",12)) {
		p+=12;
		while(*p==' ') p++;
		cfg.mdm_misc&=~(MDM_RTS|MDM_CTS);
		strupr(p);
		if(strstr(p,"RTS") || strstr(p,"RECEIVE") || strstr(p,"RECV")
			|| strstr(p,"BOTH"))
			cfg.mdm_misc|=MDM_RTS;
		if(strstr(p,"CTS") || strstr(p,"TRANSMIT") || strstr(p,"SEND")
			|| strstr(p,"BOTH"))
			cfg.mdm_misc|=MDM_CTS;
        continue; }

	if(!strnicmp(p,"RESULT ",7)) {
		p+=7;
		while(*p==' ') p++;
		i=atoi(p);
		for(j=0;j<cfg.mdm_results;j++)
			if(cfg.mdm_result[j].code==i)
				break;
		if(j>=cfg.mdm_results) {
			if((cfg.mdm_result=(mdm_result_t *)realloc(
				cfg.mdm_result,sizeof(mdm_result_t)
				*(cfg.mdm_results+1)))==NULL) {
				errormsg(WHERE,ERR_ALLOC,p,cfg.mdm_results+1);
				cfg.mdm_results=0;
				bail(1);
				continue; }
			cfg.mdm_results++; }
		cfg.mdm_result[j].code=i;
		while(*p!=' ') p++;
		while(*p==' ') p++;
		cfg.mdm_result[j].cps=atoi(p);
		while(*p!=' ') p++;
        while(*p==' ') p++;
		cfg.mdm_result[j].rate=atoi(p);
		while(*p!=' ') p++;
        while(*p==' ') p++;
		sprintf(cfg.mdm_result[j].str,"%.*s",LEN_MODEM,p);
		continue; }

	if(!strnicmp(p,"INCLUDE ",8)) {
		p+=8;
		while(*p==' ') p++;
		exec_mdm(p);
		continue; }

	sprintf(msg,"ERROR: '%.15s' in %.8s.MDM",p,fname);
	umsg(msg);
}

fclose(stream);
return(1);
}


void init_mdms()
{
	char str[128],fname[128];
	int file;
	FILE *stream;

mdm_types=0;
mdm_type=NULL;
mdm_file=NULL;
sprintf(str,"%sMDMS.IXT",cfg.ctrl_dir);
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
if((stream=fdopen(file,"rb"))==NULL) {
	close(file);
	errormsg(WHERE,ERR_FDOPEN,str,O_RDONLY);
	return; }

while(!feof(stream)) {
	if(!fgets(str,120,stream))
        break;
	truncsp(str);
	if(!fgets(fname,120,stream))
		break;
	truncsp(fname);
	if((mdm_type=realloc(mdm_type,sizeof(char *)*(mdm_types+1)))==NULL) {
		errormsg(WHERE,ERR_ALLOC,"Modem Type",sizeof(char *)*(mdm_types+1));
		break; }
	if((mdm_file=realloc(mdm_file,sizeof(char *)*(mdm_types+1)))==NULL) {
		errormsg(WHERE,ERR_ALLOC,"Modem File",sizeof(char *)*(mdm_types+1));
        break; }
	if((mdm_type[mdm_types]=malloc(strlen(str)+1))==NULL) {
		errormsg(WHERE,ERR_ALLOC,"Modem Typename",sizeof(char *)*(mdm_types+1));
        break; }
	if((mdm_file[mdm_types]=malloc(9))==NULL) {
		errormsg(WHERE,ERR_ALLOC,"Modem Filename",sizeof(char *)*(mdm_types+1));
        break; }
	strcpy(mdm_type[mdm_types],str);
	sprintf(mdm_file[mdm_types],"%.8s",fname);
	mdm_types++; }
fclose(stream);
}
