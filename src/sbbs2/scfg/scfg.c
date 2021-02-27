#line 2 "SCFG.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/****************************************************************************/
/* Synchronet configuration utility 										*/
/****************************************************************************/

#include "scfg.h"

#ifdef __FLAT__
	unsigned _stklen=64000;
#else
	unsigned _stklen=32000;
#endif

extern char *wday[];	/* names of weekdays (3 char) */
extern char uifc_status;

/********************/
/* Global Variables */
/********************/

long freedosmem;
int  no_dirchk=0,no_msghdr=0,forcesave=0,all_msghdr=0;
char **opt;
char tmp[256];
char *nulstr="";
char **mdm_type;
char **mdm_file;
int  mdm_types;
int  backup_level=3;

read_cfg_text_t txt={
	"\7\r\nError opening %s for read.\r\n",
	"","",
	"\7\r\nError allocating %u bytes of memory\r\n",
	"\7\r\nERROR: Offset %lu in %s\r\n\r\n" };

char *invalid_code=
"Invalid Internal Code:\n\n"
"Internal codes can be up to eight characters in length and can only\n"
"contain valid DOS filename characters. The code you have entered\n"
"contains one or more invalid characters.";

char *num_flags=
"Number of Flags Needed:\n\n"
"If you want users to be required to have all the flags, select All.\n"
"\n"
"If you want users to be required to have any one or more of the flags,\n"
"select One (Allowed).";


void allocfail(uint size)
{
cprintf("\7Error allocating %u bytes of memory.\r\n",size);
bail(1);
}

time_t checktime(void)
{
	struct tm tm;

memset(&tm,0,sizeof(tm));
tm.tm_year=94;
tm.tm_mday=1;
return(mktime(&tm)^0x2D24BD00L);
}

int main(int argc, char **argv)
{
	char	**mopt,*p;
	int 	i,j,main_dflt=0,no_emsovl=0,chat_dflt=0;
	uint	u;
	long	l;
	char 	str[129];
	FILE	*instream;
#ifndef __FLAT__
	union	REGS reg;
#endif

printf("\r\nSynchronet Configuration Util (%s)  v%s%c  Developed 1995-1997 "
	"Rob Swindell\r\n",
#ifdef __OS2__
	"OS/2"
#elif defined(__FLAT__)
	"DOS/Win32"
#else
	"DOS"
#endif
	,VERSION,REVISION);

if(argc>1)
	strcpy(ctrl_dir,argv[1]);
else
	getcwd(ctrl_dir,MAXDIR);
if(argc>2) {
	for(i=2;i<argc;i++) {
		if(!stricmp(argv[i],"/M"))      /* Show free mem */
			show_free_mem=!show_free_mem;
		else if(!stricmp(argv[i],"/N")) /* No EMS */
			no_emsovl=!no_emsovl;
#if !defined(__FLAT__)
		else if(!strnicmp(argv[i],"/T",2)) /* Windows/OS2 time slice API */
			mswtyp=atoi(argv[i]+2);
#endif
		else if(!strnicmp(argv[i],"/C",2)) /* Force color mode */
			uifc_status|=UIFC_COLOR;
		else if(!strnicmp(argv[i],"/B",2)) /* Backup level */
			backup_level=atoi(argv[i]+2);
		else if(!stricmp(argv[i],"/S")) /* No dir checking */
			no_dirchk=!no_dirchk;
		else if(!stricmp(argv[i],"/H")) /* No msg header updating */
			no_msghdr=!no_msghdr;
		else if(!stricmp(argv[i],"/U")) /* Update all message headers */
			all_msghdr=!all_msghdr;
		else if(!stricmp(argv[i],"/F")) /* Force save of config files */
			forcesave=!forcesave;
		else {
			printf("\r\nusage: scfg <ctrl_dir> [/c] [/m] [/n] [/s] [/u] [/h] "
				"[/t#] [/b#]\r\n"
				"\r\n"
				"/c  =  force color mode\r\n"
				"/m  =  show free memory\r\n"
				"/n  =  don't use EMS\r\n"
				"/s  =  don't check directories\r\n"
				"/f  =  force save of config files\r\n"
				"/u  =  update all message base status headers\r\n"
				"/h  =  don't update message base status headers\r\n"
				"/t# =  set supported time slice APIs to #\r\n"
				"/b# =  set automatic back-up level (default=3 max=10)\r\n"
				);
			exit(0); } } }

if(backup_level>10) backup_level=10;

if(ctrl_dir[strlen(ctrl_dir)-1]!='\\' && ctrl_dir[strlen(ctrl_dir)-1]!=':')
        strcat(ctrl_dir,"\\");

init_mdms();

uifcini();

#if !defined(__FLAT__)
if(!no_emsovl) {
	cputs("\r\nEMS: ");
	if((i=open("EMMXXXX0",O_RDONLY))==-1)
		cputs("not installed.");
	else {
		close(i);
		reg.h.ah=0x46;			/* Get EMS version */
		int86(0x67,&reg,&reg);
		if(reg.h.ah)
			cputs("\7error getting version.");
		else {
			cprintf("Version %u.%u ",(reg.h.al&0xf0)>>4,reg.h.al&0xf);
			if(_OvrInitEms(0,0,23))    /* use up to 360K */
				cprintf("allocation failed."); } }
	cputs("\r\n"); }
#endif

#ifdef __FLAT__
if(putenv("TZ=UCT0"))
	printf("putenv() failed!\n");
tzset();
#endif

if((l=checktime())!=0) {   /* Check binary time */
	printf("Time problem (%08lx)\n",l);
    exit(1); }

if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
	allocfail(sizeof(char *)*(MAX_OPTS+1));
for(i=0;i<(MAX_OPTS+1);i++)
	if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
		allocfail(MAX_OPLN);

if((mopt=(char **)MALLOC(sizeof(char *)*14))==NULL)
	allocfail(sizeof(char *)*14);
for(i=0;i<14;i++)
	if((mopt[i]=(char *)MALLOC(64))==NULL)
		allocfail(64);

txt.reading=nulstr;
txt.readit=nulstr;

strcpy(str,argv[0]);
p=strrchr(str,'\\');
if(!p) {
	sprintf(helpdatfile,"..\\EXEC\\SCFGHELP.DAT");
	sprintf(helpixbfile,"..\\EXEC\\SCFGHELP.IXB"); }
else {
	*p=0;
	sprintf(helpdatfile,"%s\\SCFGHELP.DAT",str);
	sprintf(helpixbfile,"%s\\SCFGHELP.IXB",str); }

sprintf(str,"Synchronet Configuration for %s v%s",
#if defined(__OS2__)
	"OS/2"
#elif defined(__FLAT__)
	"DOS/Win32"
#else
	"DOS"
#endif
	,VERSION);
if(uscrn(str)) {
	cprintf("USCRN failed!\r\n");
	bail(1); }
i=0;
strcpy(mopt[i++],"Nodes");
strcpy(mopt[i++],"System");
strcpy(mopt[i++],"Networks");
strcpy(mopt[i++],"File Areas");
strcpy(mopt[i++],"File Options");
strcpy(mopt[i++],"Chat Features");
strcpy(mopt[i++],"Message Areas");
strcpy(mopt[i++],"Message Options");
strcpy(mopt[i++],"Command Shells");
strcpy(mopt[i++],"External Programs");
strcpy(mopt[i++],"Text File Sections");
mopt[i][0]=0;
#ifndef __FLAT__
freedosmem=farcoreleft();
#endif
while(1) {
#if 0
	if(freedosmem!=farcoreleft()) {
		errormsg(WHERE,ERR_CHK,"lost memory",freedosmem-farcoreleft());
		freedosmem=farcoreleft(); }
#endif
	SETHELP(WHERE);
/*
Main Configuration Menu:

This is the main menu of the Synchronet configuration utility (SCFG).
From this menu, you have the following choices:

	Nodes				  : Add, delete, or configure nodes
	System				  : System-wide configuration options
	Networks			  : Message networking configuration
	File Areas			  : File area configuration
	File Options		  : File area options
	Chat Features		  : Chat actions, sections, pagers, and gurus
	Message Areas		  : Message area configuration
	Message Options 	  : Message and email options
	External Programs	  : Events, editors, and online programs
	Text File Sections	  : General text file area

Use the arrow keys and  ENTER  to select an option, or  ESC  to exit.
*/
	switch(ulist(WIN_ORG|WIN_MID|WIN_ESC|WIN_ACT,0,0,30,&main_dflt,0
		,"Configure",mopt)) {
		case 0:
			upop("Reading MAIN.CNF...");
			read_main_cfg(txt);
			upop(0);
			node_menu();
			free_main_cfg();
			break;
		case 1:
			upop("Reading MAIN.CNF...");
			read_main_cfg(txt);
			upop("Reading XTRN.CNF...");
			read_xtrn_cfg(txt);
			upop(0);
			sys_cfg();
			free_xtrn_cfg();
			free_main_cfg();
			break;
		case 2:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
			upop("Reading MSGS.CNF...");
			read_msgs_cfg(txt);
			upop(0);
			net_cfg();
			free_msgs_cfg();
			free_main_cfg();
			break;
		case 3:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
            upop("Reading FILE.CNF...");
            read_file_cfg(txt);
			upop(0);
            xfer_cfg();
            free_file_cfg();
			free_main_cfg();
            break;
		case 4:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
            upop("Reading FILE.CNF...");
            read_file_cfg(txt);
			upop(0);
			xfer_opts();
            free_file_cfg();
			free_main_cfg();
            break;
		case 5:
            upop("Reading CHAT.CNF...");
            read_chat_cfg(txt);
			upop(0);
            while(1) {
                i=0;
                strcpy(opt[i++],"Artificial Gurus");
                strcpy(opt[i++],"Multinode Chat Actions");
                strcpy(opt[i++],"Multinode Chat Channels");
                strcpy(opt[i++],"External Sysop Chat Pagers");
				opt[i][0]=0;
                j=ulist(WIN_ORG|WIN_ACT|WIN_CHE,0,0,0,&chat_dflt,0
                    ,"Chat Features",opt);
                if(j==-1) {
                    j=save_changes(WIN_MID);
                    if(j==-1)
                        continue;
                    if(!j)
                        write_chat_cfg();
                    break; }
                switch(j) {
                    case 0:
                        guru_cfg();
                        break;
                    case 1:
                        actsets_cfg();
                        break;
                    case 2:
                        chan_cfg();
                        break;
                    case 3:
                        page_cfg();
                        break; } }
            free_chat_cfg();
            break;
		case 6:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
			upop("Reading MSGS.CNF...");
			read_msgs_cfg(txt);
			upop(0);
			msgs_cfg();
			free_msgs_cfg();
			free_main_cfg();
			break;
		case 7:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
			upop("Reading MSGS.CNF...");
            read_msgs_cfg(txt);
			upop(0);
			msg_opts();
			free_msgs_cfg();
			free_main_cfg();
			break;
		case 8:
			upop("Reading MAIN.CNF...");
			read_main_cfg(txt);
			upop(0);
			shell_cfg();
			free_main_cfg();
            break;
		case 9:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
			upop("Reading XTRN.CNF...");
			read_xtrn_cfg(txt);
			upop(0);
			xprogs_cfg();
			free_xtrn_cfg();
			free_main_cfg();
			break;
		case 10:
			upop("Reading MAIN.CNF...");
            read_main_cfg(txt);
            upop("Reading FILE.CNF...");
            read_file_cfg(txt);
			upop(0);
            txt_cfg();
            free_file_cfg();
			free_main_cfg();
            break;
		case -1:
			i=0;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			SETHELP(WHERE);
/*
Exit SCFG:

If you want to exit the Synchronet configuration utility, select Yes.
Otherwise, select No or hit  ESC .
*/
			i=ulist(WIN_MID,0,0,0,&i,0,"Exit SCFG",opt);
			if(!i)
				bail(0);
			break; } }
}

/****************************************************************************/
/* Checks the changes variable. If there have been no changes, returns 2.	*/
/* If there have been changes, it prompts the user to change or not. If the */
/* user escapes the menu, returns -1, selects Yes, 0, and selects no, 1 	*/
/****************************************************************************/
int save_changes(int mode)
{
	int i=0;

if(!changes)
	return(2);
strcpy(opt[0],"Yes");
strcpy(opt[1],"No");
opt[2][0]=0;
if(mode&WIN_SAV && savdepth)
	savnum++;
SETHELP(WHERE);
/*
Save Changes:

You have made some changes to the configuration. If you want to save
these changes, select Yes. If you are positive you DO NOT want to save
these changes, select No. If you are not sure and want to review the
configuration before deciding, hit  ESC .
*/
i=ulist(mode|WIN_ACT,0,0,0,&i,0,"Save Changes",opt);
if(mode&WIN_SAV && savdepth)
	savnum--;
if(i!=-1)
	changes=0;
return(i);
}

void txt_cfg()
{
	static int txt_dflt,bar;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static txtsec_t savtxtsec;

while(1) {
	for(i=0;i<total_txtsecs;i++)
		sprintf(opt[i],"%-25s",txtsec[i]->name);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_CHE;
	if(total_txtsecs)
		j|=WIN_DEL|WIN_GET;
	if(total_txtsecs<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savtxtsec.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Text File Sections:

This is a list of General Text File (G-File) Sections configured for
your system. G-File sections are used to store text files that can be
viewed freely by the users. Common text file section topics include
ANSI Artwork, System Information, Game Help Files, and other special
interest topics.

To add a text file section, select the desired location with the arrow
keys and hit  INS .

To delete a text file section, select it and hit  DEL .

To configure a text file, select it and hit  ENTER .
*/
	i=ulist(j,0,0,45,&txt_dflt,&bar,"Text File Sections",opt);
	if((signed)i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
			continue;
		if(!j)
			write_file_cfg();
		return; }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"ANSI Artwork");
		SETHELP(WHERE);
/*
Text Section Name:

This is the name of this text section.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Text Section Name",str,40
            ,K_EDIT)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Text Section Internal Code:

Every text file section must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Text Section Internal Code",code,8
			,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }
		if((txtsec=(txtsec_t **)REALLOC(txtsec
			,sizeof(txtsec_t *)*(total_txtsecs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_txtsecs+1);
			total_txtsecs=0;
			bail(1);
			continue; }
		if(total_txtsecs)
			for(j=total_txtsecs;j>i;j--)
                txtsec[j]=txtsec[j-1];
		if((txtsec[i]=(txtsec_t *)MALLOC(sizeof(txtsec_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(txtsec_t));
			continue; }
		memset((txtsec_t *)txtsec[i],0,sizeof(txtsec_t));
		strcpy(txtsec[i]->name,str);
		strcpy(txtsec[i]->code,code);
		total_txtsecs++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(txtsec[i]);
		total_txtsecs--;
		for(j=i;j<total_txtsecs;j++)
			txtsec[j]=txtsec[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savtxtsec=*txtsec[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*txtsec[i]=savtxtsec;
		changes=1;
        continue; }
	i=txt_dflt;
	j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",txtsec[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Access Requirements"
			,txtsec[i]->ar);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",txtsec[i]->code);
		opt[k][0]=0;
		switch(ulist(WIN_ACT|WIN_MID,0,0,60,&j,0,txtsec[i]->name
			,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Text Section Name:

This is the name of this text section.
*/
				strcpy(str,txtsec[i]->name);	/* save */
				if(!uinput(WIN_MID|WIN_SAV,0,10
					,"Text File Section Name"
					,txtsec[i]->name,40,K_EDIT))
					strcpy(txtsec[i]->name,str);
				break;
			case 1:
				sprintf(str,"%s Text Section",txtsec[i]->name);
				getar(str,txtsec[i]->ar);
				break;
			case 2:
				strcpy(str,txtsec[i]->code);
				SETHELP(WHERE);
/*
Text Section Internal Code:

Every text file section must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(txtsec[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
					helpbuf=0; }
				break; } } }
}

void shell_cfg()
{
	static int shell_dflt,shell_bar;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static shell_t savshell;

while(1) {
	for(i=0;i<total_shells;i++)
		sprintf(opt[i],"%-25s",shell[i]->name);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_CHE;
	if(total_shells)
		j|=WIN_DEL|WIN_GET;
	if(total_shells<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savshell.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Command Shells:

This is a list of Command Shells configured for your system.
Command shells are the programmable command and menu structures which
are available for your BBS.

To add a command shell section, select the desired location with the arrow
keys and hit  INS .

To delete a command shell, select it and hit  DEL .

To configure a command shell, select it and hit  ENTER .
*/
	i=ulist(j,0,0,45,&shell_dflt,&shell_bar,"Command Shells",opt);
	if((signed)i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
			continue;
		if(!j)
			write_main_cfg();
		return; }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"Menu Shell");
		SETHELP(WHERE);
/*
Command Shell Name:

This is the descriptive name of this command shell.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Command Shell Name",str,40
            ,K_EDIT)<1)
            continue;
		sprintf(code,"%.8s",str);
		p=strchr(code,SP);
		if(p) *p=0;
        strupr(code);
		SETHELP(WHERE);
/*
Command Shell Internal Code:

Every command shell must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.

This code will be the base filename used to load the shell from your
EXEC directory. e.g. A shell with an internal code of MYBBS would
indicate a Baja shell file named MYBBS.BIN in your EXEC directory.
*/
		if(uinput(WIN_MID|WIN_SAV,0,0,"Command Shell Internal Code",code,8
			,K_EDIT|K_UPPER)<1)
			continue;
		if(!code_ok(code)) {
			helpbuf=invalid_code;
			umsg("Invalid Code");
			helpbuf=0;
            continue; }
		if((shell=(shell_t **)REALLOC(shell
			,sizeof(shell_t *)*(total_shells+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,total_shells+1);
			total_shells=0;
			bail(1);
			continue; }
		if(total_shells)
			for(j=total_shells;j>i;j--)
				shell[j]=shell[j-1];
		if((shell[i]=(shell_t *)MALLOC(sizeof(shell_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(shell_t));
			continue; }
		memset((shell_t *)shell[i],0,sizeof(shell_t));
		strcpy(shell[i]->name,str);
		strcpy(shell[i]->code,code);
		total_shells++;
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(shell[i]);
		total_shells--;
		for(j=i;j<total_shells;j++)
			shell[j]=shell[j+1];
		changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savshell=*shell[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*shell[i]=savshell;
		changes=1;
        continue; }
	i=shell_dflt;
	j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",shell[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Access Requirements"
			,shell[i]->ar);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",shell[i]->code);
		opt[k][0]=0;
		SETHELP(WHERE);
/*
Command Shell:

A command shell is a programmed command and menu structure that you or
your users can use to navigate the BBS. For every command shell
configured here, there must be an associated .BIN file in your EXEC
directory for Synchronet to execute.

Command shell files are created by using the Baja command shell compiler
to turn Baja source code (.SRC) files into binary files (.BIN) for
Synchronet to interpret. See the example .SRC files in the TEXT
directory and the documentation for the Baja compiler for more details.
*/
		switch(ulist(WIN_ACT|WIN_MID,0,0,60,&j,0,shell[i]->name
			,opt)) {
			case -1:
				done=1;
				break;
			case 0:
				SETHELP(WHERE);
/*
Command Shell Name:

This is the descriptive name of this command shell.
*/
				strcpy(str,shell[i]->name);    /* save */
				if(!uinput(WIN_MID|WIN_SAV,0,10
					,"Command Shell Name"
					,shell[i]->name,40,K_EDIT))
					strcpy(shell[i]->name,str);
				break;
			case 1:
				sprintf(str,"%s Command Shell",shell[i]->name);
				getar(str,shell[i]->ar);
				break;
			case 2:
				strcpy(str,shell[i]->code);
				SETHELP(WHERE);
/*
Command Shell Internal Code:

Every command shell must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.

This code will be the base filename used to load the shell from your
EXEC directory. e.g. A shell with an internal code of MYBBS would
indicate a Baja shell file named MYBBS.BIN in your EXEC directory.
*/
				uinput(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT|K_UPPER);
				if(code_ok(str))
					strcpy(shell[i]->code,str);
				else {
					helpbuf=invalid_code;
					umsg("Invalid Code");
					helpbuf=0; }
				break; } } }
}

/****************************************************************************/
/* Deletes all files in dir 'path' that match file spec 'spec'              */
/****************************************************************************/
int delfiles(char *inpath, char *spec)
{
    char str[256],path[128],done,c;
    int files=0;
    struct ffblk ff;

strcpy(path,inpath);
c=strlen(path);
if(path[c-1]!='\\' && path[c-1]!=':')
    strcat(path,"\\");
sprintf(str,"%s%s",path,spec);
done=findfirst(str,&ff,0);
while(!done) {
    sprintf(str,"%s%s",path,ff.ff_name);
    if(remove(str))
        errormsg(WHERE,ERR_REMOVE,str,0);
    else
        files++;
    done=findnext(&ff); }
return(files);
}

int whichlogic()
{
	int i;

i=0;
strcpy(opt[0],"Greater than or Equal");
strcpy(opt[1],"Equal");
strcpy(opt[2],"Not Equal");
strcpy(opt[3],"Less than");
opt[4][0]=0;
if(savdepth)
	savnum++;
SETHELP(WHERE);
/*
Select Logic for Requirement:

This menu allows you to choose the type of logic evaluation to use
in determining if the requirement is met. If, for example, the user's
level is being evaluated and you select Greater than or Equal from
this menu and set the required level to 50, the user must have level
50 or higher to meet this requirement. If you selected Equal from
this menu and set the required level to 50, the user must have level
50 exactly. If you select Not equal and level 50, then the user
must have any level BUT 50. And if you select Less than from this
menu and level 50, the user must have a level below 50.
*/
i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Select Logic",opt);
if(savdepth)
	savnum--;
return(i);
}

int whichcond()
{
	int i;

i=0;
strcpy(opt[0],"AND (Both/All)");
strcpy(opt[1],"OR  (Either/Any)");
opt[2][0]=0;
if(savdepth)
	savnum++;
SETHELP(WHERE);
/*
Select Logic for Multiple Requirements:

If you wish this new parameter to be required along with the other
parameters, select AND to specify that both or all of the
parameter requirments must be met.

If you wish this new parameter to only be required if the other
parameter requirements aren't met, select OR to specify that either
or any of the parameter requirements must be met.
*/
i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Multiple Requirement Logic",opt);
if(savdepth)
	savnum--;
return(i);
}


void getar(char *desc, char *inar)
{
	static int curar;
	char str[128],ar[128];
	int i,j,len,done=0,n;

strcpy(ar,inar);
while(!done) {
	len=strlen(ar);
	if(len>=30) {	  /* Needs to be shortened */
		str[0]=0;
		n=strlen(ar);
		for(i=0;i<n;i++) {					/* Shorten operators */
			if(!strncmp(ar+i,"AND",3)) {
				strcat(str,"&");
				i+=2; }
			else if(!strncmp(ar+i,"NOT",3)) {
				strcat(str,"!");
				i+=2; }
			else if(!strncmp(ar+i,"EQUAL",5)) {
				strcat(str,"=");
				i+=4; }
			else if(!strncmp(ar+i,"EQUALS",6)) {
				strcat(str,"=");
				i+=5; }
			else if(!strncmp(ar+i,"EQUAL TO",8)) {
				strcat(str,"=");
				i+=7; }
			else if(!strncmp(ar+i,"OR",2)) {
				strcat(str,"|");
				i+=1; }
			else
				strncat(str,ar+i,1); }
		strcpy(ar,str);
		len=strlen(ar); }

	if(len>=30) {
		str[0]=0;
		n=strlen(ar);
		for(i=0;i<n;i++) {					/* Remove spaces from ! and = */
			if(!strncmp(ar+i," ! ",3)) {
				strcat(str,"!");
				i+=2; }
			else if(!strncmp(ar+i,"= ",2)) {
				strcat(str,"=");
                i++; }
			else if(!strncmp(ar+i," = ",3)) {
				strcat(str,"=");
				i+=2; }
			else
				strncat(str,ar+i,1); }
		strcpy(ar,str);
        len=strlen(ar); }

	if(len>=30) {
		str[0]=0;
		n=strlen(ar);
		for(i=0;i<n;i++) {					/* Remove spaces from & and | */
			if(!strncmp(ar+i," & ",3)) {
				strcat(str," ");
				i+=2; }
			else if(!strncmp(ar+i," | ",3)) {
				strcat(str,"|");
				i+=2; }
			else
				strncat(str,ar+i,1); }
		strcpy(ar,str);
        len=strlen(ar); }

	if(len>=30) {					/* change week days to numbers */
        str[0]=0;
        n=strlen(ar);
		for(i=0;i<n;i++) {
			for(j=0;j<7;j++)
                if(!strnicmp(ar+i,wday[j],3)) {
                    strcat(str,itoa(j,tmp,10));
					i+=2;
					break; }
			if(j==7)
				strncat(str,ar+i,1); }
        strcpy(ar,str);
        len=strlen(ar); }

	if(len>=30) {				  /* Shorten parameters */
		str[0]=0;
		n=strlen(ar);
		for(i=0;i<n;i++) {
			if(!strncmp(ar+i,"AGE",3)) {
				strcat(str,"$A");
				i+=2; }
			else if(!strncmp(ar+i,"BPS",3)) {
				strcat(str,"$B");
				i+=2; }
			else if(!strncmp(ar+i,"PCR",3)) {
				strcat(str,"$P");
				i+=2; }
			else if(!strncmp(ar+i,"RIP",3)) {
				strcat(str,"$*");
				i+=2; }
			else if(!strncmp(ar+i,"SEX",3)) {
				strcat(str,"$S");
				i+=2; }
			else if(!strncmp(ar+i,"UDR",3)) {
				strcat(str,"$K");
				i+=2; }
			else if(!strncmp(ar+i,"DAY",3)) {
				strcat(str,"$W");
                i+=2; }
			else if(!strncmp(ar+i,"ANSI",4)) {
				strcat(str,"$[");
                i+=3; }
			else if(!strncmp(ar+i,"UDFR",4)) {
				strcat(str,"$D");
				i+=3; }
			else if(!strncmp(ar+i,"FLAG",4)) {
				strcat(str,"$F");
				i+=3; }
			else if(!strncmp(ar+i,"NODE",4)) {
				strcat(str,"$N");
				i+=3; }
			else if(!strncmp(ar+i,"NULL",4)) {
				strcat(str,"$0");
                i+=3; }
			else if(!strncmp(ar+i,"TIME",4)) {
				strcat(str,"$T");
				i+=3; }
			else if(!strncmp(ar+i,"USER",4)) {
				strcat(str,"$U");
				i+=3; }
			else if(!strncmp(ar+i,"REST",4)) {
				strcat(str,"$Z");
                i+=3; }
			else if(!strncmp(ar+i,"LOCAL",5)) {
				strcat(str,"$G");
				i+=4; }
			else if(!strncmp(ar+i,"LEVEL",5)) {
				strcat(str,"$L");
                i+=4; }
			else if(!strncmp(ar+i,"TLEFT",5)) {
				strcat(str,"$R");
				i+=4; }
			else if(!strncmp(ar+i,"TUSED",5)) {
				strcat(str,"$O");
				i+=4; }
			else if(!strncmp(ar+i,"EXPIRE",6)) {
				strcat(str,"$E");
				i+=5; }
			else if(!strncmp(ar+i,"CREDIT",6)) {
				strcat(str,"$C");
                i+=5; }
			else if(!strncmp(ar+i,"EXEMPT",6)) {
				strcat(str,"$X");
                i+=5; }
			else if(!strncmp(ar+i,"RANDOM",6)) {
				strcat(str,"$Q");
                i+=5; }
			else if(!strncmp(ar+i,"LASTON",6)) {
				strcat(str,"$Y");
                i+=5; }
			else if(!strncmp(ar+i,"LOGONS",6)) {
				strcat(str,"$V");
                i+=5; }
			else if(!strncmp(ar+i,":00",3)) {
				i+=2; }
			else
				strncat(str,ar+i,1); }
		strcpy(ar,str);
		len=strlen(ar); }
	if(len>=30) {				  /* Remove all spaces and &s */
		str[0]=0;
		n=strlen(ar);
		for(i=0;i<n;i++)
			if(ar[i]!=SP && ar[i]!='&')
				strncat(str,ar+i,1);
		strcpy(ar,str);
		len=strlen(ar); }
	i=0;
	sprintf(opt[i++],"Requirement String (%s)",ar);
	strcpy(opt[i++],"Clear Requirements");
	strcpy(opt[i++],"Set Required Level");
	strcpy(opt[i++],"Set Required Flag");
	strcpy(opt[i++],"Set Required Age");
	strcpy(opt[i++],"Set Required Sex");
	strcpy(opt[i++],"Set Required Connect Rate");
	strcpy(opt[i++],"Set Required Post/Call Ratio");
	strcpy(opt[i++],"Set Required Number of Credits");
	strcpy(opt[i++],"Set Required Upload/Download Ratio");
	strcpy(opt[i++],"Set Required Upload/Download File Ratio");
	strcpy(opt[i++],"Set Required Time of Day");
	strcpy(opt[i++],"Set Required Day of Week");
	strcpy(opt[i++],"Set Required Node Number");
	strcpy(opt[i++],"Set Required User Number");
	strcpy(opt[i++],"Set Required Time Remaining");
	strcpy(opt[i++],"Set Required Days Till Expiration");
	opt[i][0]=0;
	SETHELP(WHERE);
/*
Access Requirements:

This menu allows you to edit the access requirement string for the
selected feature/section of your BBS. You can edit the string
directly (see documentation for details) or use the Set Required...
options from this menu to automatically fill in the string for you.
*/
	sprintf(str,"%s Requirements",desc);
	switch(ulist(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&curar,0,str,opt)) {
		case -1:
			done=1;
			break;
		case 0:
			SETHELP(WHERE);
/*
Key word	Symbol		Description
컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
AND 		  & 		More than one requirement (optional)
NOT 		  ! 		Logical negation (i.e. NOT EQUAL)
EQUAL		  = 		Equality required
OR			  | 		Either of two or more parameters is required
AGE 		  $A		User's age (years since birthdate, 0-255)
BPS 		  $B		User's current connect rate (bps)
FLAG		  $F		User's flag (A-Z)
LEVEL		  $L		User's level (0-99)
NODE		  $N		Current node (1-250)
PCR 		  $P		User's post/call ratio (0-100)
SEX 		  $S		User's sex/gender (M or F)
TIME		  $T		Time of day (HH:MM, 00:00-23:59)
TLEFT		  $R		User's time left online (minutes, 0-255)
TUSED		  $O		User's time online this call (minutes, 0-255)
USER		  $U		User's number (1-xxxx)
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Requirement String",ar,LEN_ARSTR
                ,K_EDIT|K_UPPER);
			break;
		case 1:
			i=1;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			if(savdepth)
				savnum++;
			SETHELP(WHERE);
/*
Clear Requirements:

If you wish to clear the current requirement string, select Yes.
Otherwise, select No.
*/
			i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Are You Sure",opt);
			if(savdepth)
                savnum--;
			if(!i) {
				ar[0]=0;
				changes=1; }
			break;
		case 2:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Level:

You are being prompted to enter the security level to be used in this
requirement evaluation. The valid range is 0 (zero) through 99.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Level",str,2,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
					strcat(ar," OR "); }
			strcat(ar,"LEVEL ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
			break;
		case 3:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }

			for(i=0;i<4;i++)
				sprintf(opt[i],"Flag Set #%d",i+1);
			opt[i][0]=0;
			i=0;
			if(savdepth)
				savnum++;
			i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Select Flag Set",opt);
			if(savdepth)
				savnum--;
			if(i==-1)
                break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Flag:

You are being prompted to enter the security flag to be used in this
requirement evaluation. The valid range is A through Z.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Flag (A-Z)",str,1
				,K_UPPER|K_ALPHA);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"FLAG ");
			if(i)
				strcat(ar,itoa(i+1,tmp,10));
			strcat(ar,str);
			break;
		case 4:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Age:

You are being prompted to enter the user's age to be used in this
requirement evaluation. The valid range is 0 through 255.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Age",str,3,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"AGE ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 5:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			str[0]=0;
			SETHELP(WHERE);
/*
Required Sex:

You are being prompted to enter the user's gender to be used in this
requirement evaluation. The valid values are M or F (for male or
female).
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Sex (M or F)",str,1
				,K_UPPER|K_ALPHA);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
					strcat(ar," OR "); }
			strcat(ar,"SEX ");
			strcat(ar,str);
			break;
		case 6:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Connect Rate (BPS):

You are being prompted to enter the connect rate to be used in this
requirement evaluation. The valid range is 300 through 57600.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Connect Rate (BPS)",str,5,K_NUMBER);
			if(!str[0])
				break;
			j=atoi(str);
			if(j>=300 && j<30000) {
				j/=100;
				sprintf(str,"%d",j); }
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"BPS ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 7:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Post/Call Ratio:

You are being prompted to enter the post/call ratio to be used in this
requirement evaluation (percentage). The valid range is 0 through 100.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Post/Call Ratio (Percentage)"
				,str,3,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"PCR ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 8:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Number of Credits:

You are being prompted to enter the number of credits (in kilobytes) to
be used in this requirement evaluation. The valid range is 0 through
65535.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Required Credits",str,5,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"CREDIT ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 9:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Upload/Download Ratio:

You are being prompted to enter the upload/download ratio to be used in
this requirement evaluation (percentage). The valid range is 0 through
100. This ratio is based on the number of bytes uploaded by the user
divided by the number of bytes downloaded.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Upload/Download Ratio (Percentage)"
				,str,3,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"UDR ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 10:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Upload/Download File Ratio:

You are being prompted to enter the upload/download ratio to be used in
this requirement evaluation (percentage). The valid range is 0 through
100. This ratio is based on the number of files uploaded by the user
divided by the number of files downloaded.
*/
			uinput(WIN_MID|WIN_SAV,0,0
				,"Upload/Download File Ratio (Percentage)"
				,str,3,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"UDFR ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 11:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=0;
			strcpy(opt[0],"Before");
			strcpy(opt[1],"After");
			opt[2][0]=0;
			if(savdepth)
				savnum++;
			i=ulist(WIN_MID|WIN_SAV,0,0,0,&i,0,"Time Relationship",opt);
			if(savdepth)
				savnum--;
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Time of Day:

You are being prompted to enter the time of day to be used in this
requirement evaluation (24 hour HH:MM format). The valid range is 0
through 23:59.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Time of Day (HH:MM)",str,5,K_UPPER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"TIME ");
			if(!i)
				strcat(ar,"NOT ");
			strcat(ar,str);
			break;
		case 12:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			SETHELP(WHERE);
/*
Required Day of Week:

You are being prompted to select a day of the week as an access
requirement value.
*/
			for(n=0;n<7;n++)
				strcpy(opt[n],wday[n]);
			opt[n][0]=0;
			n=0;
			if(savdepth)
				savnum++;
			n=ulist(WIN_MID|WIN_SAV,0,0,0,&n,0,"Select Day of Week",opt);
			if(savdepth)
				savnum--;
			if(n==-1)
                break;
			strcpy(str,wday[n]);
			strupr(str);
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"DAY ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 13:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Node:

You are being prompted to enter the number of a node to be used in this
requirement evaluation. The valid range is 1 through 250.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Node Number",str,3,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"NODE ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
		case 14:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required User Number:

You are being prompted to enter the user's number to be used in this
requirement evaluation.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"User Number",str,5,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"USER ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;

		case 15:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Time Remaining:

You are being prompted to enter the time remaining to be used in this
requirement evaluation (in minutes). The valid range is 0 through 255.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Time Remaining (minutes)"
				,str,3,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"TLEFT ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
			break;

		case 16:
			if(strlen(ar)>=30) {
				umsg("Maximum string length reached");
                break; }
			i=whichlogic();
			if(i==-1)
				break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Days Till User Account Expiration:

You are being prompted to enter the required number of days till the
user's account will expire.
*/
			uinput(WIN_MID|WIN_SAV,0,0,"Days Till Expiration"
				,str,5,K_NUMBER);
			if(!str[0])
				break;
			if(ar[0]) {
				j=whichcond();
				if(j==-1)
					break;
				if(!j)
					strcat(ar," AND ");
				else
                    strcat(ar," OR "); }
			strcat(ar,"EXPIRE ");
			switch(i) {
				case 1:
					strcat(ar,"= ");
					break;
				case 2:
					strcat(ar,"NOT = ");
					break;
				case 3:
					strcat(ar,"NOT ");
					break; }
			strcat(ar,str);
            break;
			} }
sprintf(inar,"%.*s",LEN_ARSTR,ar);
}

char code_ok(char *str)
{

if(!strlen(str))
    return(0);
if(strcspn(str," \\/.|<>*?+[]:=\";,")!=strlen(str))
    return(0);
return(1);
}

/****************************************************************************/
/*                          Functions from MISC.C                           */
/****************************************************************************/

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.   All files are opened in BINARY mode.            */
/****************************************************************************/
int nopen(char *str, int access)
{
    int file,share,count=0;

if(access==O_RDONLY) share=O_DENYWRITE;
    else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
    && errno==EACCES && count++<LOOP_NOPEN);
if(file==-1 && errno==EACCES)
	cputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE *fnopen(int *file, char *str, int access)
{
	char mode[128];
	FILE *stream;

if(((*file)=nopen(str,access))==-1)
	return(NULL);

if(access&O_APPEND) {
	if(access&O_RDONLY)
		strcpy(mode,"a+");
	else
		strcpy(mode,"a"); }
else {
	if(access&O_WRONLY)
		strcpy(mode,"r+");
	else
		strcpy(mode,"r"); }
stream=fdopen((*file),mode);
if(stream==NULL) {
	close(*file);
	return(NULL); }
setvbuf(stream,NULL,_IOFBF,16*1024);
return(stream);
}


/****************************************************************************/
/* Converts an ASCII Hex string into an unsigned long                       */
/****************************************************************************/
unsigned long ahtoul(char *str)
{
    unsigned long l,val=0;

while((l=(*str++)|0x20)!=0x20)
    val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
}

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas. Maximum value of l is 4 gigabytes.                               */
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
    char str[81];
    char i,j;

ultoa(l,str,10);
if(!(strlen(str)%3)) i=(strlen(str)/3)-1;
    else i=strlen(str)/3;
j=strlen(str)+i;
string[j--]=0;
i=strlen(str)-1;
while(i!=-1) {
    if(!((strlen(str)-i)%3)) {
        string[j--]=str[i--];
        string[j--]=','; }
    else string[j--]=str[i--]; }
return(string);
}

/****************************************************************************/
/* If the directory 'path' doesn't exist, create it.                      	*/
/****************************************************************************/
void md(char *inpath)
{
	char path[256],str[128];
	struct ffblk ff;
	int curdisk,disk;

if(!inpath[0] || no_dirchk)
	return;
if(!strcmp(inpath+1,":\\")) {
	curdisk=getdisk();
	disk=toupper(inpath[0])-'A';
	setdisk(disk);
	if(getdisk()!=disk) {
		sprintf(str,"Invalid drive %c:",toupper(inpath[0]));
		umsg(str); }
	setdisk(curdisk);
	return; }
strcpy(path,inpath);
if(path[strlen(path)-1]=='\\')
	path[strlen(path)-1]=0;
if(!strcmp(path,"."))               /* Don't try to make '.' */
	return;
if(findfirst(path,&ff,FA_DIREC))
	if(mkdir(path)) {
		sprintf(str,"Failed to create %s",path);
		umsg(str); }
}


void bail(int code)
{
	char		str[256];
	int 		i,x;
	smbstatus_t status;

if(code) {
	cputs("\r\nHit a key...");
	getch(); }
else if(forcesave) {
	upop("Loading Configs...");
	read_main_cfg(txt);
	read_msgs_cfg(txt);
	read_file_cfg(txt);
	read_chat_cfg(txt);
	read_xtrn_cfg(txt);
	upop(0);
	write_main_cfg();
	write_msgs_cfg();
	write_file_cfg();
	write_chat_cfg();
	write_xtrn_cfg(); }

uifcbail();

exit(code);
}

/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG in the text dir.     */
/****************************************************************************/
void errormsg(int line, char *source,  char action, char *object, ulong access)
{
	char scrn_buf[8000];
    char actstr[256];

gettext(1,1,80,scrn_len,scrn_buf);
clrscr();
switch(action) {
    case ERR_OPEN:
        strcpy(actstr,"opening");
        break;
    case ERR_CLOSE:
        strcpy(actstr,"closeing");
        break;
    case ERR_FDOPEN:
        strcpy(actstr,"fdopen");
        break;
    case ERR_READ:
        strcpy(actstr,"reading");
        break;
    case ERR_WRITE:
        strcpy(actstr,"writing");
        break;
    case ERR_REMOVE:
        strcpy(actstr,"removing");
        break;
    case ERR_ALLOC:
        strcpy(actstr,"allocating memory");
        break;
    case ERR_CHK:
        strcpy(actstr,"checking");
        break;
    case ERR_LEN:
        strcpy(actstr,"checking length");
        break;
    case ERR_EXEC:
        strcpy(actstr,"executing");
        break;
    default:
        strcpy(actstr,"UNKNOWN"); }
cprintf("ERROR -     line: %d\r\n",line);
cprintf("            file: %s\r\n",source);
cprintf("          action: %s\r\n",actstr);
cprintf("          object: %s\r\n",object);
cprintf("          access: %ld (%lx)\r\n",access,access);
cputs("\r\n<Hit any key>");
getch();
puttext(1,1,80,scrn_len,scrn_buf);
}

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

i=strlen(str);
if(i && str[i-1]!='\\') {
    str[i]='\\'; str[i+1]=0; }
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/* Called from upload                                                       */
/****************************************************************************/
char fexist(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==0)
    return(1);
return(0);
}

/* End of SCFG.C */
