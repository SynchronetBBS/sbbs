#line 1 "XTRN.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/***************************************************************************/
/* Functions that pertain to external programs (other EXE or COM programs) */
/***************************************************************************/

#include "sbbs.h"
#include "cmdshell.h"

#ifndef __FLAT__

#include "spawno.h"

void interrupt (*oldfunc)(void);

/*****************************************************************************/
/* Interrupt routine to expand WWIV Ctrl-C# codes into ANSI escape sequences */
/*****************************************************************************/
void interrupt wwiv_expand()
{
	char str[256],al;
	static int ctrl_c;
	int i,j;

al=_AL;
if(al!=3 && !ctrl_c)
	oldfunc();
else if(al!=3 && ctrl_c) {
	ctrl_c=0;
	if(useron.misc&ANSI) {
		switch(al) {
			default:
				strcpy(str,"\x1b[0m");          /* low grey */
				break;
			case '1':
				strcpy(str,"\x1b[0;1;36m");     /* high cyan */
				break;
			case '2':
				strcpy(str,"\x1b[0;1;33m");     /* high yellow */
				break;
			case '3':
				strcpy(str,"\x1b[0;35m");       /* low magenta */
				break;
			case '4':
				strcpy(str,"\x1b[0;1;44m");     /* white on blue */
				break;
			case '5':
				strcpy(str,"\x1b[0;32m");       /* low green */
				break;
			case '6':
				strcpy(str,"\x1b[0;1;5;31m");   /* high blinking red */
				break;
			case '7':
				strcpy(str,"\x1b[0;1;34m");     /* high blue */
				break;
			case '8':
				strcpy(str,"\x1b[0;34m");       /* low blue */
				break;
			case '9':
				strcpy(str,"\x1b[0;36m");       /* low cyan */
				break; }
		j=strlen(str);
		for(i=0;i<j;i++) {
			_AL=str[i];
			oldfunc(); } } }
else
	ctrl_c=1;
}
#endif

/****************************************************************************/
/* Runs an external program directly using spawnvp							*/
/****************************************************************************/
int external(char *cmdline,char mode)
{
	char c,d,cmdlen,*arg[30],str[256],str2[256],fname[128],*p,x,y;
	int i,file,rmode=0;
	long l;
	FILE *fp;
#ifdef __OS2__
	RESULTCODES rc;
#endif

if(cmdline[0]=='*') {   /* Baja module */
	strcpy(str,cmdline+1);
	p=strchr(str,SP);
	if(p) {
		strcpy(main_csi.str,p+1);
		*p=0; }
	return(exec_bin(str,&main_csi)); }

#ifdef __MSDOS__
nosound();						/* if page is on, turn off sound */
#endif
attr(LIGHTGRAY);                /* set attributes to normal text */

strcpy(str,cmdline);		/* Set str to program name only */
p=strchr(str,SP);
if(p) *p=0;
strcpy(fname,str);


if(!(mode&EX_CC)) {
	if(strcspn(cmdline,"<>|")!=strlen(cmdline))
		mode|=EX_CC;	/* DOS I/O redirection; so, use command.com */
	else {
		i=strlen(str);
		if(i>4 && !stricmp(str+(i-4),".BAT"))   /* specified .BAT file */
			mode|=EX_CC;
		else {
			strcat(str,".BAT");
			if(fexist(str)) 					/* and it's a .BAT file */
				mode|=EX_CC; } } }

p=strrchr(fname,'\\');
if(!p) p=strchr(fname,':');
if(!p) p=fname;
else   p++;

#ifndef __FLAT__

for(i=0;i<total_swaps;i++)
	if(!stricmp(p,swap[i]->cmd))
		break;
if(i<total_swaps)
	mode|=EX_SWAP;

#else

for(i=0;i<total_os2pgms;i++)
	if(!stricmp(p,os2pgm[i]->name))
		break;
if(i<total_os2pgms) {
	mode|=EX_OS2;
	if(os2pgm[i]->misc&OS2_POPEN)
		mode|=EX_POPEN; }

#endif

#ifndef __FLAT__

if(node_swap&SWAP_NONE || mode&EX_WWIV)
	mode&=~EX_SWAP;

if(mode&EX_SWAP) {
	if(lclwy()>1 || lclwx()>1)
        lputs(crlf);
	lputs("Swapping...\r\n"); }

#endif


if(!(mode&EX_OS2) && mode&EX_CC)
	sprintf(str,"%s /C %s",
#ifdef __OS2__
	node_comspec	 // Only used node_comspec for OS/2 ver
#else
	comspec
#endif
	,cmdline);

else strcpy(str,cmdline);
if(!(mode&EX_OS2) && strlen(str)>126) {
	errormsg(WHERE,ERR_LEN,str,strlen(str));
	errorlevel=-1;
	return(-1); }

#ifndef __FLAT__

arg[0]=str;	/* point to the beginning of the string */
cmdlen=strlen(str);
for(c=0,d=1;c<cmdlen;c++)	/* Break up command line */
	if(str[c]==SP) {
		str[c]=0;			/* insert nulls */
		arg[d++]=str+c+1; } /* point to the beginning of the next arg */
arg[d]=0;

#endif

if(mode&EX_OUTR && (console&CON_R_ECHO))
	rmode|=INT29R;
if(mode&EX_OUTL)
	rmode|=INT29L;
if(mode&EX_INR && (console&CON_R_INPUT))
	rmode|=INT16;

#ifndef __FLAT__

if(rmode)
	ivhctl(rmode);	/* set DOS output interception vectors */

if(mode&EX_WWIV) {	/* WWIV code expansion */
	rioctl(CPTOFF); /* turn off ctrl-p translation */
	oldfunc=getvect(0x29);
	setvect(0x29,wwiv_expand); }

if(com_port && sys_status&SS_COMISR && !(mode&(EX_OUTR|EX_INR))) {
	riosync(0);
	rioini(0,0);
	sys_status&=~SS_COMISR; }

if(!rmode) {					/* clear the status line */
    lclini(node_scrnlen);
    x=lclwx();
    y=lclwy();
	STATUSLINE;
    lclxy(1,node_scrnlen);
	lputc(CLREOL);
	TEXTWINDOW;
	lclxy(x,y); }

if(mode&EX_SWAP) {				/* set the resident size */
	if(rmode)
		__spawn_resident=7000;	/* was 6000 */
	else
		__spawn_resident=0;
	i=spawnvpeo(node_swapdir,arg[0],(const char **)arg
		,(const char **)environ); }
else
	i=spawnvpe(P_WAIT,arg[0],arg,environ);

#else //lif defined(__OS2__)

if(com_port && !(mode&EX_POPEN)
	&& sys_status&SS_COMISR) {	/* Uninstall COM routines */
	riosync(0);
	rioini(0,0);
	sys_status&=~SS_COMISR; }

textattr(LIGHTGRAY);	// Redundant
if(!(mode&EX_OS2)) {	/* DOS pgm */
	if(lclwy()>1 || lclwx()>1)
        lputs(crlf);
	lprintf("Executing DOS program: %s\r\n",str);
	sprintf(str2,"%sEXECDOS.DAT",node_dir);
	if((file=nopen(str2,O_WRONLY|O_TRUNC|O_CREAT))!=-1) {
		sprintf(str2,"V1.00\r\n%X\r\n%lu\r\n%u\r\n%X\r\n%X\r\n%lX\r\n%u\r\n"
			"%s\r\n"
			,online==ON_REMOTE ? com_base:0
			,com_irq,dte_rate,rmode,mode,useron.misc,node_num,str);
		write(file,str2,strlen(str2));
		sprintf(str2,"%u\r\nDSZLOG=%s\r\n"
			,1	/* Total env vars to setup */
			,getenv("DSZLOG"));
		write(file,str2,strlen(str2));
		if(online) {
			sprintf(str2,"%s\r\n%s\r\n%u\r\n%u\r\n%c\r\n%s\r\n%s\r\n"
				,useron.alias
				,useron.name
				,useron.level
				,getage(useron.birth)
				,useron.sex
				,useron.phone
				,useron.location);
			write(file,str2,strlen(str2)); }
		close(file); }

	sprintf(str,"%sEXECDOS.EXE %s",exec_dir,node_dir);

	x=wherex();
	y=wherey();
	i=system(str);
	gotoxy(x,y); }
else {	 /* OS/2 pgm */
	window(1,1,80,node_scrnlen);
	x=lclwx();
    y=lclwy();
    lclxy(1,node_scrnlen);
	lputc(CLREOL);
    lclxy(x,y);
	i=system(cmdline);
	lputs(crlf);
	lputs(crlf);
	lclxy(1,node_scrnlen-1);
	}

#ifdef __OS2__
fixkbdmode();
#endif
textattr(LIGHTGRAY);

#endif

#ifndef __WIN32__
while(lkbrd(0)) 	/* suck up any chars in the keyboard buffer */
	;
#endif

#ifdef __MSDOS__
setcbrk(0);

lclatr(LIGHTGRAY);
c=wherey();
if(c>=node_scrnlen)
	c=node_scrnlen-1;
lclxy(wherex(),c);			/* put the cursor where BIOS thinks it is */

#endif

if(com_port && !(mode&EX_POPEN)
#ifndef __OS2__
	&& !(mode&(EX_OUTR|EX_INR))
#endif
	) {
	comini();
	setrate();
	rioctl(IOSM|PAUSE|ABORT); }

rioctl(CPTON);	/* turn on ctrl-p translation */

#ifndef __FLAT__
if(mode&EX_WWIV)
	setvect(0x29,oldfunc);

if(rmode)
	ivhctl(0);		/* replace DOS output interrupt vectors */
#endif

setdisk(node_disk);
strcpy(str,node_dir);
str[strlen(str)-1]=0;
if(chdir(str))
	errormsg(WHERE,ERR_CHDIR,str,0);

#ifndef __FLAT__
lclini(node_scrnlen-1);
#endif

lncntr=0;
if(online)
	statusline();  /*  Replace status line after calling external program */
errorlevel=i;
return(i);
}

#ifndef __FLAT__
extern uint riobp;
extern mswtyp;
#endif

uint fakeriobp=0xffff;

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *cmdstr(char *instr, char *fpath, char *fspec, char *outstr)
{
	static char static_cmd[128];
	char str[256],str2[128],*cmd;
    int i,j,len;

if(outstr==NULL)
	cmd=static_cmd;
else
	cmd=outstr;
len=strlen(instr);
for(i=j=0;i<len && j<128;i++) {
    if(instr[i]=='%') {
        i++;
        cmd[j]=0;
        switch(toupper(instr[i])) {
            case 'A':   /* User alias */
                strcat(cmd,useron.alias);
                break;
            case 'B':   /* Baud (DTE) Rate */
                strcat(cmd,ultoa(dte_rate,str,10));
                break;
            case 'C':   /* Connect Description */
                strcat(cmd,connection);
                break;
            case 'D':   /* Connect (DCE) Rate */
                strcat(cmd,ultoa((ulong)cur_rate,str,10));
                break;
            case 'E':   /* Estimated Rate */
                strcat(cmd,ultoa((ulong)cur_cps*10,str,10));
                break;
            case 'F':   /* File path */
                strcat(cmd,fpath);
                break;
            case 'G':   /* Temp directory */
				strcat(cmd,temp_dir);
                break;
			case 'H':   /* Port Handle or Hardware Flow Control */
#ifdef __OS2__
				strcat(cmd,itoa(rio_handle,str,10));
#else
                if(mdm_misc&MDM_CTS)
                    strcat(cmd,"Y");
                else
                    strcat(cmd,"N");
#endif
                break;
            case 'I':   /* UART IRQ Line */
                strcat(cmd,itoa(com_irq,str,10));
                break;
            case 'J':
				strcat(cmd,data_dir);
                break;
            case 'K':
				strcat(cmd,ctrl_dir);
                break;
            case 'L':   /* Lines per message */
                strcat(cmd,itoa(level_linespermsg[useron.level],str,10));
                break;
            case 'M':   /* Minutes (credits) for user */
                strcat(cmd,ultoa(useron.min,str,10));
                break;
            case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                strcat(cmd,node_dir);
                break;
            case 'O':   /* SysOp */
                strcat(cmd,sys_op);
                break;
            case 'P':   /* COM Port */
                strcat(cmd,itoa(online==ON_LOCAL ? 0:com_port,str,10));
                break;
            case 'Q':   /* QWK ID */
                strcat(cmd,sys_id);
                break;
            case 'R':   /* Rows */
                strcat(cmd,itoa(rows,str,10));
                break;
            case 'S':   /* File Spec */
                strcat(cmd,fspec);
                break;
            case 'T':   /* Time left in seconds */
                gettimeleft();
                strcat(cmd,itoa(timeleft,str,10));
                break;
            case 'U':   /* UART I/O Address (in hex) */
                strcat(cmd,itoa(com_base,str,16));
                break;
            case 'V':   /* Synchronet Version */
                sprintf(str,"%s%c",VERSION,REVISION);
                break;
            case 'W':   /* Time-slice API type (mswtype) */
#ifndef __FLAT__
                strcat(cmd,itoa(mswtyp,str,10));
#endif
                break;
            case 'X':
                strcat(cmd,shell[useron.shell]->code);
                break;
            case '&':   /* Address of msr */
#ifndef __FLAT__
                sprintf(str,"%lu",sys_status&SS_DCDHIGH ? &fakeriobp
                    : online==ON_REMOTE ? &riobp-1 : 0);
                strcat(cmd,str);
#else
				strcat(cmd,"%&");
#endif
                break;
			case 'Y':
				strcat(cmd,
#ifdef __OS2__
				node_comspec
#else
				comspec
#endif
				);
				break;
			case 'Z':
				strcat(cmd,text_dir);
                break;
            case '!':   /* EXEC Directory */
				strcat(cmd,exec_dir);
                break;
            case '#':   /* Node number (same as SBBSNNUM environment var) */
                sprintf(str,"%d",node_num);
                strcat(cmd,str);
                break;
            case '*':
                sprintf(str,"%03d",node_num);
                strcat(cmd,str);
                break;
            case '$':   /* Credits */
                strcat(cmd,ultoa(useron.cdt+useron.freecdt,str,10));
                break;
            case '%':   /* %% for percent sign */
                strcat(cmd,"%");
                break;
            default:    /* unknown specification */
                if(isdigit(instr[i])) {
                    sprintf(str,"%0*d",instr[i]&0xf,useron.number);
                    strcat(cmd,str); }
/*
                else
                    errormsg(WHERE,ERR_CHK,instr,i);
*/
                break; }
        j=strlen(cmd); }
    else
        cmd[j++]=instr[i]; }
cmd[j]=0;

return(cmd);
}

