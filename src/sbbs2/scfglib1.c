#line 1 "SCFGLIB1.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "scfglib.h"

void prep_path(char *path)
{
	char str[LEN_DIR*2];
	int i;

if(!path[0])
	return;
if(path[0]!='\\' && path[1]!=':')           /* Relative to NODE directory */
	sprintf(str,"%s%s",node_dir,path);
else
	strcpy(str,path);
i=strlen(str);
if(str[i-1]!=':' && str[i-1]!='\\')
	strcat(str,"\\");
strcat(str,".");                // Change C: to C:. and C:\SBBS\ to C:\SBBS\.
_fullpath(path,str,LEN_DIR+1);	// Change C:\SBBS\NODE1\..\EXEC to C:\SBBS\EXEC
i=strlen(path);
if(i && path[i-1]!='\\')
	strcat(path,"\\");
}

char *get_alloc(long *offset, char *outstr, int maxlen, FILE *instream)
{

#ifdef SCFG
	fread(outstr,1,maxlen+1,instream);
	(*offset)+=maxlen+1;
#else
	char str[257];

	fread(str,1,maxlen+1,instream);
//	  lprintf("%s %d %p\r\n",__FILE__,__LINE__,offset);
	(*offset)+=maxlen+1;	// this line was commented out (04/12/97) why?
	if(!str[0]) 			/* Save memory */
		return(scfgnulstr);
	if((outstr=(char *)MALLOC(strlen(str)+1))==NULL)
		return(NULL);
	strcpy(outstr,str);
#endif

return(outstr);
}

#ifdef SCFG 	/* SCFG allocate max length */
	#define readline_alloc(l,s,m,i) readline(l,s,m,i)
#else
	char *readline_alloc(long *offset, char *outstr, int maxline
		, FILE *instream);
	#define readline_alloc(l,s,m,i) s=readline_alloc(l,s,m,i)
	#define get_alloc(o,s,l,i) s=get_alloc(o,s,l,i)
#endif

/***********************************************************/
/* These functions are called from here and must be linked */
/***********************************************************/
/***
	nopen()
	truncsp()
***/

void allocerr(read_cfg_text_t txt, long offset, char *fname, uint size)
{
lprintf(txt.error,offset,fname);
lprintf(txt.allocerr,size);
bail(1);
}

#ifndef NO_NODE_CFG

/****************************************************************************/
/* Reads in NODE.CNF and initializes the associated variables				*/
/****************************************************************************/
void read_node_cfg(read_cfg_text_t txt)
{
	char	c,str[256],cmd[64],fname[13],*p;
	int 	i;
	short	n;
	long	offset=0;
	FILE	*instream;

strcpy(fname,"NODE.CNF");
sprintf(str,"%s%s",node_dir,fname);
if((instream=fnopen(&i,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
#ifdef SBBS
	lprintf("\r\nSBBS must be run from a NODE directory (e.g. ");
	lclatr(WHITE);
	lprintf("C:\\SBBS\\NODE1");
	lclatr(LIGHTGRAY);
	lprintf(").\r\n");
#endif
	bail(1); }

lprintf(txt.reading,fname);
get_int(node_num,instream);
if(!node_num) {
	lprintf(txt.error,offset,fname);
	lprintf("Node number must be non-zero\r\n");
	bail(1); }
get_str(node_name,instream);
get_str(node_phone,instream);
get_str(node_comspec,instream);
if(!node_comspec[0])
	strcpy(node_comspec,"C:\\OS2\\MDOS\\COMMAND.COM");
get_int(node_misc,instream);
get_int(node_ivt,instream);
get_int(node_swap,instream);
get_str(node_swapdir,instream);
#ifndef SCFG
if(!node_swapdir[0])
	strcpy(node_swapdir,node_dir);
else
	prep_path(node_swapdir);
#endif
get_int(node_valuser,instream);
get_int(node_minbps,instream);
#ifdef SCFG
get_str(node_ar,instream);
#else
fread(str,1,LEN_ARSTR+1,instream);
offset+=LEN_ARSTR+1;
node_ar=arstr(0,str);
#endif
get_int(node_dollars_per_call,instream);
get_str(node_editor,instream);
get_str(node_viewer,instream);
get_str(node_daily,instream);
get_int(c,instream);
if(c) node_scrnlen=c;
get_int(node_scrnblank,instream);
get_str(ctrl_dir,instream); 				/* ctrl directory */
get_str(text_dir,instream); 				/* text directory */
get_str(temp_dir,instream); 				/* temp directory */
if(!temp_dir[0])
	strcpy(temp_dir,"TEMP");

#ifndef SCFG
prep_path(ctrl_dir);
prep_path(text_dir);
prep_path(temp_dir);
#endif

for(i=0;i<10;i++) { 						/* WFC 0-9 DOS commands */
	get_alloc(&offset,wfc_cmd[i],LEN_CMD,instream); }
for(i=0;i<12;i++) { 						/* WFC F1-F12 shrinking DOS cmds */
	get_alloc(&offset,wfc_scmd[i],LEN_CMD,instream); }
get_str(mdm_hang,instream);
get_int(node_sem_check,instream);
if(!node_sem_check) node_sem_check=60;
get_int(node_stat_check,instream);
if(!node_stat_check) node_stat_check=10;
get_str(scfg_cmd,instream);
if(!scfg_cmd[0])
	strcpy(scfg_cmd,"%!scfg %k");
get_int(sec_warn,instream);
if(!sec_warn)
	sec_warn=180;
get_int(sec_hangup,instream);
if(!sec_hangup)
	sec_hangup=300;
for(i=0;i<188;i++) {				/* Unused - initialized to NULL */
	fread(&n,1,2,instream);
	offset+=2; }
for(i=0;i<256;i++) {				/* Unused - initialized to 0xff */
	fread(&n,1,2,instream);
	offset+=2; }

/***************/
/* Modem Stuff */
/***************/

get_int(com_port,instream);
get_int(com_irq,instream);
get_int(com_base,instream);
get_int(com_rate,instream);
get_int(mdm_misc,instream);
get_str(mdm_init,instream);
get_str(mdm_spec,instream);
get_str(mdm_term,instream);
get_str(mdm_dial,instream);
get_str(mdm_offh,instream);
get_str(mdm_answ,instream);
get_int(mdm_reinit,instream);
get_int(mdm_ansdelay,instream);
get_int(mdm_rings,instream);

get_int(mdm_results,instream);

if(mdm_results) {
	if((mdm_result=(mdm_result_t *)MALLOC(sizeof(mdm_result_t)*mdm_results))
		==NULL)
	allocerr(txt,offset,fname,sizeof(mdm_result_t *)*mdm_results); }
else
	mdm_result=NULL;

for(i=0;i<mdm_results;i++) {
	if(feof(instream)) break;
	get_int(mdm_result[i].code,instream);
	get_int(mdm_result[i].rate,instream);
	get_int(mdm_result[i].cps,instream);
	get_alloc(&offset,mdm_result[i].str,LEN_MODEM,instream); }
mdm_results=i;
fclose(instream);
lprintf(txt.readit,fname);
}

#endif

#ifndef NO_MAIN_CFG

/****************************************************************************/
/* Reads in MAIN.CNF and initializes the associated variables				*/
/****************************************************************************/
void read_main_cfg(read_cfg_text_t txt)
{
	char	str[256],fname[13],*p,c;
	int 	file;
	short	i,j,n;
	long	offset=0;
	FILE	*instream;

strcpy(fname,"MAIN.CNF");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
	bail(1); }

lprintf(txt.reading,fname);
get_str(sys_name,instream);
get_str(sys_id,instream);
get_str(sys_location,instream);
get_str(sys_phonefmt,instream);
get_str(sys_op,instream);
get_str(sys_guru,instream);
get_str(sys_pass,instream);
get_int(sys_nodes,instream);

if(!sys_nodes || sys_nodes<node_num || sys_nodes>MAX_NODES) {
	lprintf(txt.error,offset,fname);
	if(!sys_nodes)
		lprintf("Total nodes on system must be non-zero.\r\n");
	else if(sys_nodes>MAX_NODES)
		lprintf("Total nodes exceeds %u.\r\n",MAX_NODES);
	else
		lprintf("Total nodes (%u) < node number in NODE.CNF (%u)\r\n"
			,sys_nodes,node_num);
	bail(1); }

if((node_path=(char **)MALLOC(sizeof(char *)*sys_nodes))==NULL)
	allocerr(txt,offset,fname,sizeof(char *)*sys_nodes);

for(i=0;i<sys_nodes;i++) {
	if(feof(instream)) break;
	fread(str,LEN_DIR+1,1,instream);
	offset+=LEN_DIR+1;
	if((node_path[i]=(char *)MALLOC(strlen(str)+1))==NULL)
		allocerr(txt,offset,fname,strlen(str)+1);
	strcpy(node_path[i],str); }

get_str(data_dir,instream); 			  /* data directory */
get_str(exec_dir,instream); 			  /* exec directory */

#ifndef SCFG
prep_path(data_dir);
prep_path(exec_dir);
#endif

get_str(sys_logon,instream);
get_str(sys_logout,instream);
get_str(sys_daily,instream);
get_int(sys_timezone,instream);
get_int(sys_misc,instream);
get_int(sys_lastnode,instream);
get_int(sys_autonode,instream);
get_int(uq,instream);
get_int(sys_pwdays,instream);
get_int(sys_deldays,instream);
get_int(sys_exp_warn,instream); 	/* Days left till expiration warning */
get_int(sys_autodel,instream);
get_int(sys_def_stat,instream); 	/* default status line */

#ifdef SCFG
get_str(sys_chat_ar,instream);
#else
fread(str,1,LEN_ARSTR+1,instream);
offset+=LEN_ARSTR+1;
sys_chat_ar=arstr(0,str);
#endif

get_int(cdt_min_value,instream);
get_int(max_minutes,instream);
get_int(cdt_per_dollar,instream);
get_str(new_pass,instream);
get_str(new_magic,instream);
get_str(new_sif,instream);
get_str(new_sof,instream);
if(!new_sof[0])		/* if output not specified, use input file */
	strcpy(new_sof,new_sif);

/*********************/
/* New User Settings */
/*********************/

get_int(new_level,instream);
get_int(new_flags1,instream);
get_int(new_flags2,instream);
get_int(new_flags3,instream);
get_int(new_flags4,instream);
get_int(new_exempt,instream);
get_int(new_rest,instream);
get_int(new_cdt,instream);
get_int(new_min,instream);
get_str(new_xedit,instream);
get_int(new_expire,instream);
get_int(new_shell,instream);
get_int(new_misc,instream);
get_int(new_prot,instream);
if(new_prot<SP)
	new_prot=SP;
get_int(c,instream);
for(i=0;i<7;i++)
	get_int(n,instream);

/*************************/
/* Expired User Settings */
/*************************/

get_int(expired_level,instream);
get_int(expired_flags1,instream);
get_int(expired_flags2,instream);
get_int(expired_flags3,instream);
get_int(expired_flags4,instream);
get_int(expired_exempt,instream);
get_int(expired_rest,instream);

get_str(logon_mod,instream);
get_str(logoff_mod,instream);
get_str(newuser_mod,instream);
get_str(login_mod,instream);
if(!login_mod[0]) strcpy(login_mod,"LOGIN");
get_str(logout_mod,instream);
get_str(sync_mod,instream);
get_str(expire_mod,instream);
get_int(c,instream);

for(i=0;i<224;i++)					/* unused - initialized to NULL */
	get_int(n,instream);
for(i=0;i<256;i++)					/* unused - initialized to 0xff */
	get_int(n,instream);

/*******************/
/* Validation Sets */
/*******************/

for(i=0;i<10 && !feof(instream);i++) {
	get_int(val_level[i],instream);
	get_int(val_expire[i],instream);
	get_int(val_flags1[i],instream);
	get_int(val_flags2[i],instream);
	get_int(val_flags3[i],instream);
	get_int(val_flags4[i],instream);
	get_int(val_cdt[i],instream);
	get_int(val_exempt[i],instream);
	get_int(val_rest[i],instream);
	for(j=0;j<8;j++)
		get_int(n,instream); }

/***************************/
/* Security Level Settings */
/***************************/

for(i=0;i<100 && !feof(instream);i++) {
	get_int(level_timeperday[i],instream);
	if(level_timeperday[i]>500)
		level_timeperday[i]=500;
	get_int(level_timepercall[i],instream);
	if(level_timepercall[i]>500)
		level_timepercall[i]=500;
	get_int(level_callsperday[i],instream);
	get_int(level_freecdtperday[i],instream);
	get_int(level_linespermsg[i],instream);
	get_int(level_postsperday[i],instream);
	get_int(level_emailperday[i],instream);
	get_int(level_misc[i],instream);
	get_int(level_expireto[i],instream);
	get_int(c,instream);
	for(j=0;j<5;j++)
		get_int(n,instream); }
if(i!=100) {
	lprintf(txt.error,offset,fname);
	lprintf("Insufficient User Level Information\r\n"
		"%d user levels read, 100 needed.\r\n",i);
	bail(1); }

get_int(total_shells,instream);
#ifdef SBBS
if(!total_shells) {
	lprintf(txt.error,offset,fname);
	lprintf("At least one command shell must be configured.\r\n");
	bail(1); }
#endif

if(total_shells) {
	if((shell=(shell_t **)MALLOC(sizeof(shell_t *)*total_shells))==NULL)
		allocerr(txt,offset,fname,sizeof(shell_t *)*total_shells); }
else
	shell=NULL;

for(i=0;i<total_shells;i++) {
	if(feof(instream)) break;
	if((shell[i]=(shell_t *)MALLOC(sizeof(shell_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(shell_t));
	memset(shell[i],0,sizeof(shell_t));

	get_alloc(&offset,shell[i]->name,40,instream);
	get_str(shell[i]->code,instream);
#ifdef SCFG
	get_str(shell[i]->ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	shell[i]->ar=arstr(0,str);
#endif
	get_int(shell[i]->misc,instream);
	for(j=0;j<8;j++)
		get_int(n,instream);
	}
total_shells=i;


fclose(instream);
lprintf(txt.readit,fname);
}

#endif

#ifndef NO_MSGS_CFG


/****************************************************************************/
/* Reads in MSGS.CNF and initializes the associated variables				*/
/****************************************************************************/
void read_msgs_cfg(read_cfg_text_t txt)
{
	char	str[256],fname[13],tmp[128],c;
	int 	file;
	short	i,j,k,l,n;
	long	offset=0;
	FILE	*instream;

#ifndef SCFG

sprintf(data_dir_subs,"%sSUBS\\",data_dir);
prep_path(data_dir_subs);

#endif

strcpy(fname,"MSGS.CNF");
sprintf(str,"%s%s",ctrl_dir,fname);
if((instream=fnopen(&file,str,O_RDONLY))==NULL) {
	lprintf(txt.openerr,str);
	bail(1); }

lprintf(txt.reading,fname);

/*************************/
/* General Message Stuff */
/*************************/

get_int(max_qwkmsgs,instream);
get_int(mail_maxcrcs,instream);
get_int(mail_maxage,instream);
#ifdef SCFG
	get_str(preqwk_ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	preqwk_ar=arstr(0,str);
#endif
get_int(smb_retry_time,instream);	 /* odd byte */
if(!smb_retry_time)
	smb_retry_time=30;
for(i=0;i<235;i++)	/* NULL */
	get_int(n,instream);
for(i=0;i<256;i++)	/* 0xff */
    get_int(n,instream);


/******************/
/* Message Groups */
/******************/

get_int(total_grps,instream);


if(total_grps) {
	if((grp=(grp_t **)MALLOC(sizeof(grp_t *)*total_grps))==NULL)
		allocerr(txt,offset,fname,sizeof(grp_t *)*total_grps); }
else
	grp=NULL;


#ifdef SBBS

if(total_grps) {

	if((cursub=(uint *)MALLOC(sizeof(uint)*total_grps))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*total_grps);

	if((usrgrp=(uint *)MALLOC(sizeof(uint)*total_grps))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*total_grps);

	if((usrsubs=(uint *)MALLOC(sizeof(uint)*total_grps))==NULL)
		allocerr(txt,offset,fname,sizeof(uint)*total_grps);

	if((usrsub=(uint **)MALLOC(sizeof(uint *)*total_grps))==NULL)
		allocerr(txt,offset,fname,sizeof(uint *)*total_grps); }

#endif

for(i=0;i<total_grps;i++) {

	if(feof(instream)) break;
	if((grp[i]=(grp_t *)MALLOC(sizeof(grp_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(grp_t));
	memset(grp[i],0,sizeof(grp_t));

	get_alloc(&offset,grp[i]->lname,LEN_GLNAME,instream);
	get_alloc(&offset,grp[i]->sname,LEN_GSNAME,instream);

#if !defined(SCFG) && defined(SAVE_MEMORY)	  /* Save memory */
	if(!strcmp(grp[i]->lname,grp[i]->sname) && grp[i]->sname!=scfgnulstr) {
		FREE(grp[i]->sname);
		grp[i]->sname=grp[i]->lname; }
#endif

#ifdef SCFG
	get_str(grp[i]->ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	grp[i]->ar=arstr(0,str);
#endif
	for(j=0;j<48;j++)
		get_int(n,instream);
	}
total_grps=i;

/**********************/
/* Message Sub-boards */
/**********************/

get_int(total_subs,instream);

if(total_subs) {
	if((sub=(sub_t **)MALLOC(sizeof(sub_t *)*total_subs))==NULL)
		allocerr(txt,offset,fname,sizeof(sub_t *)*total_subs); }
else
	sub=NULL;

#ifdef SBBS

if(total_subs) {

	if((sub_misc=(char *)MALLOC(sizeof(char)*total_subs))==NULL)
		allocerr(txt,offset,fname,sizeof(char)*total_subs);

	if((sub_ptr=(ulong *)MALLOC(sizeof(ulong)*total_subs))==NULL)
		allocerr(txt,offset,fname,sizeof(ulong)*total_subs);

	if((sub_last=(ulong *)MALLOC(sizeof(ulong)*total_subs))==NULL)
		allocerr(txt,offset,fname,sizeof(ulong)*total_subs); }

#endif

for(i=0;i<total_subs;i++) {
	if(feof(instream)) break;
	if((sub[i]=(sub_t *)MALLOC(sizeof(sub_t)))==NULL)
		allocerr(txt,offset,fname,sizeof(sub_t));
	memset(sub[i],0,sizeof(sub_t));

	get_int(sub[i]->grp,instream);
	get_alloc(&offset,sub[i]->lname,LEN_SLNAME,instream);
	get_alloc(&offset,sub[i]->sname,LEN_SSNAME,instream);

#if !defined(SCFG) && defined(SAVE_MEMORY) /* Save memory */
	if(!strcmp(sub[i]->lname,sub[i]->sname) && sub[i]->sname!=scfgnulstr) {
		FREE(sub[i]->sname);
		sub[i]->sname=sub[i]->lname; }
#endif

	get_alloc(&offset,sub[i]->qwkname,10,instream);

#if !defined(SCFG) && defined(SAVE_MEMORY)	/* Save memory */
	if(!strcmp(sub[i]->qwkname,sub[i]->sname) && sub[i]->qwkname!=scfgnulstr) {
		FREE(sub[i]->qwkname);
		sub[i]->qwkname=sub[i]->sname; }
#endif

	get_str(sub[i]->code,instream);

#ifdef SCFG
	get_str(sub[i]->data_dir,instream);
#else
	fread(str,LEN_DIR+1,1,instream);   /* substitute data dir */
	offset+=LEN_DIR+1;
	if(str[0]) {
		prep_path(str);
		if((sub[i]->data_dir=(char *)MALLOC(strlen(str)+1))==NULL)
			allocerr(txt,offset,fname,strlen(str)+1);
		strcpy(sub[i]->data_dir,str); }
	else
		sub[i]->data_dir=data_dir_subs;
#endif


#ifdef SCFG
	get_str(sub[i]->ar,instream);
	get_str(sub[i]->read_ar,instream);
	get_str(sub[i]->post_ar,instream);
	get_str(sub[i]->op_ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	sub[i]->ar=arstr(0,str);
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	sub[i]->read_ar=arstr(0,str);
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	sub[i]->post_ar=arstr(0,str);
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	sub[i]->op_ar=arstr(0,str);
#endif
	get_int(sub[i]->misc,instream);


#ifdef SCFG
	get_str(sub[i]->tagline,instream);
#else
	fread(str,81,1,instream);	/* substitute tagline */
	offset+=81;
	if(str[0]) {
		if((sub[i]->tagline=(char *)MALLOC(strlen(str)+1))==NULL)
			allocerr(txt,offset,fname,strlen(str)+1);
		strcpy(sub[i]->tagline,str); }
	else
		sub[i]->tagline=qnet_tagline;
#endif

#ifdef SCFG
	get_str(sub[i]->origline,instream);
#else
	fread(str,1,51,instream);	/* substitute origin line */
	offset+=51;
	if(str[0]) {
		if((sub[i]->origline=(char *)MALLOC(strlen(str)+1))==NULL)
			allocerr(txt,offset,fname,strlen(str)+1);
		strcpy(sub[i]->origline,str); }
	else
		sub[i]->origline=origline;
#endif

#ifdef SCFG
	get_str(sub[i]->echomail_sem,instream);
#else
	fread(str,1,LEN_DIR+1,instream);   /* substitute echomail semaphore */
	offset+=LEN_DIR+1;
	if(str[0]) {
		if((sub[i]->echomail_sem=(char *)MALLOC(strlen(str)+1))==NULL)
			allocerr(txt,offset,fname,strlen(str)+1);
		strcpy(sub[i]->echomail_sem,str); }
	else
		sub[i]->echomail_sem=echomail_sem;
#endif
	fread(str,1,LEN_DIR+1,instream);   /* substitute EchoMail path */
	offset+=LEN_DIR+1;
#ifndef SBBS
	backslash(str);
	strcpy(sub[i]->echopath,str);
#endif
	get_int(sub[i]->faddr,instream);			/* FidoNet address */

	get_int(sub[i]->maxmsgs,instream);
	get_int(sub[i]->maxcrcs,instream);
	get_int(sub[i]->maxage,instream);
	get_int(sub[i]->ptridx,instream);
#ifdef SBBS
	for(j=0;j<i;j++)
		if(sub[i]->ptridx==sub[j]->ptridx) {
			lprintf(txt.error,offset,fname);
			lprintf("Duplicate pointer index for subs #%d and #%d\r\n"
				,i+1,j+1);
			bail(1); }
#endif

#ifdef SCFG
	get_str(sub[i]->mod_ar,instream);
#else
	fread(str,1,LEN_ARSTR+1,instream);
	offset+=LEN_ARSTR+1;
	sub[i]->mod_ar=arstr(0,str);
#endif
	get_int(sub[i]->qwkconf,instream);
	get_int(c,instream);
	for(j=0;j<26;j++)
		get_int(n,instream);
	}
total_subs=i;

#ifdef SBBS
for(i=l=0;i<total_grps;i++) {
	for(j=k=0;j<total_subs;j++)
		if(sub[j]->grp==i)
			k++;	/* k = number of subs per grp[i] */
	if(k>l) l=k; }	/* l = the largest number of subs per grp */
if(l)
	for(i=0;i<total_grps;i++)
		if((usrsub[i]=(uint *)MALLOC(sizeof(uint)*l))==NULL)
			allocerr(txt,offset,fname,sizeof(uint)*l);

if(sys_status&SS_INITIAL) {
	fclose(instream);
	lprintf(txt.readit,fname);
	return; }
#endif

/***********/
/* FidoNet */
/***********/

get_int(total_faddrs,instream);

if(total_faddrs) {
	if((faddr=(faddr_t *)MALLOC(sizeof(faddr_t)*total_faddrs))==NULL)
		allocerr(txt,offset,fname,sizeof(faddr_t)*total_faddrs); }
else
	faddr=NULL;

for(i=0;i<total_faddrs;i++)
	get_int(faddr[i],instream);

get_str(origline,instream);
get_str(netmail_sem,instream);
get_str(echomail_sem,instream);
get_str(netmail_dir,instream);
get_str(echomail_dir,instream);
get_str(fidofile_dir,instream);
get_int(netmail_misc,instream);
get_int(netmail_cost,instream);
get_int(dflt_faddr,instream);
for(i=0;i<28;i++)
    get_int(n,instream);


/**********/
/* QWKnet */
/**********/

get_str(qnet_tagline,instream);

get_int(total_qhubs,instream);

if(total_qhubs) {
	if((qhub=(qhub_t **)MALLOC(sizeof(qhub_t *)*total_qhubs))==NULL)
		allocerr(txt,offset,fname,sizeof(qhub_t*)*total_qhubs); }
else
	qhub=NULL;

for(i=0;i<total_qhubs;i++) {
    if(feof(instream)) break;
	if((qhub[i]=(qhub_t *)MALLOC(sizeof(qhub_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(qhub_t));
    memset(qhub[i],0,sizeof(qhub_t));

	get_str(qhub[i]->id,instream);
	get_int(qhub[i]->time,instream);
	get_int(qhub[i]->freq,instream);
	get_int(qhub[i]->days,instream);
	get_int(qhub[i]->node,instream);
	get_alloc(&offset,qhub[i]->call,LEN_CMD,instream);
	get_alloc(&offset,qhub[i]->pack,LEN_CMD,instream);
	get_alloc(&offset,qhub[i]->unpack,LEN_CMD,instream);
	get_int(k,instream);

	if(k) {
		if((qhub[i]->sub=(ushort *)MALLOC(sizeof(ushort)*k))==NULL)
			allocerr(txt,offset,fname,sizeof(uint)*k);
		if((qhub[i]->conf=(ushort *)MALLOC(sizeof(ushort)*k))==NULL)
			allocerr(txt,offset,fname,sizeof(ushort)*k);
		if((qhub[i]->mode=(uchar *)MALLOC(sizeof(uchar)*k))==NULL)
			allocerr(txt,offset,fname,sizeof(uchar)*k); }

	for(j=0;j<k;j++) {
		if(feof(instream)) break;
		get_int(qhub[i]->conf[qhub[i]->subs],instream);
		get_int(qhub[i]->sub[qhub[i]->subs],instream);
		get_int(qhub[i]->mode[qhub[i]->subs],instream);
		if(qhub[i]->sub[qhub[i]->subs]<total_subs)
            sub[qhub[i]->sub[qhub[i]->subs]]->misc|=SUB_QNET;
		else
			continue;
		if(qhub[i]->sub[qhub[i]->subs]!=INVALID_SUB)
			qhub[i]->subs++; }
	for(j=0;j<32;j++)
		get_int(n,instream); }

total_qhubs=i;

for(j=0;j<32;j++)
	get_int(n,instream);

/************/
/* PostLink */
/************/

fread(str,11,1,instream);		/* Unused - used to be Site Name */
offset+=11;
get_int(sys_psnum,instream);	/* Site Number */
get_int(total_phubs,instream);

if(total_phubs) {
	if((phub=(phub_t **)MALLOC(sizeof(phub_t *)*total_phubs))==NULL)
		allocerr(txt,offset,fname,sizeof(phub_t*)*total_phubs); }
else
	phub=NULL;

for(i=0;i<total_phubs;i++) {
    if(feof(instream)) break;
    if((phub[i]=(phub_t *)MALLOC(sizeof(phub_t)))==NULL)
        allocerr(txt,offset,fname,sizeof(phub_t));
	memset(phub[i],0,sizeof(phub_t));
#ifdef SCFG
	get_str(phub[i]->name,instream);
#else
	fread(str,11,1,instream);
	offset+=11;
#endif
	get_int(phub[i]->time,instream);
	get_int(phub[i]->freq,instream);
	get_int(phub[i]->days,instream);
	get_int(phub[i]->node,instream);
	get_alloc(&offset,phub[i]->call,LEN_CMD,instream);
	for(j=0;j<32;j++)
		get_int(n,instream); }

total_phubs=i;

get_str(sys_psname,instream);	/* Site Name */

for(j=0;j<32;j++)
    get_int(n,instream);

/* Internet */

get_str(sys_inetaddr,instream); /* Internet address */
get_str(inetmail_sem,instream);
get_int(inetmail_misc,instream);
get_int(inetmail_cost,instream);

fclose(instream);
lprintf(txt.readit,fname);
}

#endif

