#line 1 "EXEC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"

char *readtext(long *line, FILE *stream);

extern csi_t main_csi;
extern FILE *nodefile_fp,*node_ext_fp,*logfile_fp;

uint global_str_vars=0;
char **global_str_var=0;
long *global_str_var_name=0;
uint global_int_vars=0;
long *global_int_var=0;
long *global_int_var_name=0;

char *sysvar_p[MAX_SYSVARS]={NULL};
int sysvar_pi=0;
long sysvar_l[MAX_SYSVARS]={0L};
int sysvar_li=0;

char **getstrvar(csi_t *bin, long name)
{
	int i;

if(sysvar_pi>=MAX_SYSVARS) sysvar_pi=0;
switch(name) {
	case 0:
		return((char **)&(bin->str));
	case 0x490873f1:
		sysvar_p[sysvar_pi]=useron.alias;
		break;
	case 0x5de44e8b:
		sysvar_p[sysvar_pi]=useron.name;
		break;
	case 0x979ef1de:
		sysvar_p[sysvar_pi]=useron.handle;
		break;
	case 0xc8cd5fb7:
		sysvar_p[sysvar_pi]=useron.comp;
		break;
	case 0xcc7aca99:
		sysvar_p[sysvar_pi]=useron.note;
		break;
	case 0xa842c43b:
		sysvar_p[sysvar_pi]=useron.address;
		break;
	case 0x4ee1ff3a:
		sysvar_p[sysvar_pi]=useron.location;
		break;
	case 0xf000aa78:
		sysvar_p[sysvar_pi]=useron.zipcode;
		break;
	case 0xcdb7e4a9:
		sysvar_p[sysvar_pi]=useron.pass;
		break;
	case 0x94d59a7a:
		sysvar_p[sysvar_pi]=useron.birth;
		break;
	case 0xec2b8fb8:
		sysvar_p[sysvar_pi]=useron.phone;
		break;
	case 0x08f65a2a:
		sysvar_p[sysvar_pi]=useron.modem;
		break;
	case 0xc7e0e8ce:
		sysvar_p[sysvar_pi]=useron.netmail;
		break;
	case 0xd3606303:
		sysvar_p[sysvar_pi]=useron.tmpext;
		break;
	case 0x3178f9d6:
		sysvar_p[sysvar_pi]=useron.comment;
		break;

    case 0x41239e21:
		sysvar_p[sysvar_pi]=connection;
		break;
	case 0xe9f1fad0:
		sysvar_p[sysvar_pi]=cap_fname;
		break;
	case 0x90fc82b4:
		sysvar_p[sysvar_pi]=cid;
		break;
	case 0x15755030:
		return((char **)&comspec);

	case 0xf19cd046:
		sysvar_p[sysvar_pi]=wordwrap;
		break;

	default:
		if(bin->str_var && bin->str_var_name)
			for(i=0;i<bin->str_vars;i++)
				if(bin->str_var_name[i]==name)
					return((char **)&(bin->str_var[i]));
		if(global_str_var && global_str_var_name)
			for(i=0;i<global_str_vars;i++)
				if(global_str_var_name[i]==name)
					return(&(global_str_var[i]));
		return(NULL); }

return((char **)&sysvar_p[sysvar_pi++]);
}

long *getintvar(csi_t *bin, long name)
{
	int i;

if(sysvar_li==MAX_SYSVARS) sysvar_li=0;
switch(name) {
	case 0:
		sysvar_l[sysvar_li]=strtol(bin->str,0,0);
		break;
	case 0x908ece53:
		sysvar_l[sysvar_li]=useron.number;
		break;
	case 0xdcedf626:
		sysvar_l[sysvar_li]=useron.uls;
		break;
	case 0xc1093f61:
		sysvar_l[sysvar_li]=useron.dls;
		break;
	case 0x2039a29f:
		sysvar_l[sysvar_li]=useron.posts;
		break;
	case 0x4a9f3955:
		sysvar_l[sysvar_li]=useron.emails;
		break;
	case 0x0c8dcf3b:
		sysvar_l[sysvar_li]=useron.fbacks;
		break;
	case 0x9a13bf95:
		sysvar_l[sysvar_li]=useron.etoday;
		break;
	case 0xc9082cbd:
		sysvar_l[sysvar_li]=useron.ptoday;
		break;
	case 0x7c72376d:
		sysvar_l[sysvar_li]=useron.timeon;
		break;
	case 0xac72c50b:
		sysvar_l[sysvar_li]=useron.textra;
		break;
	case 0x04807a11:
		sysvar_l[sysvar_li]=useron.logons;
		break;
	case 0x52996eab:
		sysvar_l[sysvar_li]=useron.ttoday;
		break;
	case 0x098bdfcb:
		sysvar_l[sysvar_li]=useron.tlast;
		break;
	case 0xbd1cee5d:
		sysvar_l[sysvar_li]=useron.ltoday;
		break;
	case 0x07954570:
		sysvar_l[sysvar_li]=useron.xedit;
		break;
	case 0xedf6aa98:
		sysvar_l[sysvar_li]=useron.shell;
		break;
	case 0x328ed476:
		sysvar_l[sysvar_li]=useron.level;
		break;
	case 0x9e70e855:
		sysvar_l[sysvar_li]=useron.sex;
		break;
	case 0x094cc42c:
		sysvar_l[sysvar_li]=useron.rows;
		break;
	case 0xabc4317e:
		sysvar_l[sysvar_li]=useron.prot;
		break;
	case 0x7dd9aac0:
		sysvar_l[sysvar_li]=useron.leech;
		break;
	case 0x7c602a37:
		return((long *)&useron.misc);
	case 0x61be0d36:
		return((long *)&useron.qwk);
	case 0x665ac227:
		return((long *)&useron.chat);
	case 0x951341ab:
		return((long *)&useron.flags1);
	case 0x0c1a1011:
		return((long *)&useron.flags2);
	case 0x7b1d2087:
		return((long *)&useron.flags3);
	case 0xe579b524:
		return((long *)&useron.flags4);
	case 0x12e7d6d2:
		return((long *)&useron.exempt);
	case 0xfed3115d:
		return((long *)&useron.rest);
	case 0xb65dd6d4:
		return((long *)&useron.ulb);
	case 0xabb91f93:
		return((long *)&useron.dlb);
	case 0x92fb364f:
		return((long *)&useron.cdt);
	case 0xd0a99c72:
		return((long *)&useron.min);
	case 0xd7ae3022:
		return((long *)&useron.freecdt);
	case 0x1ef214ef:
		return((long *)&useron.firston);
	case 0x0ea515b1:
		return((long *)&useron.laston);
	case 0x2aaf9bd3:
		return((long *)&useron.expire);
	case 0x89c91dc8:
		return((long *)&useron.pwmod);
	case 0x5b0d0c54:
		return((long *)&useron.ns_time);

	case 0xae256560:
		return((long *)&cur_rate);
	case 0x2b3c257f:
		return((long *)&cur_cps);
	case 0x1c4455ee:
		return((long *)&dte_rate);
	case 0x7fbf958e:
		return((long *)&lncntr);
	case 0x5c1c1500:
		return((long *)&tos);
	case 0x613b690e:
		return((long *)&rows);
	case 0x205ace36:
		return((long *)&autoterm);
	case 0x7d0ed0d1:
		return((long *)&console);
	case 0xbf31a280:
		return((long *)&answertime);
	case 0x83aa2a6a:
		return((long *)&logontime);
	case 0xb50cb889:
		return((long *)&ns_time);
	case 0xae92d249:
		return((long *)&last_ns_time);
	case 0x97f99eef:
		return((long *)&online);
	case 0x381d3c2a:
		return((long *)&sys_status);
	case 0x7e29c819:
		return((long *)&sys_misc);
	case 0x11c83294:
		return((long *)&sys_psnum);
	case 0x02408dc5:
		sysvar_l[sysvar_li]=sys_timezone;
		break;
	case 0x78afeaf1:
		sysvar_l[sysvar_li]=sys_pwdays;
		break;
	case 0xd859385f:
		sysvar_l[sysvar_li]=sys_deldays;
		break;
	case 0x6392dc62:
		sysvar_l[sysvar_li]=sys_autodel;
		break;
	case 0x698d59b4:
		sysvar_l[sysvar_li]=sys_nodes;
		break;
	case 0x6fb1c46e:
		sysvar_l[sysvar_li]=sys_exp_warn;
		break;
	case 0xdf391ca7:
		sysvar_l[sysvar_li]=sys_lastnode;
		break;
	case 0xdd982780:
		sysvar_l[sysvar_li]=sys_autonode;
		break;
	case 0xf53db6c7:
		sysvar_l[sysvar_li]=node_scrnlen;
		break;
	case 0xa1f0fcb7:
		sysvar_l[sysvar_li]=node_scrnblank;
		break;
	case 0x709c07da:
		return((long *)&node_misc);
	case 0xb17e7914:
		sysvar_l[sysvar_li]=node_valuser;
		break;
	case 0xadae168a:
		sysvar_l[sysvar_li]=node_ivt;
		break;
	case 0x2aa89801:
		sysvar_l[sysvar_li]=node_swap;
		break;
	case 0x4f02623a:
		sysvar_l[sysvar_li]=node_minbps;
		break;
	case 0xe7a7fb07:
		sysvar_l[sysvar_li]=node_num;
		break;
	case 0x6c8e350a:
		sysvar_l[sysvar_li]=new_level;
		break;
	case 0xccfe7c5d:
		return((long *)&new_flags1);
	case 0x55f72de7:
		return((long *)&new_flags2);
	case 0x22f01d71:
		return((long *)&new_flags3);
	case 0xbc9488d2:
		return((long *)&new_flags4);
	case 0x4b0aeb24:
		return((long *)&new_exempt);
	case 0x20cb6325:
		return((long *)&new_rest);
	case 0x31178ba2:
		return((long *)&new_cdt);
	case 0x7345219f:
		return((long *)&new_min);
	case 0xb3f64be4:
		sysvar_l[sysvar_li]=new_shell;
		break;
	case 0xa278584f:
		return((long *)&new_misc);
	case 0x7342a625:
		sysvar_l[sysvar_li]=new_expire;
		break;
	case 0x75dc4306:
		sysvar_l[sysvar_li]=new_prot;
		break;
	case 0xfb394e27:
		sysvar_l[sysvar_li]=expired_level;
		break;
	case 0x89b69753:
		return((long *)&expired_flags1);
	case 0x10bfc6e9:
		return((long *)&expired_flags2);
	case 0x67b8f67f:
		return((long *)&expired_flags3);
	case 0xf9dc63dc:
		return((long *)&expired_flags4);
	case 0x0e42002a:
		return((long *)&expired_exempt);
	case 0x4569c62e:
		return((long *)&expired_rest);
	case 0xfcf3542e:
		sysvar_l[sysvar_li]=min_dspace;
		break;
	case 0xcf9ce02c:
		sysvar_l[sysvar_li]=cdt_min_value;
		break;
	case 0xfcb5b274:
		return((long *)&cdt_per_dollar);
	case 0x4db200d2:
		sysvar_l[sysvar_li]=leech_pct;
		break;
	case 0x9a7d9cca:
		sysvar_l[sysvar_li]=leech_sec;
		break;
	case 0x396b7167:
		return((long *)&netmail_cost);
	case 0x5eeaff21:
		sysvar_l[sysvar_li]=netmail_misc;
		break;
	case 0x82d9484e:
		return((long *)&inetmail_cost);
	case 0xe558c608:
		return((long *)&inetmail_misc);

	case 0xc6e8539d:
		return((long *)&logon_ulb);
	case 0xdb0c9ada:
		return((long *)&logon_dlb);
	case 0xac58736f:
		return((long *)&logon_uls);
	case 0xb1bcba28:
		return((long *)&logon_dls);
	case 0x9c5051c9:
		return((long *)&logon_posts);
	case 0xc82ba467:
		return((long *)&logon_emails);
	case 0x8e395209:
		return((long *)&logon_fbacks);
	case 0x8b12ba9d:
		return((long *)&posts_read);
	case 0xe51c1956:
		sysvar_l[sysvar_li]=(ulong)logfile_fp;
		break;
	case 0x5a22d4bd:
		sysvar_l[sysvar_li]=(ulong)nodefile_fp;
		break;
	case 0x3a37c26b:
		sysvar_l[sysvar_li]=(ulong)node_ext_fp;
		break;

	case 0xeb6c9c73:
		sysvar_l[sysvar_li]=errorlevel;
		break;

	case 0x5aaccfc5:
		sysvar_l[sysvar_li]=errno;
		break;

	case 0x057e4cd4:
		sysvar_l[sysvar_li]=timeleft;
		break;

	case 0x1e5052a7:
		return((long *)&max_minutes);
	case 0xedc643f1:
		return((long *)&max_qwkmsgs);

	case 0x430178ec:
		return((long *)&uq);

	default:
		if(bin->int_var && bin->int_var_name)
			for(i=0;i<bin->int_vars;i++)
				if(bin->int_var_name[i]==name)
					return(&bin->int_var[i]);
		if(global_int_var && global_int_var_name)
			for(i=0;i<global_int_vars;i++)
				if(global_int_var_name[i]==name)
					return(&global_int_var[i]);
		return(NULL); }

return(&sysvar_l[sysvar_li++]);
}

void clearvars(csi_t *bin)
{
bin->str_vars=0;
bin->str_var=NULL;
bin->str_var_name=NULL;
bin->int_vars=0;
bin->int_var=NULL;
bin->int_var_name=NULL;
bin->files=0;
bin->retval=0;
}

void freevars(csi_t *bin)
{
	int i;

if(bin->str_var) {
	for(i=0;i<bin->str_vars;i++)
		if(bin->str_var[i])
			FREE(bin->str_var[i]);
	FREE(bin->str_var); }
if(bin->int_var)
	FREE(bin->int_var);
if(bin->str_var_name)
	FREE(bin->str_var_name);
if(bin->int_var_name)
	FREE(bin->int_var_name);
for(i=0;i<bin->files;i++)
	if(bin->file[i]) {
		fclose((FILE *)bin->file[i]);
		bin->file[i]=0; }
}

/****************************************************************************/
/* Copies a new value (str) into the string variable pointed to by p		*/
/* re-allocating if necessary												*/
/****************************************************************************/
char *copystrvar(csi_t *csi, char *p, char *str)
{
	char *np;	/* New pointer after realloc */
	int i;

if(p!=csi->str) {
	if(p)
		for(i=0;i<MAX_SYSVARS;i++)
			if(p==sysvar_p[i])
				break;
	if(!p || i==MAX_SYSVARS) {		/* Not system variable */
		if((np=REALLOC(p,strlen(str)+1))==NULL)
			errormsg(WHERE,ERR_ALLOC,"variable",strlen(str)+1);
		else
			p=np; } }
if(p)
	strcpy(p,str);
return(p);
}


long exec_bin(uchar *mod, csi_t *csi)
{
    char    str[128];
	int 	i,file;
    csi_t   bin;

//lprintf("%s %d\r\n",__FILE__,__LINE__);
memcpy(&bin,csi,sizeof(csi_t));
clearvars(&bin);

sprintf(str,"%s%s.BIN",exec_dir,mod);
if((file=nopen(str,O_RDONLY|O_BINARY))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_BINARY);
	return(-1); }

//lprintf("%s %d\r\n",__FILE__,__LINE__);
bin.length=filelength(file);
if((bin.cs=(uchar *)MALLOC(bin.length))==NULL) {
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,bin.length);
	return(-1); }
if(lread(file,bin.cs,bin.length)!=bin.length) {
    close(file);
    errormsg(WHERE,ERR_READ,str,bin.length);
    FREE(bin.cs);
	return(-1); }
close(file);

bin.ip=bin.cs;
bin.rets=bin.cmdrets=bin.misc=0;
//lprintf("%s %d\r\n",__FILE__,__LINE__);
while(exec(&bin)==0)
	if(!(bin.misc&CS_OFFLINE_EXEC)) {
		checkline();
		if(!online)
			break; }
//lprintf("%s %d logic=%d\r\n",__FILE__,__LINE__,bin.logic);
freevars(&bin);
FREE(bin.cs);
csi->logic=bin.logic;
return(bin.retval);
}

/****************************************************************************/
/* Skcsi->ip to a specific instruction                                           */
/****************************************************************************/
void skipto(csi_t *csi, uchar inst)
{
	int i,j;

while(csi->ip<csi->cs+csi->length && ((inst&0x80) || *csi->ip!=inst)) {

	if(*csi->ip==CS_IF_TRUE || *csi->ip==CS_IF_FALSE
		|| (*csi->ip>=CS_IF_GREATER && *csi->ip<=CS_IF_LESS_OR_EQUAL)) {
        csi->ip++;
        skipto(csi,CS_ENDIF);
        csi->ip++;
        continue; }

    if(inst==CS_ELSEORENDIF
        && (*csi->ip==CS_ELSE || *csi->ip==CS_ENDIF))
        break;

	if(inst==CS_NEXTCASE
		&& (*csi->ip==CS_CASE || *csi->ip==CS_DEFAULT
			|| *csi->ip==CS_END_SWITCH))
		break;

	if(*csi->ip==CS_SWITCH) {
		csi->ip++;
		csi->ip+=4; /* Skip variable name */
		skipto(csi,CS_END_SWITCH);
		csi->ip++;
		continue; }

    if(*csi->ip==CS_CASE) {
        csi->ip++;
        csi->ip+=4; /* Skip value */
        skipto(csi,CS_NEXTCASE);
        continue; }

    if(*csi->ip==CS_CMDKEY) {
        csi->ip+=2;
        skipto(csi,CS_END_CMD);
        csi->ip++;
        continue; }
	if(*csi->ip==CS_CMDSTR || *csi->ip==CS_CMDKEYS) {
        csi->ip++;              /* skip inst */
        while(*(csi->ip++));    /* skip string */
        skipto(csi,CS_END_CMD);
        csi->ip++;
        continue; }

    if(*csi->ip>=CS_FUNCTIONS) {
        csi->ip++;
        continue; }

    if(*csi->ip>=CS_MISC) {
        switch(*csi->ip) {
			case CS_VAR_INSTRUCTION:
				csi->ip++;
				switch(*(csi->ip++)) {
					case SHOW_VARS:
						continue;
					case PRINT_VAR:
					case DEFINE_STR_VAR:
					case DEFINE_INT_VAR:
					case DEFINE_GLOBAL_STR_VAR:
					case DEFINE_GLOBAL_INT_VAR:
					case TIME_INT_VAR:
					case STRUPR_VAR:
					case STRLWR_VAR:
					case TRUNCSP_STR_VAR:
					case CHKFILE_VAR:
					case STRIP_CTRL_STR_VAR:
						csi->ip+=4; /* Skip variable name */
						continue;
					case GETSTR_VAR:
					case GETNAME_VAR:
					case GETLINE_VAR:
					case GETSTRUPR_VAR:
					case SHIFT_STR_VAR:
					case SEND_FILE_VIA_VAR:
					case RECEIVE_FILE_VIA_VAR:
						csi->ip+=4; /* Skip variable name */
						csi->ip++;	/* Skip char */
						continue;
					case PRINTTAIL_VAR_MODE:
						csi->ip++;	/* Skip length */
					case PRINTFILE_VAR_MODE:
					case GETNUM_VAR:
						csi->ip+=4; /* Skip variable name */
						csi->ip+=2;  /* Skip max num */
                        continue;
					case STRNCMP_VAR:
						csi->ip++;	/* Skip length */
					case SET_STR_VAR:
					case COMPARE_STR_VAR:
					case CAT_STR_VAR:
					case STRSTR_VAR:
						csi->ip+=4; /* Skip variable name */
						while(*(csi->ip++));	/* skip string */
						continue;
					case FORMAT_TIME_STR:
						csi->ip+=4; /* Skip destination variable */
						while(*(csi->ip++));	/* Skip string */
						csi->ip+=4; /* Skip int variable */
						continue;
					case FORMAT_STR_VAR:	/* SPRINTF */
						csi->ip+=4; /* Skip destination variable */
					case VAR_PRINTF:
						while(*(csi->ip++));	/* Skip string */
						j=*(csi->ip++); /* Skip number of arguments */
						for(i=0;i<j;i++)
							csi->ip+=4; /* Skip arguments */
						continue;
					case SEND_FILE_VIA:
					case RECEIVE_FILE_VIA:
						csi->ip++;				/* Skip prot */
						while(*(csi->ip++));	/* Skip filepath */
						continue;
					case GETSTR_MODE:
					case STRNCMP_VARS:
						csi->ip++;	/* Skip length */
					default:
						csi->ip+=8; /* Skip two variable names or var & val */
						continue; }

			case CS_FIO_FUNCTION:
				csi->ip++;
				switch(*(csi->ip++)) {
					case FIO_OPEN:
						csi->ip+=4; 			/* File handle */
						csi->ip+=2; 			/* Access */
						while(*(csi->ip++));	/* path/filename */
						continue;
					case FIO_CLOSE:
					case FIO_FLUSH:
					case FIO_EOF:
					case REMOVE_FILE:
					case REMOVE_DIR:
					case CHANGE_DIR:
					case MAKE_DIR:
					case REWIND_DIR:
					case CLOSE_DIR:
						csi->ip+=4; 			/* File handle */
						continue;
					case FIO_SET_ETX:
						csi->ip++;
						continue;
					case FIO_PRINTF:
						csi->ip+=4; 			/* File handle */
						while(*(csi->ip++));	/* String */
						j=*(csi->ip++); 		/* Number of arguments */
						for(i=0;i<j;i++)
							csi->ip+=4; 		/* Arguments */
                        continue;
					case FIO_READ:
					case FIO_WRITE:
					case FIO_SEEK:
					case FIO_SEEK_VAR:
					case FIO_OPEN_VAR:
						csi->ip+=4; 			/* File handle */
						csi->ip+=4; 			/* Variable */
						csi->ip+=2; 			/* Length/access */
						continue;
					case FIO_READ_VAR:
					case FIO_WRITE_VAR:
						csi->ip+=4; 			/* File handle */
						csi->ip+=4; 			/* Buf Variable */
						csi->ip+=4; 			/* Length Variable */
                        continue;
					default:
                        csi->ip+=4;             /* File handle */
                        csi->ip+=4;             /* Variable */
						continue; }

			case CS_COMPARE_ARS:
                csi->ip++;
                csi->ip+=(*csi->ip);
                csi->ip++;
                break;
            case CS_TOGGLE_USER_MISC:
            case CS_COMPARE_USER_MISC:
			case CS_TOGGLE_USER_CHAT:
			case CS_COMPARE_USER_CHAT:
			case CS_TOGGLE_USER_QWK:
			case CS_COMPARE_USER_QWK:
                csi->ip+=5;
                break;
            case CS_REPLACE_TEXT:
                csi->ip+=3;     /* skip inst and text # */
                while(*(csi->ip++));    /* skip string */
                break;
			case CS_USE_INT_VAR:
				csi->ip+=7; // inst, var, offset, len
				break;
            default:
                csi->ip++; }
        continue; }

	if(*csi->ip==CS_ONE_MORE_BYTE) {
		csi->ip++;				/* skip extension */
		csi->ip++;				/* skip instruction */
		continue; }

	if(*csi->ip==CS_TWO_MORE_BYTES) {
		csi->ip++;				/* skip extension */
		csi->ip++;				/* skip instruction */
		csi->ip++;				/* skip argument */
        continue; }

	if(*csi->ip==CS_THREE_MORE_BYTES) {
		csi->ip++;				/* skip extension */
		csi->ip++;				/* skip instruction */
		csi->ip+=2; 			/* skip argument */
        continue; }

	if(*csi->ip==CS_STR_FUNCTION) {
		csi->ip++;				/* skip extension */
		csi->ip++;				/* skip instruction */
        while(*(csi->ip++));    /* skip string */
        continue; }

    if(*csi->ip>=CS_ASCIIZ) {
        csi->ip++;              /* skip inst */
        while(*(csi->ip++));    /* skip string */
        continue; }

    if(*csi->ip>=CS_THREE_BYTE) {
        csi->ip+=3;
        continue; }

    if(*csi->ip>=CS_TWO_BYTE) {
        csi->ip+=2;
        continue; }

    csi->ip++; }
}


int exec(csi_t *csi)
{
	uchar	str[256],tmp2[128],buf[1025],*path,ch,*p,**pp,**pp1,**pp2;
	int 	i,j,k,s,file,x,y;
	long	l,*lp,*lp1,*lp2;
	stats_t stats;
	time_t	t;
	FILE	*stream;

//lprintf("line %d (%d %d %d)\r\n",__LINE__,curgrp,cursub[curgrp],usrsub[curgrp]);
#if 0
if(curgrp>=total_grps)
	curgrp=0;
if(cursub[curgrp]>=total_subs)
	cursub[curgrp]=0;
if(curlib>=total_libs)
	curlib=0;
if(curdir[curlib]>=total_dirs)
	curdir[curlib]=0;
#endif

if(usrgrps)
	cursubnum=usrsub[curgrp][cursub[curgrp]];		/* Used for ARS */
else
	cursubnum=INVALID_SUB;
if(usrlibs) {
	curdirnum=usrdir[curlib][curdir[curlib]];		/* Used for ARS */
	path=dir[usrdir[curlib][curdir[curlib]]]->path; }
else {
	curdirnum=INVALID_DIR;
	path=nulstr; }
now=time(NULL);

if(csi->ip>=csi->cs+csi->length)
	return(1);

//lprintf("%04X: %02X ",(uint)(csi->ip-csi->cs),*csi->ip);

if(*csi->ip>=CS_FUNCTIONS)
	return(exec_function(csi));

/**********************************************/
/* Miscellaneous variable length instructions */
/**********************************************/

if(*csi->ip>=CS_MISC)
	return(exec_misc(csi,path));

/********************************/
/* ASCIIZ argument instructions */
/********************************/

if(*csi->ip>=CS_ASCIIZ) {
	switch(*(csi->ip++)) {
		case CS_STR_FUNCTION:
			switch(*(csi->ip++)) {
				case CS_LOGIN:
					csi->logic=login(csi->str,csi->ip);
					break;
				case CS_LOAD_TEXT:
					csi->logic=LOGIC_FALSE;
					for(i=0;i<TOTAL_TEXT;i++)
						if(text[i]!=text_sav[i]) {
							if(text[i]!=nulstr)
								FREE(text[i]);
							text[i]=text_sav[i]; }
					sprintf(str,"%s%s.DAT"
						,ctrl_dir,cmdstr(csi->ip,path,csi->str,buf));
					if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
						errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
						break; }
					for(i=0;i<TOTAL_TEXT && !feof(stream);i++) {
						if((text[i]=readtext((long *)NULL,stream))==NULL) {
							i--;
							continue; }
						if(!strcmp(text[i],text_sav[i])) {	/* If identical */
							FREE(text[i]);					/* Don't alloc */
							text[i]=text_sav[i]; }
						else if(text[i][0]==0) {
							FREE(text[i]);
							text[i]=nulstr; } }
					if(i<TOTAL_TEXT) {
						fclose(stream);
						errormsg(WHERE,ERR_READ,str,TOTAL_TEXT);
						break; }
					fclose(stream);
					csi->logic=LOGIC_TRUE;
					break;
				default:
					errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
					break; }
			while(*(csi->ip++));	 /* Find NULL */
			return(0);
		case CS_LOG:
			log(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_GETCMD:
			csi->cmd=getkeys(csi->ip,0);
			if((char)csi->cmd==-1)
				csi->cmd=3;
			break;
		case CS_CMDSTR:
			if(stricmp(csi->str,csi->ip)) {
				while(*(csi->ip++));		/* Find NULL */
				skipto(csi,CS_END_CMD);
				csi->ip++;
				return(0); }
			break;
		case CS_CMDKEYS:
			for(i=0;csi->ip[i];i++)
				if(csi->cmd==csi->ip[i])
					break;
			if(!csi->ip[i]) {
				while(*(csi->ip++));		/* Find NULL */
				skipto(csi,CS_END_CMD);
				csi->ip++;
				return(0); }
			break;
		case CS_GET_TEMPLATE:
			gettmplt(csi->str,csi->ip,K_LINE);
			if(sys_status&SS_ABORT)
				csi->str[0]=0;
			csi->cmd=csi->str[0];
			break;
		case CS_TRASHCAN:
			csi->logic=!trashcan(csi->str,csi->ip);
			break;
		case CS_CREATE_SIF:
			create_sif_dat(csi->ip,csi->str);
			break;
		case CS_READ_SIF:
			read_sif_dat(csi->ip,csi->str);
			break;
		case CS_MNEMONICS:
			mnemonics(csi->ip);
			break;
		case CS_PRINT:
			putmsg(cmdstr(csi->ip,path,csi->str,buf),P_SAVEATR|P_NOABORT);
			break;
		case CS_PRINT_LOCAL:
			lputs(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_PRINT_REMOTE:
			putcom(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_PRINTFILE:
			printfile(cmdstr(csi->ip,path,csi->str,buf),P_SAVEATR);
			break;
		case CS_PRINTFILE_REMOTE:
			if(online!=ON_REMOTE || !(console&CON_R_ECHO))
				break;
			console&=~CON_L_ECHO;
			printfile(cmdstr(csi->ip,path,csi->str,buf),P_SAVEATR);
			console|=CON_L_ECHO;
			break;
		case CS_PRINTFILE_LOCAL:
			if(!(console&CON_L_ECHO))
				break;
			console&=~CON_R_ECHO;
			printfile(cmdstr(csi->ip,path,csi->str,buf),P_SAVEATR);
			console|=CON_R_ECHO;
			break;
		case CS_CHKFILE:
			csi->logic=!fexist(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_EXEC:
			external(cmdstr(csi->ip,path,csi->str,buf),0);
			break;
		case CS_EXEC_INT:
			external(cmdstr(csi->ip,path,csi->str,buf),EX_OUTR|EX_INR|EX_OUTL);
			break;
		case CS_EXEC_XTRN:
			for(i=0;i<total_xtrns;i++)
				if(!stricmp(xtrn[i]->code,csi->ip))
					break;
			if(i<total_xtrns)
				exec_xtrn(i);
			break;
		case CS_EXEC_BIN:
			exec_bin(cmdstr(csi->ip,path,csi->str,buf),csi);
			break;
		case CS_YES_NO:
			csi->logic=!yesno(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_NO_YES:
			csi->logic=!noyes(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_MENU:
			menu(cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_SETSTR:
			strcpy(csi->str,cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_SET_MENU_DIR:
			cmdstr(csi->ip,path,csi->str,menu_dir);
			break;
		case CS_SET_MENU_FILE:
			cmdstr(csi->ip,path,csi->str,menu_file);
			break;
		case CS_COMPARE_STR:
			csi->logic=stricmp(csi->str,cmdstr(csi->ip,path,csi->str,buf));
			break;
		case CS_COMPARE_KEYS:
			for(i=0;csi->ip[i];i++)
				if(csi->cmd==csi->ip[i])
					break;
			if(csi->ip[i])
				csi->logic=LOGIC_TRUE;
			else
				csi->logic=LOGIC_FALSE;
			break;
		case CS_COMPARE_WORD:
			csi->logic=strnicmp(csi->str,csi->ip,strlen(csi->ip));
			break;
		default:
			errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
			break; }
	while(*(csi->ip++));	 /* Find NULL */
	return(0); }

if(*csi->ip>=CS_THREE_BYTE) {
	switch(*(csi->ip++)) {
		case CS_THREE_MORE_BYTES:
			switch(*(csi->ip++)) {
				}
			errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
			return(0);
		case CS_GOTO:
			csi->ip=csi->cs+*((ushort *)(csi->ip));
			return(0);
		case CS_CALL:
			csi->ret[csi->rets++]=csi->ip+2;
			csi->ip=csi->cs+*((ushort *)(csi->ip));
			return(0);
		case CS_MSWAIT:
			mswait(*(ushort *)csi->ip);
			csi->ip+=2;
			return(0);
		case CS_TOGGLE_NODE_MISC:
			getnodedat(node_num,&thisnode,1);
			thisnode.misc^=*(ushort *)csi->ip;
			csi->ip+=2;
			putnodedat(node_num,thisnode);
            return(0);
		case CS_COMPARE_NODE_MISC:
			getnodedat(node_num,&thisnode,0);
			if((thisnode.misc&*(ushort *)csi->ip)==*(ushort *)csi->ip)
				csi->logic=LOGIC_TRUE;
			else
				csi->logic=LOGIC_FALSE;
			csi->ip+=2;
            return(0);
		case CS_ADJUST_USER_CREDITS:
			i=*(short *)csi->ip;
			l=i*1024L;
			if(l<0)
				subtract_cdt(-l);
			else
				useron.cdt=adjustuserrec(useron.number,U_CDT,10,l);
			csi->ip+=2;
			return(0);
		case CS_ADJUST_USER_MINUTES:
			i=*(short *)csi->ip;
			useron.min=adjustuserrec(useron.number,U_MIN,10,i);
			csi->ip+=2;
            return(0);
		case CS_GETNUM:
			i=*(short *)csi->ip;
			csi->ip+=2;
			l=getnum(i);
			if(l<=0) {
				csi->str[0]=0;
				csi->logic=LOGIC_FALSE; }
			else {
				sprintf(csi->str,"%lu",l);
				csi->logic=LOGIC_TRUE; }
			return(0);

		case CS_TOGGLE_USER_FLAG:
			i=*(csi->ip++);
			ch=*(csi->ip++);
			switch(i) {
				case '1':
					useron.flags1^=FLAG(ch);
					putuserrec(useron.number,U_FLAGS1,8
						,ultoa(useron.flags1,tmp,16));
					break;
				case '2':
					useron.flags2^=FLAG(ch);
					putuserrec(useron.number,U_FLAGS2,8
						,ultoa(useron.flags2,tmp,16));
					break;
				case '3':
					useron.flags3^=FLAG(ch);
					putuserrec(useron.number,U_FLAGS3,8
						,ultoa(useron.flags3,tmp,16));
					break;
				case '4':
					useron.flags4^=FLAG(ch);
					putuserrec(useron.number,U_FLAGS4,8
						,ultoa(useron.flags4,tmp,16));
					break;
				case 'R':
					useron.rest^=FLAG(ch);
					putuserrec(useron.number,U_REST,8
						,ultoa(useron.rest,tmp,16));
					break;
				case 'E':
					useron.exempt^=FLAG(ch);
					putuserrec(useron.number,U_EXEMPT,8
						,ultoa(useron.exempt,tmp,16));
					break;
				default:
					errormsg(WHERE,ERR_CHK,"user flag type",*(csi->ip-2));
					return(0); }
			statusline();
			return(0);
		case CS_REVERT_TEXT:
            i=*(ushort *)csi->ip;
            csi->ip+=2;
			if((ushort)i==0xffff) {
				for(i=0;i<TOTAL_TEXT;i++) {
					if(text[i]!=text_sav[i] && text[i]!=nulstr)
						FREE(text[i]);
					text[i]=text_sav[i]; }
				return(0); }
            i--;
            if(i>=TOTAL_TEXT) {
				errormsg(WHERE,ERR_CHK,"revert text #",i);
                return(0); }
			if(text[i]!=text_sav[i] && text[i]!=nulstr)
                FREE(text[i]);
            text[i]=text_sav[i];
			return(0);
		default:
			errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
			return(0); } }

if(*csi->ip>=CS_TWO_BYTE) {
	switch(*(csi->ip++)) {
		case CS_TWO_MORE_BYTES:
			switch(*(csi->ip++)) {
				case CS_USER_EVENT:
					user_event(*(csi->ip++));
					return(0);
				}
			errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
			return(0);
		case CS_SETLOGIC:
			csi->logic=*csi->ip++;
			return(0);
		case CS_CMDKEY:
			if( ((*csi->ip)==CS_DIGIT && isdigit(csi->cmd))
				|| ((*csi->ip)==CS_EDIGIT && csi->cmd&0x80
				&& isdigit(csi->cmd&0x7f))) {
				csi->ip++;
				return(0); }
			if(csi->cmd!=*csi->ip) {
				csi->ip++;
				skipto(csi,CS_END_CMD); }	 /* skip code */
			csi->ip++;						/* skip key */
			return(0);
		case CS_NODE_ACTION:
			action=*csi->ip++;
			return(0);
		case CS_NODE_STATUS:
			getnodedat(node_num,&thisnode,1);
			thisnode.status=*csi->ip++;
            putnodedat(node_num,thisnode);
			return(0);
		case CS_GETSTR:
			csi->logic=LOGIC_TRUE;
			getstr(csi->str,*csi->ip++,0);
			if(sys_status&SS_ABORT) {
				csi->str[0]=0;
				csi->logic=LOGIC_FALSE; }
			if(csi->str[0]=='/' && csi->str[1])
				csi->cmd=csi->str[1]|0x80;
			else
				csi->cmd=csi->str[0];
            return(0);
		case CS_GETLINE:
			getstr(csi->str,*csi->ip++,K_LINE);
			if(sys_status&SS_ABORT)
				csi->str[0]=0;
			if(csi->str[0]=='/' && csi->str[1])
				csi->cmd=csi->str[1]|0x80;
			else
				csi->cmd=csi->str[0];
            return(0);
		case CS_GETSTRUPR:
			getstr(csi->str,*csi->ip++,K_UPPER);
			if(sys_status&SS_ABORT)
                csi->str[0]=0;
			if(csi->str[0]=='/' && csi->str[1])
				csi->cmd=csi->str[1]|0x80;
			else
                csi->cmd=csi->str[0];
			return(0);
		case CS_GETNAME:
			getstr(csi->str,*csi->ip++,K_UPRLWR);
			if(sys_status&SS_ABORT)
                csi->str[0]=0;
			return(0);
		case CS_SHIFT_STR:
			if(strlen(csi->str)>=*csi->ip)
				memmove(csi->str,csi->str+(*csi->ip)
					,strlen(csi->str)+1);
			csi->ip++;
			return(0);
		case CS_COMPARE_KEY:
			if( ((*csi->ip)==CS_DIGIT && isdigit(csi->cmd))
				|| ((*csi->ip)==CS_EDIGIT && csi->cmd&0x80
				&& isdigit(csi->cmd&0x7f))) {
				csi->ip++;
				csi->logic=LOGIC_TRUE; }
			else {
				if(csi->cmd==*(csi->ip++))
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE; }
			return(0);
		case CS_SET_USER_LEVEL:
			useron.level=*(csi->ip++);
            putuserrec(useron.number,U_LEVEL,2,itoa(useron.level,tmp,10));
			statusline();
			return(0);
		case CS_SET_USER_STRING:
			csi->logic=LOGIC_FALSE;
			if(!csi->str[0]) {
				csi->ip++;
				return(0); }
			switch(*(csi->ip++)) {
				case USER_STRING_ALIAS:
					if(!isalpha(csi->str[0]) || trashcan(csi->str,"NAME"))
						break;
					i=matchuser(csi->str);
					if(i && i!=useron.number)
						break;
					sprintf(useron.alias,"%.*s",LEN_ALIAS,csi->str);
					putuserrec(useron.number,U_ALIAS,LEN_ALIAS,useron.alias);
					putusername(useron.number,useron.alias);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_REALNAME:
					if(trashcan(csi->str,"NAME"))
						break;
					if(uq&UQ_DUPREAL
						&& userdatdupe(useron.number,U_NAME,LEN_NAME
						,csi->str,0))
						break;
					sprintf(useron.name,"%.*s",LEN_NAME,csi->str);
					putuserrec(useron.number,U_NAME,LEN_NAME
						,useron.name);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_HANDLE:
					if(trashcan(csi->str,"NAME"))
						break;
					if(uq&UQ_DUPHAND
						&& userdatdupe(useron.number,U_HANDLE,LEN_HANDLE
						,csi->str,0))
						break;
					sprintf(useron.handle,"%.*s",LEN_HANDLE,csi->str);
					putuserrec(useron.number,U_HANDLE,LEN_HANDLE
						,useron.handle);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_COMPUTER:
					sprintf(useron.comp,"%.*s",LEN_COMP,csi->str);
					putuserrec(useron.number,U_COMP,LEN_COMP
						,useron.comp);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_NOTE:
					sprintf(useron.note,"%.*s",LEN_NOTE,csi->str);
					putuserrec(useron.number,U_NOTE,LEN_NOTE
						,useron.note);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_ADDRESS:
					sprintf(useron.address,"%.*s",LEN_ADDRESS,csi->str);
					putuserrec(useron.number,U_ADDRESS,LEN_ADDRESS
						,useron.address);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_LOCATION:
					sprintf(useron.location,"%.*s",LEN_LOCATION,csi->str);
					putuserrec(useron.number,U_LOCATION,LEN_LOCATION
						,useron.location);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_ZIPCODE:
					sprintf(useron.zipcode,"%.*s",LEN_ZIPCODE,csi->str);
					putuserrec(useron.number,U_ZIPCODE,LEN_ZIPCODE
						,useron.zipcode);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_PASSWORD:
					sprintf(useron.pass,"%.*s",LEN_PASS,csi->str);
					putuserrec(useron.number,U_PASS,LEN_PASS
						,useron.pass);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_BIRTHDAY:
					if(!getage(csi->str))
						break;
					sprintf(useron.birth,"%.*s",LEN_BIRTH,csi->str);
					putuserrec(useron.number,U_BIRTH,LEN_BIRTH
						,useron.birth);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_PHONE:
					if(trashcan(csi->str,"PHONE"))
						break;
					sprintf(useron.phone,"%.*s",LEN_PHONE,csi->str);
					putuserrec(useron.number,U_PHONE,LEN_PHONE
						,useron.phone);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_MODEM:
					sprintf(useron.modem,"%.*s",LEN_MODEM,csi->str);
					putuserrec(useron.number,U_MODEM,LEN_MODEM
						,useron.phone);
					csi->logic=LOGIC_TRUE;
					break;
				case USER_STRING_COMMENT:
					sprintf(useron.comment,"%.*s",LEN_COMMENT,csi->str);
					putuserrec(useron.number,U_COMMENT,LEN_COMMENT
						,useron.comment);
					csi->logic=LOGIC_TRUE;
					break;
				default:
					errormsg(WHERE,ERR_CHK,"user string type",*(csi->ip-1));
					return(0); }
			statusline();
			return(0);
		default:
			errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
			return(0); } }


/*********************************/
/* Single Byte Instrcutions ONLY */
/*********************************/

switch(*(csi->ip++)) {
	case CS_ONE_MORE_BYTE:				 /* Just one MORE byte */
		switch(*(csi->ip++)) {
			case CS_OFFLINE:
				csi->misc|=CS_OFFLINE_EXEC;
				return(0);
			case CS_ONLINE:
				csi->misc&=~CS_OFFLINE_EXEC;
				return(0);
			case CS_NEWUSER:
				newuser();
				return(0);
			case CS_LOGON:
				if(logon())
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case CS_LOGOUT:
				logout();
				return(0);
			case CS_EXIT:
				return(1);
			default:
				errormsg(WHERE,ERR_CHK,"one byte extended function"
					,*(csi->ip-1));
				return(0); }
	case CS_CRLF:
		CRLF;
		return(0);
	case CS_CLS:
		CLS;
		return(0);
	case CS_PAUSE:
		pause();
		return(0);
	case CS_PAUSE_RESET:
		lncntr=0;
		return(0);
	case CS_GETLINES:
		getlines();
		return(0);
	case CS_HANGUP:
		hangup();
		return(0);
	case CS_LOGKEY:
		logch(csi->cmd,0);
		return(0);
	case CS_LOGKEY_COMMA:
		logch(csi->cmd,1);
		return(0);
	case CS_LOGSTR:
		log(csi->str);
		return(0);
	case CS_CHKSYSPASS:
		csi->logic=!chksyspass(0);
		return(0);
	case CS_PUT_NODE:
		getnodedat(node_num,&thisnode,1);
		putnodedat(node_num,thisnode);
		return(0);
	case CS_SYNC:
		SYNC;
		return(0);
	case CS_ASYNC:
		ASYNC;
		return(0);
	case CS_RIOSYNC:
		RIOSYNC(0);
		return(0);
	case CS_GETTIMELEFT:
		gettimeleft();
		return(0);
	case CS_RETURN:
		if(!csi->rets)
			return(1);
		csi->ip=csi->ret[--csi->rets];
		return(0);
	case CS_GETKEY:
		csi->cmd=getkey(K_UPPER);
		return(0);
	case CS_INKEY:
		csi->cmd=toupper(inkey(K_GETSTR));
		if(csi->cmd)
			csi->logic=LOGIC_TRUE;
		else
			csi->logic=LOGIC_FALSE;
        return(0);
	case CS_GETKEYE:
		csi->cmd=getkey(K_UPPER);
		if(csi->cmd=='/') {
			outchar('/');
			csi->cmd=getkey(K_UPPER);
			csi->cmd|=0x80; }
		return(0);
	case CS_GETFILESPEC:
		if(getfilespec(csi->str))
			csi->logic=LOGIC_TRUE;
		else
			csi->logic=LOGIC_FALSE;
		return(0);
	case CS_SAVELINE:
		SAVELINE;
		return(0);
	case CS_RESTORELINE:
		RESTORELINE;
		return(0);
	case CS_SELECT_SHELL:
		csi->logic=LOGIC_TRUE;
		for(i=0;i<total_shells;i++)
			uselect(1,i,"Command Shell",shell[i]->name,shell[i]->ar);
		if((i=uselect(0,useron.shell,0,0,0))>=0) {
			useron.shell=i;
			putuserrec(useron.number,U_SHELL,8,shell[i]->code); }
		else
			csi->logic=LOGIC_FALSE;
		return(0);
	case CS_SET_SHELL:
		csi->logic=LOGIC_TRUE;
		for(i=0;i<total_shells;i++)
			if(!stricmp(csi->str,shell[i]->code)
				&& chk_ar(shell[i]->ar,useron))
				break;
		if(i<total_shells) {
			useron.shell=i;
			putuserrec(useron.number,U_SHELL,8,shell[i]->code); }
		else
			csi->logic=LOGIC_FALSE;
		return(0);

	case CS_SELECT_EDITOR:
		csi->logic=LOGIC_TRUE;
		for(i=0;i<total_xedits;i++)
			uselect(1,i,"External Editor",xedit[i]->name,xedit[i]->ar);
		if(useron.xedit) useron.xedit--;
		if((i=uselect(0,useron.xedit,0,0,0))>=0) {
			useron.xedit=i+1;
			putuserrec(useron.number,U_XEDIT,8,xedit[i]->code); }
		else
			csi->logic=LOGIC_FALSE;
		return(0);
	case CS_SET_EDITOR:
		csi->logic=LOGIC_TRUE;
		for(i=0;i<total_xedits;i++)
			if(!stricmp(csi->str,xedit[i]->code)
				&& chk_ar(xedit[i]->ar,useron))
				break;
		if(i<total_xedits) {
			useron.xedit=i+1;
			putuserrec(useron.number,U_XEDIT,8,xedit[i]->code); }
		else
			csi->logic=LOGIC_FALSE;
        return(0);

	case CS_CLEAR_ABORT:
		sys_status&=~SS_ABORT;
		return(0);
	case CS_FINDUSER:
		i=finduser(csi->str);
		if(i) {
			csi->logic=LOGIC_TRUE;
			username(i,csi->str); }
		else
			csi->logic=LOGIC_FALSE;
		return(0);
	case CS_UNGETKEY:
		ungetkey(csi->cmd&0x7f);
		return(0);
	case CS_UNGETSTR:
		j=strlen(csi->str);
		for(i=0;i<j;i++)
			ungetkey(csi->str[i]);
		return(0);
	case CS_PRINTKEY:
		if((csi->cmd&0x7f)>=SP)
			outchar(csi->cmd&0x7f);
		return(0);
	case CS_PRINTSTR:
		putmsg(csi->str,P_SAVEATR|P_NOABORT);
		return(0);
	case CS_CMD_HOME:
		csi->cmdret[csi->cmdrets++]=(csi->ip-1);
		return(0);
	case CS_END_CMD:
		if(csi->cmdrets)
			csi->ip=csi->cmdret[--csi->cmdrets];
/*													Removed 06/07/95
		else
			errormsg(WHERE,ERR_CHK,"misplaced end_cmd",(csi->ip-csi->cs)-1);
*/
		return(0);
	case CS_CMD_POP:
		if(csi->cmdrets)
			csi->cmdrets--;
		return(0);
	case CS_IF_TRUE:
		if(csi->logic!=LOGIC_TRUE) {
			skipto(csi,CS_ELSEORENDIF);
			csi->ip++; }
		return(0);
	case CS_IF_GREATER:
		if(csi->logic!=LOGIC_GREATER) {
			skipto(csi,CS_ELSEORENDIF);
			csi->ip++; }
		return(0);
	case CS_IF_GREATER_OR_EQUAL:
		if(csi->logic!=LOGIC_GREATER && csi->logic!=LOGIC_EQUAL) {
			skipto(csi,CS_ELSEORENDIF);
			csi->ip++; }
		return(0);
	case CS_IF_LESS:
		if(csi->logic!=LOGIC_LESS) {
			skipto(csi,CS_ELSEORENDIF);
			csi->ip++; }
		return(0);
	case CS_IF_LESS_OR_EQUAL:
		if(csi->logic!=LOGIC_LESS && csi->logic!=LOGIC_EQUAL) {
			skipto(csi,CS_ELSEORENDIF);
			csi->ip++; }
        return(0);
	case CS_IF_FALSE:
		if(csi->logic==LOGIC_TRUE) {
			skipto(csi,CS_ELSEORENDIF);
			csi->ip++; }
		return(0);
	case CS_ELSE:
		skipto(csi,CS_ENDIF);
		csi->ip++;
		return(0);
	case CS_END_CASE:
		skipto(csi,CS_END_SWITCH);
		csi->misc&=~CS_IN_SWITCH;
		csi->ip++;
		return(0);
	case CS_DEFAULT:
	case CS_END_SWITCH:
		csi->misc&=~CS_IN_SWITCH;
		return(0);
	case CS_ENDIF:
		return(0);
	default:
		errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
		return(0); }
}

