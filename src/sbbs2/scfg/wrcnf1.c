#line 2 "WRCNF1.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "scfg.h"

extern int no_msghdr,all_msghdr;
extern int no_dirchk;

char *crlf="\r\n";
char nulbuf[256]={0};
int  pslen;

#define put_int(var,stream) fwrite(&var,1,sizeof(var),stream)
#define put_str(var,stream) { pslen=strlen(var); \
							fwrite(var,1,pslen > sizeof(var) \
								? sizeof(var) : pslen ,stream); \
							fwrite(nulbuf,1,pslen > sizeof(var) \
								? 0 : sizeof(var)-pslen,stream); }

void write_node_cfg()
{
	char	str[128],cmd[64],c;
	int 	i,file;
	short	n;
	FILE	*stream;

memset(cmd,0,64);

upop("Writing NODE.CNF...");
sprintf(str,"%sNODE.CNF",node_path[node_num-1]);
backup(str);

if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
	|| (stream=fdopen(file,"wb"))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return; }
setvbuf(stream,NULL,_IOFBF,2048);

put_int(node_num,stream);
put_str(node_name,stream);
put_str(node_phone,stream);
put_str(node_comspec,stream);				 /* Was node_logon */
put_int(node_misc,stream);
put_int(node_ivt,stream);
put_int(node_swap,stream);
if(node_swapdir[0]) {
    backslash(node_swapdir);
    md(node_swapdir); }              /* make sure it's a valid directory */
put_str(node_swapdir,stream);
put_int(node_valuser,stream);
put_int(node_minbps,stream);
put_str(node_ar,stream);
put_int(node_dollars_per_call,stream);
put_str(node_editor,stream);
put_str(node_viewer,stream);
put_str(node_daily,stream);
put_int(node_scrnlen,stream);
put_int(node_scrnblank,stream);
backslash(ctrl_dir);
put_str(ctrl_dir,stream);
backslash(text_dir);
put_str(text_dir,stream);
backslash(temp_dir);
put_str(temp_dir,stream);
for(i=0;i<10;i++)
	put_str(wfc_cmd[i],stream);
for(i=0;i<12;i++)
	put_str(wfc_scmd[i],stream);
put_str(mdm_hang,stream);
put_int(node_sem_check,stream);
put_int(node_stat_check,stream);
put_str(scfg_cmd,stream);
put_int(sec_warn,stream);
put_int(sec_hangup,stream);
n=0;
for(i=0;i<188;i++)					/* unused init to NULL */
	fwrite(&n,1,2,stream);
n=0xffff;							/* unused init to 0xff */
for(i=0;i<256;i++)
    fwrite(&n,1,2,stream);
put_int(com_port,stream);
put_int(com_irq,stream);
put_int(com_base,stream);
put_int(com_rate,stream);
put_int(mdm_misc,stream);
put_str(mdm_init,stream);
put_str(mdm_spec,stream);
put_str(mdm_term,stream);
put_str(mdm_dial,stream);
put_str(mdm_offh,stream);
put_str(mdm_answ,stream);
put_int(mdm_reinit,stream);
put_int(mdm_ansdelay,stream);
put_int(mdm_rings,stream);
put_int(mdm_results,stream);
for(i=0;i<mdm_results;i++) {
	put_int(mdm_result[i].code,stream);
	put_int(mdm_result[i].rate,stream);
	put_int(mdm_result[i].cps,stream);
	put_str(mdm_result[i].str,stream); }
fclose(stream);
}

void free_node_cfg()
{

FREE(mdm_result);
}

void write_main_cfg()
{
	char	str[128],c=0;
	int 	file;
	short	i,j,n;
	FILE	*stream;

upop("Writing MAIN.CNF...");
sprintf(str,"%sMAIN.CNF",ctrl_dir);
backup(str);

if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
	|| (stream=fdopen(file,"wb"))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return; }
setvbuf(stream,NULL,_IOFBF,2048);

put_str(sys_name,stream);
put_str(sys_id,stream);
put_str(sys_location,stream);
put_str(sys_phonefmt,stream);
put_str(sys_op,stream);
put_str(sys_guru,stream);
put_str(sys_pass,stream);
put_int(sys_nodes,stream);
for(i=0;i<sys_nodes;i++) {
	backslash(node_path[i]);
	fwrite(node_path[i],LEN_DIR+1,1,stream); }
backslash(data_dir);
put_str(data_dir,stream);

/************************************************************/
/* Create data and sub-dirs off data if not already created */
/************************************************************/
strcpy(str,data_dir);
md(str);
sprintf(str,"%sSUBS",data_dir);
md(str);
sprintf(str,"%sDIRS",data_dir);
md(str);
sprintf(str,"%sTEXT",data_dir);
md(str);
sprintf(str,"%sMSGS",data_dir);
md(str);
sprintf(str,"%sUSER",data_dir);
md(str);
sprintf(str,"%sUSER\\PTRS",data_dir);
md(str);
sprintf(str,"%sLOGS",data_dir);
md(str);
sprintf(str,"%sQNET",data_dir);
md(str);
sprintf(str,"%sFILE",data_dir);
md(str);

backslash(exec_dir);
put_str(exec_dir,stream);
put_str(sys_logon,stream);
put_str(sys_logout,stream);
put_str(sys_daily,stream);
put_int(sys_timezone,stream);
put_int(sys_misc,stream);
put_int(sys_lastnode,stream);
put_int(sys_autonode,stream);
put_int(uq,stream);
put_int(sys_pwdays,stream);
put_int(sys_deldays,stream);
put_int(sys_exp_warn,stream);
put_int(sys_autodel,stream);
put_int(sys_def_stat,stream);
put_str(sys_chat_ar,stream);
put_int(cdt_min_value,stream);
put_int(max_minutes,stream);
put_int(cdt_per_dollar,stream);
put_str(new_pass,stream);
put_str(new_magic,stream);
put_str(new_sif,stream);
put_str(new_sof,stream);

put_int(new_level,stream);
put_int(new_flags1,stream);
put_int(new_flags2,stream);
put_int(new_flags3,stream);
put_int(new_flags4,stream);
put_int(new_exempt,stream);
put_int(new_rest,stream);
put_int(new_cdt,stream);
put_int(new_min,stream);
put_str(new_xedit,stream);
put_int(new_expire,stream);
if(new_shell>total_shells)
	new_shell=0;
put_int(new_shell,stream);
put_int(new_misc,stream);
put_int(new_prot,stream);
c=0;
put_int(c,stream);
n=0;
for(i=0;i<7;i++)
	put_int(n,stream);

put_int(expired_level,stream);
put_int(expired_flags1,stream);
put_int(expired_flags2,stream);
put_int(expired_flags3,stream);
put_int(expired_flags4,stream);
put_int(expired_exempt,stream);
put_int(expired_rest,stream);

put_str(logon_mod,stream);
put_str(logoff_mod,stream);
put_str(newuser_mod,stream);
put_str(login_mod,stream);
put_str(logout_mod,stream);
put_str(sync_mod,stream);
put_str(expire_mod,stream);
put_int(c,stream);
n=0;
for(i=0;i<224;i++)
	put_int(n,stream);
n=0xffff;
for(i=0;i<256;i++)
	put_int(n,stream);

n=0;
for(i=0;i<10;i++) {
	put_int(val_level[i],stream);
	put_int(val_expire[i],stream);
	put_int(val_flags1[i],stream);
	put_int(val_flags2[i],stream);
	put_int(val_flags3[i],stream);
	put_int(val_flags4[i],stream);
	put_int(val_cdt[i],stream);
	put_int(val_exempt[i],stream);
	put_int(val_rest[i],stream);
	for(j=0;j<8;j++)
		put_int(n,stream); }

c=0;
for(i=0;i<100 && !feof(stream);i++) {
	put_int(level_timeperday[i],stream);
	put_int(level_timepercall[i],stream);
	put_int(level_callsperday[i],stream);
	put_int(level_freecdtperday[i],stream);
	put_int(level_linespermsg[i],stream);
	put_int(level_postsperday[i],stream);
	put_int(level_emailperday[i],stream);
	put_int(level_misc[i],stream);
	put_int(level_expireto[i],stream);
	put_int(c,stream);
	for(j=0;j<5;j++)
		put_int(n,stream); }

/* Command Shells */

put_int(total_shells,stream);
for(i=0;i<total_shells;i++) {
	put_str(shell[i]->name,stream);
	put_str(shell[i]->code,stream);
	put_str(shell[i]->ar,stream);
	put_int(shell[i]->misc,stream);
	n=0;
	for(j=0;j<8;j++)
        put_int(n,stream); }

fclose(stream);
}

void free_main_cfg()
{
	int i;

for(i=0;i<sys_nodes;i++)
	FREE(node_path[i]);
FREE(node_path);
for(i=0;i<total_shells;i++)
	FREE(shell[i]);
FREE(shell);
}

void write_msgs_cfg()
{
	char	str[128],c;
	int 	i,j,k,file,x;
	short	n;
	long	l;
	FILE	*stream;
	smb_t	smb;

upop("Writing MSGS.CNF...");
sprintf(str,"%sMSGS.CNF",ctrl_dir);
backup(str);

if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1
	|| (stream=fdopen(file,"wb"))==NULL) {
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return; }
setvbuf(stream,NULL,_IOFBF,2048);

put_int(max_qwkmsgs,stream);
put_int(mail_maxcrcs,stream);
put_int(mail_maxage,stream);
put_str(preqwk_ar,stream);
put_int(smb_retry_time,stream);
n=0;
for(i=0;i<235;i++)
	put_int(n,stream);
n=0xffff;
for(i=0;i<256;i++)
	put_int(n,stream);

/* Message Groups */

put_int(total_grps,stream);
for(i=0;i<total_grps;i++) {
	put_str(grp[i]->lname,stream);
	put_str(grp[i]->sname,stream);
	put_str(grp[i]->ar,stream);
	n=0;
	for(j=0;j<32;j++)
		put_int(n,stream);
	n=0xffff;
	for(j=0;j<16;j++)
		put_int(n,stream); }

/* Message Sub-boards */

backslash(echomail_dir);

str[0]=0;
for(i=n=0;i<total_subs;i++)
	if(sub[i]->grp<total_grps)		/* total VALID sub-boards */
		n++;
put_int(n,stream);
for(i=0;i<total_subs;i++) {
	if(sub[i]->grp>=total_grps) 	/* skip bogus group numbers */
		continue;
	put_int(sub[i]->grp,stream);
	put_str(sub[i]->lname,stream);
	put_str(sub[i]->sname,stream);
	put_str(sub[i]->qwkname,stream);
	put_str(sub[i]->code,stream);
	if(sub[i]->data_dir[0])
		backslash(sub[i]->data_dir);
	put_str(sub[i]->data_dir,stream);
	md(sub[i]->data_dir);
	put_str(sub[i]->ar,stream);
	put_str(sub[i]->read_ar,stream);
	put_str(sub[i]->post_ar,stream);
	put_str(sub[i]->op_ar,stream);
	l=(sub[i]->misc&(~SUB_HDRMOD));    /* Don't write mod bit */
	put_int(l,stream);
	put_str(sub[i]->tagline,stream);
	put_str(sub[i]->origline,stream);
	put_str(sub[i]->echomail_sem,stream);
	if(sub[i]->echopath)
		backslash(sub[i]->echopath);
	put_str(sub[i]->echopath,stream);
	if(sub[i]->misc&SUB_FIDO && (echomail_dir[0] || sub[i]->echopath[0])) {
		md(echomail_dir);
		if(!sub[i]->echopath[0])
			sprintf(sub[i]->echopath,"%s%s",echomail_dir,sub[i]->code);
		md(sub[i]->echopath); }
	put_int(sub[i]->faddr,stream);
	put_int(sub[i]->maxmsgs,stream);
	put_int(sub[i]->maxcrcs,stream);
	put_int(sub[i]->maxage,stream);
	put_int(sub[i]->ptridx,stream);
	put_str(sub[i]->mod_ar,stream);
	put_int(sub[i]->qwkconf,stream);
	c=0;
	put_int(c,stream);
	n=0;
	for(k=0;k<26;k++)
		put_int(n,stream);

	if(all_msghdr || (sub[i]->misc&SUB_HDRMOD && !no_msghdr)) {
		if(!sub[i]->data_dir[0])
			sprintf(smb.file,"%sSUBS\\%s",data_dir,sub[i]->code);
		else
			sprintf(smb.file,"%s%s",sub[i]->data_dir,sub[i]->code);
		if((x=smb_open(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,x);
			continue; }
		if(!filelength(fileno(smb.shd_fp))) {
			smb.status.max_crcs=sub[i]->maxcrcs;
			smb.status.max_msgs=sub[i]->maxmsgs;
			smb.status.max_age=sub[i]->maxage;
			smb.status.attr=sub[i]->misc&SUB_HYPER ? SMB_HYPERALLOC :0;
			if((x=smb_create(&smb))!=0)
				errormsg(WHERE,ERR_CREATE,smb.file,x);
			smb_close(&smb);
			continue; }
		if((x=smb_locksmbhdr(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_LOCK,smb.file,x);
			continue; }
		if((x=smb_getstatus(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_READ,smb.file,x);
			continue; }
		if((!(sub[i]->misc&SUB_HYPER) || smb.status.attr&SMB_HYPERALLOC)
			&& smb.status.max_msgs==sub[i]->maxmsgs
			&& smb.status.max_crcs==sub[i]->maxcrcs
			&& smb.status.max_age==sub[i]->maxage) {	/* No change */
			smb_close(&smb);
			continue; }
		smb.status.max_msgs=sub[i]->maxmsgs;
		smb.status.max_crcs=sub[i]->maxcrcs;
		smb.status.max_age=sub[i]->maxage;
		if(sub[i]->misc&SUB_HYPER)
			smb.status.attr|=SMB_HYPERALLOC;
		if((x=smb_putstatus(&smb))!=0) {
			smb_close(&smb);
			errormsg(WHERE,ERR_WRITE,smb.file,x);
			continue; }
		smb_close(&smb); }

				}

/* FidoNet */

put_int(total_faddrs,stream);
for(i=0;i<total_faddrs;i++) {
	put_int(faddr[i].zone,stream);
	put_int(faddr[i].net,stream);
	put_int(faddr[i].node,stream);
	put_int(faddr[i].point,stream); }

put_str(origline,stream);
put_str(netmail_sem,stream);
put_str(echomail_sem,stream);
backslash(netmail_dir);
put_str(netmail_dir,stream);
put_str(echomail_dir,stream);
backslash(fidofile_dir);
put_str(fidofile_dir,stream);
put_int(netmail_misc,stream);
put_int(netmail_cost,stream);
put_int(dflt_faddr,stream);
n=0;
for(i=0;i<28;i++)
	put_int(n,stream);

/* QWKnet Config */

put_str(qnet_tagline,stream);

put_int(total_qhubs,stream);
for(i=0;i<total_qhubs;i++) {
	put_str(qhub[i]->id,stream);
	put_int(qhub[i]->time,stream);
	put_int(qhub[i]->freq,stream);
	put_int(qhub[i]->days,stream);
	put_int(qhub[i]->node,stream);
	put_str(qhub[i]->call,stream);
	put_str(qhub[i]->pack,stream);
	put_str(qhub[i]->unpack,stream);
	put_int(qhub[i]->subs,stream);
	for(j=0;j<qhub[i]->subs;j++) {
		put_int(qhub[i]->conf[j],stream);
		put_int(qhub[i]->sub[j],stream);
		put_int(qhub[i]->mode[j],stream); }
	n=0;
	for(j=0;j<32;j++)
		put_int(n,stream); }
n=0;
for(i=0;i<32;i++)
    put_int(n,stream);

/* PostLink Config */

memset(str,0,11);
fwrite(str,11,1,stream);	/* unused, used to be site name */
put_int(sys_psnum,stream);

put_int(total_phubs,stream);
for(i=0;i<total_phubs;i++) {
	put_str(phub[i]->name,stream);
	put_int(phub[i]->time,stream);
	put_int(phub[i]->freq,stream);
	put_int(phub[i]->days,stream);
	put_int(phub[i]->node,stream);
	put_str(phub[i]->call,stream);
	n=0;
	for(j=0;j<32;j++)
		put_int(n,stream); }

put_str(sys_psname,stream);
n=0;
for(i=0;i<32;i++)
    put_int(n,stream);

put_str(sys_inetaddr,stream); /* Internet address */
put_str(inetmail_sem,stream);
put_int(inetmail_misc,stream);
put_int(inetmail_cost,stream);

fclose(stream);

/* MUST BE AT END */

if(!no_msghdr) {
	backslash(data_dir);
	sprintf(smb.file,"%sMAIL",data_dir);
	if((x=smb_open(&smb))!=0) {
		errormsg(WHERE,ERR_OPEN,smb.file,x);
		return; }
	if(!filelength(fileno(smb.shd_fp))) {
		smb.status.max_crcs=mail_maxcrcs;
		smb.status.max_msgs=MAX_SYSMAIL;
		smb.status.max_age=mail_maxage;
		smb.status.attr=SMB_EMAIL;
		if((x=smb_create(&smb))!=0)
			errormsg(WHERE,ERR_CREATE,smb.file,x);
		smb_close(&smb);
		return; }
	if((x=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_LOCK,smb.file,x);
		return; }
	if((x=smb_getstatus(&smb))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_READ,smb.file,x);
		return; }
	smb.status.max_msgs=MAX_SYSMAIL;
	smb.status.max_crcs=mail_maxcrcs;
	smb.status.max_age=mail_maxage;
	if((x=smb_putstatus(&smb))!=0) {
		smb_close(&smb);
		errormsg(WHERE,ERR_WRITE,smb.file,x);
		return; }
	smb_close(&smb); }
}

void free_msgs_cfg()
{
	int i;

for(i=0;i<total_grps;i++)
	FREE(grp[i]);
FREE(grp);
grp=NULL;

for(i=0;i<total_subs;i++)
	FREE(sub[i]);
FREE(sub);
sub=NULL;

FREE(faddr);
total_faddrs=0;

for(i=0;i<total_qhubs;i++) {
	FREE(qhub[i]->mode);
	FREE(qhub[i]->conf);
	FREE(qhub[i]->sub);
	FREE(qhub[i]); }
FREE(qhub);
qhub=NULL;

for(i=0;i<total_phubs;i++)
	FREE(phub[i]);
FREE(phub);
phub=NULL;

}

