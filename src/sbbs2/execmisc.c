#line 1 "EXECMISC.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "cmdshell.h"
#include <sys/locking.h>
#include <dirent.h>


int exec_misc(csi_t *csi, uchar *path)
{
	uchar	str[256],tmp2[128],buf[1025],ch,*p,**pp,**pp1,**pp2;
	ushort	w;
	int 	i,j,k,s,file;
	long	l,*lp,*lp1,*lp2;
	void	*vp;
	va_list arglist[64];
	struct	dirent *de;
    struct  tm *tm_p;
	struct	ftime ft;
	FILE	*fp;

switch(*(csi->ip++)) {
	case CS_VAR_INSTRUCTION:
		switch(*(csi->ip++)) {	/* sub-op-code stored as next byte */
			case PRINT_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				if(!pp || !*pp) {
					lp=getintvar(csi,*(long *)csi->ip);
					if(lp)
						bprintf("%ld",*lp); }
				else
					putmsg(cmdstr(*pp,path,csi->str,buf)
						,P_SAVEATR|P_NOABORT);
				csi->ip+=4;
				return(0);
			case VAR_PRINTF:
				strcpy(str,csi->ip);
				while(*(csi->ip++));	/* Find NULL */
				j=*(csi->ip++); 		/* total args */
				for(i=0;i<j;i++) {
					vp=getstrvar(csi,*(long *)csi->ip);
					if(!vp) {
						lp=getintvar(csi,*(long *)csi->ip);
						if(!lp)
							arglist[i]=0;
						else
							arglist[i]=(void *)*lp; }
					else
						arglist[i]=*(char **)vp;
					csi->ip+=4; }
				vsprintf(tmp,str,arglist);
				putmsg(cmdstr(tmp,path,csi->str,buf),P_SAVEATR|P_NOABORT);
				return(0);
			case SHOW_VARS:
				bprintf("shell     str=(%08lX) %s\r\n"
					,csi->str,csi->str);
				for(i=0;i<csi->str_vars;i++)
					bprintf("local  str[%d]=(%08lX) (%08lX) %s\r\n"
						,i,csi->str_var_name[i]
						,csi->str_var[i]
						,csi->str_var[i]);
				for(i=0;i<csi->int_vars;i++)
					bprintf("local  int[%d]=(%08lX) (%08lX) %ld\r\n"
						,i,csi->int_var_name[i]
						,csi->int_var[i]
						,csi->int_var[i]);
				for(i=0;i<global_str_vars;i++)
					bprintf("global str[%d]=(%08lX) (%08lX) %s\r\n"
						,i,global_str_var_name[i]
						,global_str_var[i]
						,global_str_var[i]);
				for(i=0;i<global_int_vars;i++)
					bprintf("global int[%d]=(%08lX) (%08lX) %ld\r\n"
						,i,global_int_var_name[i]
						,global_int_var[i]
						,global_int_var[i]);
				return(0);
			case DEFINE_STR_VAR:
				if(getstrvar(csi,*(long *)csi->ip)) {
					csi->ip+=4;
					return(0); }
				csi->str_vars++;
				csi->str_var=REALLOC(csi->str_var
					,sizeof(char *)*csi->str_vars);
				csi->str_var_name=REALLOC(csi->str_var_name
					,sizeof(long)*csi->str_vars);
				if(csi->str_var==NULL
					|| csi->str_var_name==NULL) { /* REALLOC failed */
					errormsg(WHERE,ERR_ALLOC,"local str var"
						,sizeof(char *)*csi->str_vars);
					if(csi->str_var_name) {
						FREE(csi->str_var_name);
						csi->str_var_name=0; }
					if(csi->str_var) {
						FREE(csi->str_var);
						csi->str_var=0; }
					csi->str_vars=0; }
				else {
					csi->str_var_name[csi->str_vars-1]=*(long *)csi->ip;
					csi->str_var[csi->str_vars-1]=0; }
				csi->ip+=4; /* Skip variable name */
				return(0);
			case DEFINE_INT_VAR:
				if(getintvar(csi,*(long *)csi->ip)) {
					csi->ip+=4;
					return(0); }
				csi->int_vars++;
				csi->int_var=REALLOC(csi->int_var
					,sizeof(char *)*csi->int_vars);
				csi->int_var_name=REALLOC(csi->int_var_name
					,sizeof(long)*csi->int_vars);
				if(csi->int_var==NULL
					|| csi->int_var_name==NULL) { /* REALLOC failed */
					errormsg(WHERE,ERR_ALLOC,"local int var"
						,sizeof(char *)*csi->int_vars);
					if(csi->int_var_name) {
						FREE(csi->int_var_name);
						csi->int_var_name=0; }
					if(csi->int_var) {
						FREE(csi->int_var);
						csi->int_var=0; }
					csi->int_vars=0; }
				else {
					csi->int_var_name[csi->int_vars-1]=*(long *)csi->ip;
					csi->int_var[csi->int_vars-1]=0; }
				csi->ip+=4; /* Skip variable name */
				return(0);
			case DEFINE_GLOBAL_STR_VAR:
				if(getstrvar(csi,*(long *)csi->ip)) {
					csi->ip+=4;
					return(0); }
				global_str_vars++;
				global_str_var=REALLOC(global_str_var
					,sizeof(char *)*global_str_vars);
				global_str_var_name=REALLOC(global_str_var_name
					,sizeof(long)*global_str_vars);
				if(global_str_var==NULL
					|| global_str_var_name==NULL) { /* REALLOC failed */
					errormsg(WHERE,ERR_ALLOC,"global str var"
						,sizeof(char *)*global_str_vars);
					if(global_str_var_name) {
						FREE(global_str_var_name);
						global_str_var_name=0; }
					if(global_str_var) {
						FREE(global_str_var);
						global_str_var=0; }
					global_str_vars=0; }
				else {
					global_str_var_name[global_str_vars-1]=
						*(long *)csi->ip;
					global_str_var[global_str_vars-1]=0; }
				csi->ip+=4; /* Skip variable name */
				return(0);
			case DEFINE_GLOBAL_INT_VAR:
				if(getintvar(csi,*(long *)csi->ip)) {
					csi->ip+=4;
					return(0); }
				global_int_vars++;
				global_int_var=REALLOC(global_int_var
					,sizeof(char *)*global_int_vars);
				global_int_var_name=REALLOC(global_int_var_name
					,sizeof(long)*global_int_vars);
				if(global_int_var==NULL
					|| global_int_var_name==NULL) { /* REALLOC failed */
					errormsg(WHERE,ERR_ALLOC,"local int var"
						,sizeof(char *)*global_int_vars);
					if(global_int_var_name) {
						FREE(global_int_var_name);
						global_int_var_name=0; }
					if(global_int_var) {
						FREE(global_int_var);
						global_int_var=0; }
					global_int_vars=0; }
				else {
					global_int_var_name[global_int_vars-1]
						=*(long *)csi->ip;
					global_int_var[global_int_vars-1]=0; }
				csi->ip+=4; /* Skip variable name */
				return(0);

			case SET_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				if(pp)
					*pp=copystrvar(csi,*pp
						,cmdstr(csi->ip,path,csi->str,buf));
				while(*(csi->ip++));	 /* Find NULL */
				return(0);
			case SET_INT_VAR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				if(lp)
					*lp=*(long *)csi->ip;
				csi->ip+=4; /* Skip value */
				return(0);
			case COMPARE_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				if(pp)
					csi->logic=stricmp(*pp
						,cmdstr(csi->ip,path,csi->str,buf));
				else {	/* Uninitialized str var */
					if(*(csi->ip)==0)	 /* Blank static str */
						csi->logic=LOGIC_TRUE;
					else
						csi->logic=LOGIC_FALSE; }
				while(*(csi->ip++));	 /* Find NULL */
				return(0);
			case STRSTR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				if(pp && *pp && strstr(*pp
					,cmdstr(csi->ip,path,csi->str,buf)))
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE;
				while(*(csi->ip++));	 /* Find NULL */
				return(0);
			case STRNCMP_VAR:
				i=*csi->ip++;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				if(pp && *pp)
					csi->logic=strnicmp(*pp
						,cmdstr(csi->ip,path,csi->str,buf),i);
				else
					csi->logic=LOGIC_FALSE;
				while(*(csi->ip++));	 /* Find NULL */
				return(0);
			case STRNCMP_VARS:
				i=*csi->ip++;
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp1 && *pp1 && pp2 && *pp2)
					csi->logic=strnicmp(*pp1,*pp2,i);
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case STRSTR_VARS:
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp1 && *pp1 && pp2 && *pp2 && strstr(*pp1,*pp2))
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case COMPARE_INT_VAR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				l=*(long *)csi->ip;
				csi->ip+=4; /* Skip static value */
				if(!lp) {	/* Unknown variable */
					csi->logic=LOGIC_FALSE;
					return(0); }
				if(*lp>l)
					csi->logic=LOGIC_GREATER;
				else if(*lp<l)
					csi->logic=LOGIC_LESS;
				else
					csi->logic=LOGIC_EQUAL;
				return(0);
			case COMPARE_VARS:
				lp1=lp2=0;
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				if(!pp1)
					lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				if(!pp2)
					lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */

				if(((!pp1 || !*pp1) && !lp1)
					|| ((!pp2 || !*pp2) && !lp2)) {
					if(pp1 && pp2)		/* Both unitialized or blank */
						csi->logic=LOGIC_TRUE;
					else
						csi->logic=LOGIC_FALSE;
					return(0); }

				if(pp1) { /* ASCII */
					if(!pp2) {
						ltoa(*lp2,tmp,10);
						csi->logic=stricmp(*pp1,tmp); }
					else
						csi->logic=stricmp(*pp1,*pp2);
					return(0); }

				/* Binary */
				if(!lp2) {
					l=strtol(*pp2,0,0);
					if(*lp1>l)
						csi->logic=LOGIC_GREATER;
					else if(*lp1<l)
						csi->logic=LOGIC_LESS;
					else
						csi->logic=LOGIC_EQUAL;
					return(0); }
				if(*lp1>*lp2)
					csi->logic=LOGIC_GREATER;
				else if(*lp1<*lp2)
					csi->logic=LOGIC_LESS;
				else
					csi->logic=LOGIC_EQUAL;
				return(0);
			case COPY_VAR:
				lp1=lp2=0;
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				if(!pp1)
					lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				if(!pp2)
					lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */

				if((!pp1 && !lp1)
					|| ((!pp2 || !*pp2) && !lp2)) {
					csi->logic=LOGIC_FALSE;
					return(0); }
				csi->logic=LOGIC_TRUE;

				if(pp1) {	/* ASCII */
					if(!pp2)
						ltoa(*lp2,tmp,10);
					else
						strcpy(tmp,*pp2);
					*pp1=copystrvar(csi,*pp1,tmp);
					return(0); }
				if(!lp2)
					*lp1=strtol(*pp2,0,0);
				else
					*lp1=*lp2;
				return(0);
			case SWAP_VARS:
				lp1=lp2=0;
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				if(!pp1)
					lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				if(!pp2)
					lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */

				if(((!pp1 || !*pp1) && !lp1)
					|| ((!pp2 || !*pp2) && !lp2)) {
					csi->logic=LOGIC_FALSE;
					return(0); }

				csi->logic=LOGIC_TRUE;

				if(pp1) {	/* ASCII */
					if(!pp2) {
						if(!strnicmp(*pp2,"0x",2)) {
							l=strtol((*pp1)+2,0,16);
							ltoa(*lp2,tmp,16); }
						else {
							l=atol(*pp1);
							ltoa(*lp2,tmp,10); }
						*pp1=copystrvar(csi,*pp1,tmp);
						*lp2=l; }
					else {
						p=*pp1;
						*pp1=*pp2;
						*pp2=p; }
					return(0); }

				/* Binary */
				if(!lp2) {
					if(!strnicmp(*pp2,"0x",2)) {
						l=strtol((*pp2)+2,0,16);
						ltoa(*lp1,tmp,16); }
					else {
						l=atol(*pp2);
						ltoa(*lp1,tmp,10); }
					*pp2=copystrvar(csi,*pp2,tmp);
					*lp1=l; }
				else {
					l=*lp1;
					*lp1=*lp2;
					*lp2=l; }
				return(0);
			case CAT_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				strcpy(tmp,csi->ip);
				while(*(csi->ip++));
				if(pp && *pp)
					for(i=0;i<MAX_SYSVARS;i++)
						if(*pp==sysvar_p[i])
							break;
				if(pp && *pp!=csi->str && i==MAX_SYSVARS) {
					if(*pp)
						*pp=REALLOC(*pp,strlen(*pp)+strlen(tmp)+1);
					else
						*pp=REALLOC(*pp,strlen(tmp)+1); }
				if(pp && *pp)
					strcat(*pp,tmp);
				return(0);
			case CAT_STR_VARS:
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip dest variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip source variable name */
				if(!pp1 || !pp2 || !*pp2) {
					csi->logic=LOGIC_FALSE;
					return(0); }
				csi->logic=LOGIC_TRUE;
				if(*pp1)
					for(i=0;i<MAX_SYSVARS;i++)
						if(*pp1==sysvar_p[i])
							break;
				if(*pp1!=csi->str && (!*pp1 || i==MAX_SYSVARS)) {
					if(*pp1)
						*pp1=REALLOC(*pp1,strlen(*pp1)+strlen(*pp2)+1);
					else
						*pp1=REALLOC(*pp1,strlen(*pp2)+1); }
				strcat(*pp1,*pp2);
				return(0);
			case FORMAT_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				strcpy(str,csi->ip);
				while(*(csi->ip++));	/* Find NULL */
				j=*(csi->ip++); 		/* total args */
				for(i=0;i<j;i++) {
					vp=getstrvar(csi,*(long *)csi->ip);
					if(!vp) {
						lp=getintvar(csi,*(long *)csi->ip);
						if(!lp)
							arglist[i]=0;
						else
							arglist[i]=(void *)*lp; }
					else
						arglist[i]=*(char **)vp;
					csi->ip+=4; }
				vsprintf(tmp,str,arglist);
				cmdstr(tmp,path,csi->str,str);
				if(pp)
					*pp=copystrvar(csi,*pp,str);
				return(0);
			case FORMAT_TIME_STR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				strcpy(str,csi->ip);
				while(*(csi->ip++));	/* Find NULL */
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && lp) {
					tm_p=gmtime(lp);
					strftime(buf,128,str,tm_p);
					*pp=copystrvar(csi,*pp,buf); }
				return(0);
			case TIME_STR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip str variable name */
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip int variable name */
				if(pp && lp) {
					strcpy(str,timestr(lp));
					*pp=copystrvar(csi,*pp,str); }
				return(0);
			case DATE_STR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip str variable name */
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip int variable name */
				if(pp && lp) {
					unixtodstr(*lp,str);
					*pp=copystrvar(csi,*pp,str); }
				return(0);
			case SECOND_STR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip str variable name */
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip int variable name */
				if(pp && lp) {
					sectostr(*lp,str);
					*pp=copystrvar(csi,*pp,str); }
				return(0);
			case STRUPR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp)
					strupr(*pp);
				return(0);
			case STRLWR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp)
					strlwr(*pp);
				return(0);
			case TRUNCSP_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp)
					truncsp(*pp);
				return(0);
			case STRIP_CTRL_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp)
					strip_ctrl(*pp);
				return(0);

			case ADD_INT_VAR:
			case SUB_INT_VAR:
			case MUL_INT_VAR:
			case DIV_INT_VAR:
			case MOD_INT_VAR:
			case AND_INT_VAR:
			case OR_INT_VAR:
			case NOT_INT_VAR:
			case XOR_INT_VAR:
				i=*(csi->ip-1);
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				l=*(long *)csi->ip;
				csi->ip+=4;
				if(!lp)
					return(0);
				switch(i) {
					case ADD_INT_VAR:
						*lp+=l;
						break;
					case SUB_INT_VAR:
						*lp-=l;
						break;
					case MUL_INT_VAR:
						*lp*=l;
						break;
					case DIV_INT_VAR:
						*lp/=l;
						break;
					case MOD_INT_VAR:
						*lp%=l;
						break;
					case AND_INT_VAR:
						*lp&=l;
						break;
					case OR_INT_VAR:
						*lp|=l;
						break;
					case NOT_INT_VAR:
						*lp&=~l;
						break;
					case XOR_INT_VAR:
						*lp^=l;
						break; }
				return(0);
			case ADD_INT_VARS:
			case SUB_INT_VARS:
			case MUL_INT_VARS:
			case DIV_INT_VARS:
			case MOD_INT_VARS:
			case AND_INT_VARS:
			case OR_INT_VARS:
			case NOT_INT_VARS:
			case XOR_INT_VARS:
				i=*(csi->ip-1);
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(!lp1)
					return(0);
				if(!lp2) {
					(char **)pp=getstrvar(csi,*(long *)csi->ip);
					if(!pp || !*pp)
						return(0);
					l=strtol(*pp,0,0); }
				else
					l=*lp2;
				switch(i) {
					case ADD_INT_VARS:
						*lp1+=l;
						break;
					case SUB_INT_VARS:
						*lp1-=l;
						break;
					case MUL_INT_VARS:
						*lp1*=l;
						break;
					case DIV_INT_VARS:
						*lp1/=l;
						break;
					case MOD_INT_VARS:
						*lp1%=l;
						break;
					case AND_INT_VARS:
						*lp1&=l;
						break;
					case OR_INT_VARS:
						*lp1|=l;
						break;
					case NOT_INT_VARS:
						*lp1&=~l;
						break;
					case XOR_INT_VARS:
						*lp1^=l;
						break; }
				return(0);
			case RANDOM_INT_VAR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				l=*(long *)csi->ip;
				csi->ip+=4;
				if(lp)
					*lp=random(l);
				return(0);
			case TIME_INT_VAR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp)
					*lp=time(NULL);
				return(0);
			case DATE_STR_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp && pp && *pp)
					*lp=dstrtounix(*pp);
				return(0);
			case STRLEN_INT_VAR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					if(pp && *pp)
						*lp=strlen(*pp);
					else
						*lp=0; }
				return(0);
			case CRC16_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					if(pp && *pp)
						*lp=crc16(*pp);
					else
						*lp=0; }
				return(0);
			case CRC32_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					if(pp && *pp)
						*lp=crc32(*pp,strlen(*pp));
					else
						*lp=0; }
				return(0);
			case CHKSUM_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					*lp=0;
					if(pp && *pp) {
						i=0;
						while(*((*pp)+i))
							*lp+=(uchar)*((*pp)+(i++)); } }
				return(0);
			case FLENGTH_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					if(pp && *pp)
						*lp=flength(*pp);
					else
						*lp=0; }
				return(0);
			case FTIME_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					if(pp && *pp)
						*lp=fdate_dir(*pp);
					else
						*lp=0; }
				return(0);
			case CHARVAL_TO_INT:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					if(pp && *pp)
						*lp=**pp;
					else
						*lp=0; }
				return(0);
			case GETSTR_VAR:
			case GETLINE_VAR:
			case GETNAME_VAR:
			case GETSTRUPR_VAR:
			case GETSTR_MODE:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				i=*(csi->ip++);
				csi->logic=LOGIC_FALSE;
				switch(*(csi->ip-6)) {
					case GETNAME_VAR:
						getstr(buf,i,K_UPRLWR);
						break;
					case GETSTRUPR_VAR:
						getstr(buf,i,K_UPPER);
						break;
					case GETLINE_VAR:
						getstr(buf,i,K_LINE);
						break;
					case GETSTR_MODE:
						l=*(long *)csi->ip;
						csi->ip+=4;
						if(l&K_EDIT) {
							if(pp && *pp)
								strcpy(buf,*pp);
							else
								buf[0]=0; }
						getstr(buf,i,l);
						break;
					default:
						getstr(buf,i,0); }
				if(sys_status&SS_ABORT)
					return(0);
				if(pp) {
					*pp=copystrvar(csi,*pp,buf);
					csi->logic=LOGIC_TRUE; }
				return(0);
			case GETNUM_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				if(!pp)
					lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				i=*(short *)csi->ip;
				csi->ip+=2;
				csi->logic=LOGIC_FALSE;
				l=getnum(i);
				if(!pp && !lp)
					return(0);
				if(pp) {
					if(l<=0)
						str[0]=0;
					else
						ltoa(l,str,10);
					*pp=copystrvar(csi,*pp,str);
					csi->logic=LOGIC_TRUE;
					return(0); }
				if(lp) {
					*lp=l;
					csi->logic=LOGIC_TRUE; }
				return(0);

			case SHIFT_STR_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				i=*(csi->ip++);
				if(!pp || !*pp)
					return(0);
				if(strlen(*pp)>=i)
					memmove(*pp,*pp+i,strlen(*pp)+1);
				return(0);

			case CHKFILE_VAR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp && fexist(cmdstr(*pp,path,csi->str,buf)))
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case PRINTFILE_VAR_MODE:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				i=*(short *)(csi->ip);
				csi->ip+=2;
				if(pp && *pp)
					printfile(*pp,i);
				return(0);
			case PRINTTAIL_VAR_MODE:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				i=*(short *)(csi->ip);
				csi->ip+=2;
				j=*csi->ip;
				csi->ip++;
				if(pp && *pp)
					printtail(*pp,j,i);
				return(0);
			case SEND_FILE_VIA:
			case RECEIVE_FILE_VIA:
				j=*(csi->ip-1);
				ch=*(csi->ip++);	/* Protocol */
				cmdstr(csi->ip,csi->str,csi->str,str);
				while(*(csi->ip++));	/* Find NULL */
				for(i=0;i<total_prots;i++)
					if(prot[i]->mnemonic==ch && chk_ar(prot[i]->ar,useron))
						break;
				csi->logic=LOGIC_FALSE;
				if(i<total_prots)
					if(external(cmdstr(j==SEND_FILE_VIA
						? prot[i]->dlcmd : prot[i]->ulcmd,str,str,buf)
						,EX_OUTL)==0)
						csi->logic=LOGIC_TRUE;
				return(0);
			case SEND_FILE_VIA_VAR:
			case RECEIVE_FILE_VIA_VAR:
				j=*(csi->ip-1);
				ch=*(csi->ip++);	/* Protocol */
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				for(i=0;i<total_prots;i++)
					if(prot[i]->mnemonic==ch && chk_ar(prot[i]->ar,useron))
						break;
				csi->logic=LOGIC_FALSE;
				if(!pp || !(*pp))
					return(0);
				if(i<total_prots)
					if(external(cmdstr(j==SEND_FILE_VIA_VAR
						 ? prot[i]->dlcmd : prot[i]->ulcmd,*pp,*pp,buf)
						,EX_OUTL)==0)
						csi->logic=LOGIC_TRUE;
				return(0);

			default:
				errormsg(WHERE,ERR_CHK,"var sub-instruction",*(csi->ip-1));
				return(0); }

	case CS_FIO_FUNCTION:
		switch(*(csi->ip++)) {	/* sub-op-code stored as next byte */
			case FIO_OPEN:
			case FIO_OPEN_VAR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				w=*(ushort *)csi->ip;
				csi->ip+=2;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-7)==FIO_OPEN) {
					cmdstr(csi->ip,path,csi->str,str);
					while(*(csi->ip++)); }	 /* skip filename */
				else {
					(char **)pp=getstrvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!pp || !*pp)
						return(0);
					strcpy(str,*pp); }
				if(csi->files>=MAX_FOPENS)
					return(0);
				if(lp) {
					/* Access flags are not cross-platform, so convert */
					i=0;
					if(w&1) 	i|=O_RDONLY;
					if(w&2) 	i|=O_WRONLY;
					if(w&4) 	i|=O_RDWR;
					if(w&0x040) i|=O_DENYNONE;
					if(w&0x100) i|=O_CREAT;
					if(w&0x200) i|=O_TRUNC;
					if(w&0x400) i|=O_EXCL;
					if(w&0x800) i|=O_APPEND;
					*lp=(long)fnopen(&j,str,i);
					if(*lp) {
						for(i=0;i<csi->files;i++)
							if(!csi->file[i])
								break;
						csi->file[i]=(FILE *)*lp;
						if(i==csi->files)
							csi->files++;
						csi->logic=LOGIC_TRUE; } }
				return(0);
			case FIO_CLOSE:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp && *lp) {
					csi->logic=fclose((FILE *)*lp);
					for(i=0;i<csi->files;i++)
						if(csi->file[i]==(FILE *)*lp)
							csi->file[i]=0; }
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case FIO_FLUSH:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp && *lp)
					csi->logic=fflush((FILE *)*lp);
				else
					csi->logic=LOGIC_FALSE;
                return(0);
			case FIO_READ:
			case FIO_READ_VAR:
				lp1=getintvar(csi,*(long *)csi->ip);		/* Handle */
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				if(!pp)
					lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-9)==FIO_READ) {
					i=*(short *)csi->ip;
					csi->ip+=2; /* Length */ }
				else {			/* FIO_READ_VAR */
					vp=getintvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!vp)
						return(0);
					i=*(short *)vp; }
				if(i>1024)
					i=1024;
				if(!lp1 || !(*lp1) || (!pp && !lp2))
					return(0);
				if(pp) {
					if(i<1) {
						if(*pp && **pp)
							i=strlen(*pp);
						else
							i=128; }
					if((j=fread(buf,1,i,(FILE *)*lp1))==i)
						csi->logic=LOGIC_TRUE;
					buf[j]=0;
					if(csi->etx) {
						p=strchr(buf,csi->etx);
						if(p) *p=0; }
					*pp=copystrvar(csi,*pp,buf); }
				else {
					*lp2=0;
					if(i>4 || i<1) i=4;
					if(fread(lp2,1,i,(FILE *)*lp1)==i)
						csi->logic=LOGIC_TRUE; }
				return(0);
			case FIO_READ_LINE:
				lp1=getintvar(csi,*(long *)csi->ip);		/* Handle */
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				if(!pp)
					lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(!lp1 || !(*lp1) || feof((FILE *)*lp1) || (!pp && !lp2))
					return(0);
				csi->logic=LOGIC_TRUE;
				for(i=0;i<1024 /* && !eof(*lp1) removed 1/23/96 */;i++) {
					if(!fread(buf+i,1,1,(FILE *)*lp1))
						break;
					if(*(buf+i)==LF) {
						i++;
						break; } }
				buf[i]=0;
				if(csi->etx) {
					p=strchr(buf,csi->etx);
					if(p) *p=0; }
				if(pp)
					*pp=copystrvar(csi,*pp,buf);
				else
					*lp2=strtol(buf,0,0);
				return(0);
			case FIO_WRITE:
			case FIO_WRITE_VAR:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				if(!pp)
					lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-9)==FIO_WRITE) {
					i=*(short *)csi->ip;
					csi->ip+=2; /* Length */ }
				else {			/* FIO_WRITE_VAR */
					vp=getintvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!vp)
						return(0);
					i=*(short *)vp; }
				if(i>1024)
					i=1024;
				if(!lp1 || !(*lp1) || (!pp && !lp2) || (pp && !*pp))
					return(0);
				if(pp) {
					j=strlen(*pp);
					if(i<1) i=j;
					if(j>i) j=i;
					j=fwrite(*pp,1,j,(FILE *)*lp1);
					if(j<i) {
						memset(buf,csi->etx,i-j);
						fwrite(buf,1,i-j,(FILE *)*lp1); }
					csi->logic=LOGIC_TRUE; }
				else {
					if(i<1 || i>4) i=4;
					if(fwrite(lp2,1,i,(FILE *)*lp1)==i)
						csi->logic=LOGIC_TRUE; }
				return(0);
			case FIO_GET_LENGTH:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp1 && *lp1 && lp2)
					*lp2=filelength(fileno((FILE *)*lp1));
				return(0);
			case FIO_GET_TIME:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp1 && *lp1 && lp2) {
					getftime(fileno((FILE *)*lp1),&ft);
					*lp2=ftimetounix(ft); }
				return(0);
			case FIO_SET_TIME:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp1 && *lp1 && lp2) {
					ft=unixtoftime(*lp2);
					setftime(fileno((FILE *)*lp1),&ft); }
				return(0);
			case FIO_EOF:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(lp && *lp)
					if(ftell((FILE *)*lp)>=filelength(fileno((FILE *)*lp)))
						csi->logic=LOGIC_TRUE;
				return(0);
			case FIO_GET_POS:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				lp2=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp1 && *lp1 && lp2)
					*lp2=ftell((FILE *)*lp1);
				return(0);
			case FIO_SEEK:
			case FIO_SEEK_VAR:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-5)==FIO_SEEK) {
					l=*(long *)csi->ip;
					csi->ip+=4; }
				else {
					lp2=getintvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!lp2) {
						csi->ip+=2;
						return(0); }
					l=*lp2; }
				i=*(short *)csi->ip;
				csi->ip+=2;
				if(lp1 && *lp1)
					if(fseek((FILE *)*lp1,l,i)!=-1)
						csi->logic=LOGIC_TRUE;
				return(0);
			case FIO_LOCK:
			case FIO_LOCK_VAR:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-5)==FIO_LOCK) {
					l=*(long *)csi->ip;
					csi->ip+=4; }
				else {
					lp2=getintvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!lp2)
						return(0);
					l=*lp2; }
				if(lp1 && *lp1) {
					fflush((FILE *)*lp1);
					lseek(fileno((FILE *)*lp1),ftell((FILE *)*lp1),SEEK_SET);
					csi->logic=locking(fileno((FILE *)*lp1),LK_LOCK,l); }
				return(0);
			case FIO_UNLOCK:
			case FIO_UNLOCK_VAR:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-5)==FIO_UNLOCK) {
					l=*(long *)csi->ip;
					csi->ip+=4; }
				else {
					lp2=getintvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!lp2)
						return(0);
					l=*lp2; }
				if(lp1 && *lp1) {
					fflush((FILE *)*lp1);
					lseek(fileno((FILE *)*lp1),ftell((FILE *)*lp1),SEEK_SET);
					csi->logic=locking(fileno((FILE *)*lp1),LK_UNLCK,l); }
				return(0);
			case FIO_SET_LENGTH:
			case FIO_SET_LENGTH_VAR:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(*(csi->ip-5)==FIO_SET_LENGTH) {
					l=*(long *)csi->ip;
					csi->ip+=4; }
				else {
					lp2=getintvar(csi,*(long *)csi->ip);
					csi->ip+=4;
					if(!lp2)
						return(0);
					l=*lp2; }
				if(lp1 && *lp1)
					csi->logic=chsize(fileno((FILE *)*lp1),l);
				return(0);
			case FIO_PRINTF:
				lp1=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				strcpy(str,csi->ip);
				while(*(csi->ip++));	/* Find NULL */
				j=*(csi->ip++); 		/* total args */
				for(i=0;i<j;i++) {
					vp=getstrvar(csi,*(long *)csi->ip);
					if(!vp) {
						lp2=getintvar(csi,*(long *)csi->ip);
						if(!lp2)
							arglist[i]=0;
						else
							arglist[i]=(void *)*lp2; }
					else
						arglist[i]=*(char **)vp;
					csi->ip+=4; }
				vsprintf(tmp,str,arglist);
				if(lp1 && *lp1) {
					cmdstr(tmp,path,csi->str,str);
					fwrite(str,1,strlen(str),(FILE *)*lp1); }
				return(0);
			case FIO_SET_ETX:
				csi->etx=*(csi->ip++);
				return(0);
			case REMOVE_FILE:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp && remove(*pp)==0)
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case RENAME_FILE:
			case COPY_FILE:
			case MOVE_FILE:
				(char **)pp1=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4; /* Skip variable name */
				(char **)pp2=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp1 && *pp1 && pp2 && *pp2)
					switch(*(csi->ip-9)) {
						case RENAME_FILE:
							csi->logic=rename(*pp1,*pp2);
							break;
						case COPY_FILE:
							csi->logic=mv(*pp1,*pp2,1);
							break;
						case MOVE_FILE:
							csi->logic=mv(*pp1,*pp2,0);
							break; }
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case GET_FILE_ATTRIB:
			case SET_FILE_ATTRIB:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp && lp) {
					if(*(csi->ip-9)==GET_FILE_ATTRIB)
						*lp=_chmod(*pp,0,0);
					else
						*lp=_chmod(*pp,1,(int)*lp); }
				return(0);
			case MAKE_DIR:
			case REMOVE_DIR:
			case CHANGE_DIR:
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(pp && *pp)
					switch(*(csi->ip-5)) {
						case MAKE_DIR:
							csi->logic=mkdir(*pp);
							break;
						case REMOVE_DIR:
							csi->logic=rmdir(*pp);
							break;
						case CHANGE_DIR:
							csi->logic=chdir(*pp);
							break; }
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case OPEN_DIR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(pp && *pp && lp) {
					*lp=(long)opendir((char *)*pp);
					if(*lp)
						csi->logic=LOGIC_TRUE; }
				return(0);
			case READ_DIR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				(char **)pp=getstrvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				csi->logic=LOGIC_FALSE;
				if(pp && lp) {
					de=readdir((DIR *)(*lp));
					if(de!=NULL) {
						csi->logic=LOGIC_TRUE;
						*pp=copystrvar(csi,*pp,de->d_name); } }
				return(0);
			case REWIND_DIR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp) {
					rewinddir((DIR *)(*lp));
					csi->logic=LOGIC_TRUE; }
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			case CLOSE_DIR:
				lp=getintvar(csi,*(long *)csi->ip);
				csi->ip+=4;
				if(lp && closedir((DIR *)(*lp))==0)
					csi->logic=LOGIC_TRUE;
				else
					csi->logic=LOGIC_FALSE;
				return(0);
			default:
				errormsg(WHERE,ERR_CHK,"fio sub-instruction",*(csi->ip-1));
				return(0); }


	case CS_SWITCH:
		lp=getintvar(csi,*(long *)csi->ip);
		csi->ip+=4;
		if(!lp) {
			skipto(csi,CS_END_SWITCH);
			csi->ip++; }
		else {
			csi->misc|=CS_IN_SWITCH;
			csi->switch_val=*lp; }
		return(0);
	case CS_CASE:
		l=*(long *)csi->ip;
		csi->ip+=4;
		if(csi->misc&CS_IN_SWITCH && csi->switch_val!=l)
			skipto(csi,CS_NEXTCASE);
		else
			csi->misc&=~CS_IN_SWITCH;
		return(0);
	case CS_COMPARE_ARS:
		i=*(csi->ip++);  /* Length of ARS stored as byte before ARS */
		csi->logic=!chk_ar(csi->ip,useron);
		csi->ip+=i;
		return(0);
	case CS_TOGGLE_USER_MISC:
		useron.misc^=*(long *)csi->ip;
		putuserrec(useron.number,U_MISC,8,ultoa(useron.misc,tmp,16));
		csi->ip+=4;
		return(0);
	case CS_COMPARE_USER_MISC:
		if((useron.misc&*(long *)csi->ip)==*(long *)csi->ip)
			csi->logic=LOGIC_TRUE;
		else
			csi->logic=LOGIC_FALSE;
		csi->ip+=4;
		return(0);
	case CS_TOGGLE_USER_CHAT:
		useron.chat^=*(long *)csi->ip;
		putuserrec(useron.number,U_CHAT,8,ultoa(useron.chat,tmp,16));
		csi->ip+=4;
		return(0);
	case CS_COMPARE_USER_CHAT:
		if((useron.chat&*(long *)csi->ip)==*(long *)csi->ip)
			csi->logic=LOGIC_TRUE;
		else
			csi->logic=LOGIC_FALSE;
		csi->ip+=4;
		return(0);
	case CS_TOGGLE_USER_QWK:
		useron.qwk^=*(long *)csi->ip;
		putuserrec(useron.number,U_QWK,8,ultoa(useron.qwk,tmp,16));
		csi->ip+=4;
		return(0);
	case CS_COMPARE_USER_QWK:
		if((useron.qwk&*(long *)csi->ip)==*(long *)csi->ip)
			csi->logic=LOGIC_TRUE;
		else
			csi->logic=LOGIC_FALSE;
		csi->ip+=4;
		return(0);
	case CS_REPLACE_TEXT:
		i=*(ushort *)csi->ip;
		csi->ip+=2;
		i--;
		if(i>=TOTAL_TEXT) {
			errormsg(WHERE,ERR_CHK,"replace text #",i);
			while(*(csi->ip++));	 /* Find NULL */
			return(0); }
		if(text[i]!=text_sav[i] && text[i]!=nulstr)
			FREE(text[i]);
		j=strlen(cmdstr(csi->ip,path,csi->str,buf));
		if(!j)
			text[i]=nulstr;
		else
			text[i]=MALLOC(j+1);
		if(!text[i]) {
			errormsg(WHERE,ERR_ALLOC,"replacement text",j);
			while(*(csi->ip++));	 /* Find NULL */
			text[i]=text_sav[i];
			return(0); }
		if(j)
			strcpy(text[i],buf);
		while(*(csi->ip++));	 /* Find NULL */
		return(0);
	case CS_USE_INT_VAR:	// Self-modifying code!
		(char **)pp=getstrvar(csi,*(long *)csi->ip);
		if(pp && *pp)
			l=strtol(*pp,0,0);
		else {
			lp=getintvar(csi,*(long *)csi->ip);
			if(lp)
				l=*lp;
			else
				l=0; }
		csi->ip+=4; 			// Variable
		i=*(csi->ip++); 		// Offset
		if(i<1 || csi->ip+1+i>=csi->cs+csi->length) {
			errormsg(WHERE,ERR_CHK,"offset",i);
			csi->ip++;
			return(0); }
		switch(*(csi->ip++)) {	// Length
			case sizeof(char):
				*(csi->ip+i)=(char)l;
				break;
			case sizeof(short):
				*((short *)(csi->ip+i))=(short)l;
				break;
			case sizeof(long):
				*((long *)(csi->ip+i))=l;
                break;
			default:
				errormsg(WHERE,ERR_CHK,"length",*(csi->ip-1));
				break; }
		return(0);
	default:
		errormsg(WHERE,ERR_CHK,"shell instruction",*(csi->ip-1));
		return(0); }
}
