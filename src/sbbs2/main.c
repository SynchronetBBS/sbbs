#line 1 "MAIN.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*********************************************************/
/* Entry point to the BBS and the waitforcall() function */
/*********************************************************/

#include "sbbs.h"
#include "cmdshell.h"

csi_t	main_csi;
uint	curshell=0;

char onquiet=0,qwklogon;
char term_ret=0;
ulong connect_rate=0;	/* already connected at xbps */

#if __OS2__
void cbreakh(int sig)		 /* Ctrl-C */
{
sys_status|=SS_ABORT;
signal(sig,cbreakh);
}

#else

int cbreakh()	/* ctrl-break handler */
{
sys_status|=SS_ABORT;
return(1);		/* 1 to continue, 0 to abort */
}
#endif

/****************************************************************************/
/* This is the entry point to the BBS from dos                              */
/* No arguments are defined as of yet.                                      */
/****************************************************************************/
main(int argc, char *argv[])
{
	char	str[256];
	int 	i,j,file,twenty;
	node_t	node;

startup(argc,argv);    /* startup code overlaid */

while(1) {
	
	while(1) {
		qwklogon=0;
		twenty=0;

		/* Reset TEXT.DAT */

		for(i=0;i<TOTAL_TEXT;i++)
			if(text[i]!=text_sav[i]) {
				if(text[i]!=nulstr)
					FREE(text[i]);
				text[i]=text_sav[i]; }

		/* Reset COMMAND SHELL */

		if(main_csi.cs)
			FREE(main_csi.cs);
		if(main_csi.str)
			FREE(main_csi.str);
		freevars(&main_csi);
		memset(&main_csi,0,sizeof(csi_t));
		main_csi.str=MALLOC(1024);
		if(!main_csi.str) {
			errormsg(WHERE,ERR_ALLOC,"command shell",1024);
			bail(1); }
		memset(main_csi.str,0,1024);
		menu_dir[0]=0;
		twenty+=10;

		/* Reset Global Variables */

		if(global_str_var)
			for(i=0;i<global_str_vars;i++)
				if(global_str_var[i]) {
					FREE(global_str_var[i]);
					global_str_var[i]=0; }
		if(global_str_var) {
			FREE(global_str_var);
			global_str_var=0; }
		if(global_str_var_name) {
			FREE(global_str_var_name);
			global_str_var_name=0; }
		global_str_vars=0;
		if(global_int_var) {
			FREE(global_int_var);
			global_int_var=0; }
		if(global_int_var_name) {
			FREE(global_int_var_name);
			global_int_var_name=0; }
		global_int_vars=0;
		twenty*=2;

		if(waitforcall())		/* Got caller/logon */
			break; }

	if(qwklogon) {
		getsmsg(useron.number);
        qwk_sec();
        hangup();
		logout();
        continue; }

	while(useron.number && (main_csi.misc&CS_OFFLINE_EXEC || online)) {

		if(!main_csi.cs || curshell!=useron.shell) {
			if(useron.shell>=total_shells)
				useron.shell=0;
			sprintf(str,"%s%s.BIN",exec_dir,shell[useron.shell]->code);
			if((file=nopen(str,O_RDONLY|O_BINARY))==-1) {
				errormsg(WHERE,ERR_OPEN,str,O_RDONLY|O_BINARY);
				hangup();
				break; }
			if(main_csi.cs)
				FREE(main_csi.cs);
			freevars(&main_csi);
			clearvars(&main_csi);

			main_csi.length=filelength(file);
			if((main_csi.cs=(uchar *)MALLOC(main_csi.length))==NULL) {
				close(file);
				errormsg(WHERE,ERR_ALLOC,str,main_csi.length);
				hangup();
				break; }

			if(lread(file,main_csi.cs,main_csi.length)!=main_csi.length) {
				errormsg(WHERE,ERR_READ,str,main_csi.length);
				close(file);
				FREE(main_csi.cs);
                main_csi.cs=NULL;
				hangup();
				break; }
			close(file);

			main_csi.ip=main_csi.cs;
			curshell=useron.shell;
			menu_dir[0]=0;
			menu_file[0]=0;
		#ifdef __MSDOS__
			freedosmem=farcoreleft();
		#endif
			}
		if(exec(&main_csi))
			break;

		if(!(main_csi.misc&CS_OFFLINE_EXEC))
			checkline();

#if 0
		if(freedosmem!=farcoreleft()) {
			if(freedosmem>farcoreleft())
				errormsg(WHERE,ERR_CHK,"memory",freedosmem-farcoreleft());
			freedosmem=farcoreleft(); }
#endif
		}
	logout();
	catsyslog(0);
	if(!REALSYSOP || sys_misc&SM_SYSSTAT)
		logoffstats();	/* Updates both system and node dsts.dab files */
	if(qoc) {
		while(!wfc_events(time(NULL)))
			;
		catsyslog(0);
		if(qoc==1)
			offhook();
		lclini(node_scrnlen);
		lputc(FF);
		bail(0); }
	}
}

/************************************/
/* encrypted string output function */
/************************************/
char *decrypt(ulong l[], char *instr)
{
	static char str[128];
    uchar ch,bits,len;
	ushort i,j,lc=0;

len=(uchar)(l[0]&0x7f)^0x49;
bits=7;
for(i=0,j=0;i<len;i++) {
    ch=(char)((l[j]>>bits)&0x7fL);
    ch^=(i^0x2c);
	str[lc++]=ch;
    bits+=7;
    if(bits>=26 && i+1<len) {
		if(bits==32)
			ch=0;
		else
			ch=(char)((l[j]>>bits)&0x7fL);
        bits=(32-bits);
        j++;
        ch|=((l[j]&0x7f)<<(bits))&0x7f;
        i++;
        ch^=(i^0x2c);
        bits=7-bits;
		str[lc++]=ch; } }
str[lc]=0;
if(instr) {
	strcpy(instr,str);
	return(instr); }
else
	return(str);
}

/****************************************************************************/
/* Writes NODE.LOG at end of SYSTEM.LOG										*/
/****************************************************************************/
void catsyslog(int crash)
{
	char str[256];
	char HUGE16 *buf;
	int  i,file;
	ulong length;

if(sys_status&SS_LOGOPEN) {
	if(close(logfile)) {
		errormsg(WHERE,ERR_CLOSE,"logfile",0);
		return; }
	sys_status&=~SS_LOGOPEN; }
sprintf(str,"%sNODE.LOG",node_dir);
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
length=filelength(file);
if(length) {
	if((buf=(char HUGE16 *)LMALLOC(length))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length);
		return; }
	if(lread(file,buf,length)!=length) {
		close(file);
		errormsg(WHERE,ERR_READ,str,length);
		FREE((char *)buf);
		return; }
	close(file);
	now=time(NULL);
	unixtodos(now,&date,&curtime);
	sprintf(str,"%sLOGS\\%2.2d%2.2d%2.2d.LOG",data_dir,date.da_mon,date.da_day
		,TM_YEAR(date.da_year-1900));
	if((file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
		FREE((char *)buf);
		return; }
	if(lwrite(file,buf,length)!=length) {
		close(file);
		errormsg(WHERE,ERR_WRITE,str,length);
		FREE((char *)buf);
		return; }
	close(file);
	if(crash) {
		for(i=0;i<2;i++) {
			sprintf(str,"%sCRASH.LOG",i ? data_dir : node_dir);
			if((file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
				errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
				FREE((char *)buf);
				return; }
			if(lwrite(file,buf,length)!=length) {
				close(file);
				errormsg(WHERE,ERR_WRITE,str,length);
				FREE((char *)buf);
				return; }
			close(file); } }
	FREE((char *)buf); }
else
	close(file);
sprintf(str,"%sNODE.LOG",node_dir);
if((logfile=nopen(str,O_WRONLY|O_TRUNC))==-1) /* Truncate NODE.LOG */
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_TRUNC);
else sys_status|=SS_LOGOPEN;
}

void quicklogonstuff()
{
	int i;
	node_t node;

reset_logon_vars();

lclini(node_scrnlen-1);
if(com_port && !DCDHIGH)				/* don't take phone offhook if */
	offhook();							/* connected */
useron.number=1;
getuserdat(&useron);
autoterm=ANSI;
if(!useron.number)
	useron.level=99;
console=CON_L_ECHO|CON_L_INPUT;
online=ON_LOCAL;
statline=sys_def_stat;
statusline();
answertime=logontime=time(NULL);
sprintf(connection,"%.*s",LEN_MODEM,text[Locally]);
cur_rate=14400;
cur_cps=1750;
sys_status|=SS_USERON;
}


