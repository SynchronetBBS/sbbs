/* scfg.c */

/* Synchronet configuration utility 										*/

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/********************/
/* Global Variables */
/********************/

scfg_t	cfg;    /* Synchronet Configuration */
uifcapi_t uifc; /* User Interface (UIFC) Library API */

BOOL  no_dirchk=FALSE,forcesave=FALSE;
extern BOOL all_msghdr;
extern BOOL no_msghdr;
char **opt;
char tmp[256];
char error[256];
int  backup_level=5;

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
    printf("\7Error allocating %u bytes of memory.\r\n",size);
    bail(1);
}

int main(int argc, char **argv)
{
	char	**mopt,*p;
    char    errormsg[MAX_PATH*2];
	int 	i,j,main_dflt=0,chat_dflt=0;
	char 	str[129];
 	char	exepath[MAX_PATH+1];
	BOOL    door_mode=FALSE;

    printf("\r\nSynchronet Configuration Utility (%s)  v%s  Copyright 2002 "
        "Rob Swindell\r\n",PLATFORM_DESC,VERSION);

    memset(&uifc,0,sizeof(uifc));
    p=getenv("SBBSCTRL");
    if(p!=NULL)
        SAFECOPY(cfg.ctrl_dir,p);
    else
        getcwd(cfg.ctrl_dir,sizeof(cfg.ctrl_dir));

	uifc.esc_delay=25;

	for(i=1;i<argc;i++) {
        if(argv[i][0]=='-'
#ifndef __unix__
            || argv[i][0]=='/'
#endif
            )
            switch(toupper(argv[i][1])) {
		        case 'M':   /* Show free mem */
                case 'N':   /* No EMS */
                case 'T':   /* Windows/OS2 time slice API */
        			/* do nothing */
                    continue;
                case 'C':
        			uifc.mode|=UIFC_COLOR;
                    break;
                case 'D':
                    door_mode=TRUE;
                    break;
                case 'B':
        			backup_level=atoi(argv[i]+2);
                    break;
                case 'S':
        			no_dirchk=!no_dirchk;
                    break;
                case 'H':
        			no_msghdr=!no_msghdr;
                    break;
                case 'U':
        			all_msghdr=!all_msghdr;
                    break;
                case 'F':
                	forcesave=!forcesave;
                    break;
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
                    break;
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
                    break;
				case 'I':
					uifc.mode|=UIFC_IBM;
					break;
                case 'V':
#if !defined(__unix__)
                    textmode(atoi(argv[i]+2));
#endif
                    break;
                default:
                    printf("\nusage: scfg [ctrl_dir] [options]"
                        "\n\noptions:\n\n"
                        "-s  =  don't check directories\r\n"
                        "-f  =  force save of config files\r\n"
                        "-u  =  update all message base status headers\r\n"
                        "-h  =  don't update message base status headers\r\n"
                        "-d  =  run in standard input/output/door mode\r\n"
                        "-c  =  force color mode\r\n"
#ifdef USE_CURSES
                        "-e# =  set escape delay to #msec\r\n"
						"-i  =  force IBM charset\r\n"
#endif
#if !defined(__unix__)
                        "-v# =  set video mode to #\r\n"
#endif
                        "-l# =  set screen lines to #\r\n"
                        "-b# =  set automatic back-up level (default=3 max=10)\r\n"
                        );
        			exit(0);
           }
        else
            SAFECOPY(cfg.ctrl_dir,argv[i]);
    }

backslashcolon(cfg.ctrl_dir);

FULLPATH(exepath,argv[0],sizeof(exepath));	/* Must do this before chdir */

if(chdir(cfg.ctrl_dir)!=0) {
	printf("!ERROR %d changing current directory to: %s\n"
		,errno,cfg.ctrl_dir);
	exit(-1);
}

uifc.size=sizeof(uifc);
#if defined(USE_DIALOG)
if(!door_mode)
    i=uifcinid(&uifc);  /* dialog */
else
#elif defined(USE_CURSES)
if(!door_mode)
    i=uifcinic(&uifc);  /* curses */
else
#elif !defined(__unix__)
if(!door_mode)
    i=uifcini(&uifc);   /* conio */
else
#endif
    i=uifcinix(&uifc);  /* stdio */
if(i!=0) {
    printf("uifc library init returned error %d\n",i);
    exit(1);
}

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

if((p=getenv("SBBSEXEC"))!=NULL)
	SAFECOPY(str,p);
else {
	SAFECOPY(str,exepath);
	p=strrchr(str,'/');
	if(p==NULL)
	    p=strrchr(str,'\\');
	if(p!=NULL)
		*p=0;
	else 
	   	sprintf(str,"%s../exec",cfg.ctrl_dir);
}
sprintf(uifc.helpdatfile,"%s/scfghelp.dat",str);
sprintf(uifc.helpixbfile,"%s/scfghelp.ixb",str);

sprintf(str,"Synchronet for %s v%s",PLATFORM_DESC,VERSION);
if(uifc.scrn(str)) {
	printf(" USCRN (len=%d) failed!\r\n",uifc.scrn_len+1);
	bail(1);
}

if(!fexist(uifc.helpdatfile)) {
    sprintf(errormsg,"Help file (%s) missing!",uifc.helpdatfile);
    uifc.msg(errormsg);
}
if(!fexist(uifc.helpixbfile)) {
    sprintf(errormsg,"Help file (%s) missing!",uifc.helpixbfile);
    uifc.msg(errormsg);
}
sprintf(str,"%smain.cnf",cfg.ctrl_dir);
if(!fexist(str)) {
    sprintf(errormsg,"Main configuration file (%s) missing!",str);
    uifc.msg(errormsg);
}

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
while(1) {
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
	switch(uifc.list(WIN_ORG|WIN_MID|WIN_ESC|WIN_ACT,0,0,30,&main_dflt,0
		,"Configure",mopt)) {
		case 0:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			node_menu();
			free_main_cfg(&cfg);
			break;
		case 1:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop("Reading XTRN.CNF...");
			if(!read_xtrn_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			sys_cfg();
			free_xtrn_cfg(&cfg);
			free_main_cfg(&cfg);
			break;
		case 2:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop("Reading MSGS.CNF...");
			if(!read_msgs_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			net_cfg();
			free_msgs_cfg(&cfg);
			free_main_cfg(&cfg);
			break;
		case 3:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
            uifc.pop("Reading FILE.CNF...");
            if(!read_file_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}	
			uifc.pop(0);
            xfer_cfg();
            free_file_cfg(&cfg);
			free_main_cfg(&cfg);
            break;
		case 4:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
            uifc.pop("Reading FILE.CNF...");
            if(!read_file_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			xfer_opts();
            free_file_cfg(&cfg);
			free_main_cfg(&cfg);
            break;
		case 5:
            uifc.pop("Reading CHAT.CNF...");
            if(!read_chat_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}	
			uifc.pop(0);
            while(1) {
                i=0;
                strcpy(opt[i++],"Artificial Gurus");
                strcpy(opt[i++],"Multinode Chat Actions");
                strcpy(opt[i++],"Multinode Chat Channels");
                strcpy(opt[i++],"External Sysop Chat Pagers");
				opt[i][0]=0;
                j=uifc.list(WIN_ORG|WIN_ACT|WIN_CHE,0,0,0,&chat_dflt,0
                    ,"Chat Features",opt);
                if(j==-1) {
                    j=save_changes(WIN_MID);
                    if(j==-1)
                        continue;
                    if(!j) {
                        write_chat_cfg(&cfg,backup_level);
                        rerun_nodes();
                    }
                    break;
                }
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
            free_chat_cfg(&cfg);
            break;
		case 6:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop("Reading MSGS.CNF...");
			if(!read_msgs_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			msgs_cfg();
			free_msgs_cfg(&cfg);
			free_main_cfg(&cfg);
			break;
		case 7:
			uifc.pop("Reading MAIN.CNF...");
            if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop("Reading MSGS.CNF...");
            if(!read_msgs_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			msg_opts();
			free_msgs_cfg(&cfg);
			free_main_cfg(&cfg);
			break;
		case 8:
			uifc.pop("Reading MAIN.CNF...");
			if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			shell_cfg();
			free_main_cfg(&cfg);
            break;
		case 9:
			uifc.pop("Reading MAIN.CNF...");
            if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop("Reading XTRN.CNF...");
			if(!read_xtrn_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
			xprogs_cfg();
			free_xtrn_cfg(&cfg);
			free_main_cfg(&cfg);
			break;
		case 10:
			uifc.pop("Reading MAIN.CNF...");
            if(!read_main_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
            uifc.pop("Reading FILE.CNF...");
            if(!read_file_cfg(&cfg,error)) {
				uifc.pop(0);
				sprintf(errormsg,"ERROR: %s",error);
				uifc.msg(errormsg);
				break;
			}
			uifc.pop(0);
            txt_cfg();
            free_file_cfg(&cfg);
			free_main_cfg(&cfg);
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
			i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit SCFG",opt);
			if(!i)
				bail(0);
			break; } }
}

/****************************************************************************/
/* Checks the uifc.changes variable. If there have been no uifc.changes, returns 2.	*/
/* If there have been uifc.changes, it prompts the user to change or not. If the */
/* user escapes the menu, returns -1, selects Yes, 0, and selects no, 1 	*/
/****************************************************************************/
int save_changes(int mode)
{
	int i=0;

if(!uifc.changes)
	return(2);
strcpy(opt[0],"Yes");
strcpy(opt[1],"No");
opt[2][0]=0;
if(mode&WIN_SAV && uifc.savdepth)
	uifc.savnum++;
SETHELP(WHERE);
/*
Save uifc.changes:

You have made some uifc.changes to the configuration. If you want to save
these uifc.changes, select Yes. If you are positive you DO NOT want to save
these uifc.changes, select No. If you are not sure and want to review the
configuration before deciding, hit  ESC .
*/
i=uifc.list(mode|WIN_ACT,0,0,0,&i,0,"Save Changes",opt);
if(mode&WIN_SAV && uifc.savdepth && uifc.savnum)
	uifc.savnum--;
if(i!=-1)
	uifc.changes=0;
return(i);
}

void rerun_nodes()
{
	char	str[MAX_PATH+1];
    int		i;
	int		file;
    node_t	node;
    
    for(i=0;i<cfg.sys_nodes;i++) {
       	if(getnodedat(&cfg,i+1,&node,&file))
            break;
        node.misc|=NODE_RRUN;
        if(putnodedat(&cfg,i+1,&node,file))
            break;
    }

	sprintf(str,"%sftpsrvr.rec",cfg.ctrl_dir);
	if((file=open(str,O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE|S_IREAD))!=-1)
		close(file);
	sprintf(str,"%smailsrvr.rec",cfg.ctrl_dir);
	if((file=open(str,O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE|S_IREAD))!=-1)
		close(file);
	sprintf(str,"%sservices.rec",cfg.ctrl_dir);
	if((file=open(str,O_WRONLY|O_CREAT|O_TRUNC,S_IWRITE|S_IREAD))!=-1)
		close(file);
}

void txt_cfg()
{
	static int txt_dflt,bar;
	char str[81],code[9],done=0,*p;
	int j,k;
	uint i;
	static txtsec_t savtxtsec;

while(1) {
	for(i=0;i<cfg.total_txtsecs;i++)
		sprintf(opt[i],"%-25s",cfg.txtsec[i]->name);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_CHE;
	if(cfg.total_txtsecs)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_txtsecs<MAX_OPTS)
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
	i=uifc.list(j,0,0,45,&txt_dflt,&bar,"Text File Sections",opt);
	if((signed)i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
			continue;
		if(!j) {
			write_file_cfg(&cfg,backup_level);
            rerun_nodes();
        }
		return;
    }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"ANSI Artwork");
		SETHELP(WHERE);
/*
Text Section Name:

This is the name of this text section.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Text Section Name",str,40
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
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Text Section Internal Code",code,8
			,K_EDIT)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }
		if((cfg.txtsec=(txtsec_t **)REALLOC(cfg.txtsec
			,sizeof(txtsec_t *)*(cfg.total_txtsecs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_txtsecs+1);
			cfg.total_txtsecs=0;
			bail(1);
			continue; }
		if(cfg.total_txtsecs)
			for(j=cfg.total_txtsecs;j>i;j--)
                cfg.txtsec[j]=cfg.txtsec[j-1];
		if((cfg.txtsec[i]=(txtsec_t *)MALLOC(sizeof(txtsec_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(txtsec_t));
			continue; }
		memset((txtsec_t *)cfg.txtsec[i],0,sizeof(txtsec_t));
		strcpy(cfg.txtsec[i]->name,str);
		strcpy(cfg.txtsec[i]->code,code);
		cfg.total_txtsecs++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.txtsec[i]);
		cfg.total_txtsecs--;
		for(j=i;j<cfg.total_txtsecs;j++)
			cfg.txtsec[j]=cfg.txtsec[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savtxtsec=*cfg.txtsec[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.txtsec[i]=savtxtsec;
		uifc.changes=1;
        continue; }
	i=txt_dflt;
	j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",cfg.txtsec[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Access Requirements"
			,cfg.txtsec[i]->arstr);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",cfg.txtsec[i]->code);
		opt[k][0]=0;
		switch(uifc.list(WIN_ACT|WIN_MID,0,0,60,&j,0,cfg.txtsec[i]->name
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
				strcpy(str,cfg.txtsec[i]->name);	/* save */
				if(!uifc.input(WIN_MID|WIN_SAV,0,10
					,"Text File Section Name"
					,cfg.txtsec[i]->name,40,K_EDIT))
					strcpy(cfg.txtsec[i]->name,str);
				break;
			case 1:
				sprintf(str,"%s Text Section",cfg.txtsec[i]->name);
				getar(str,cfg.txtsec[i]->arstr);
				break;
			case 2:
				strcpy(str,cfg.txtsec[i]->code);
				SETHELP(WHERE);
/*
Text Section Internal Code:

Every text file section must have its own unique internal code for
Synchronet to reference it by. It is helpful if this code is an
abreviation of the name.
*/
				uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT);
				if(code_ok(str))
					strcpy(cfg.txtsec[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
					uifc.helpbuf=0; }
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
	for(i=0;i<cfg.total_shells;i++)
		sprintf(opt[i],"%-25s",cfg.shell[i]->name);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_CHE;
	if(cfg.total_shells)
		j|=WIN_DEL|WIN_GET;
	if(cfg.total_shells<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(savshell.name[0])
		j|=WIN_PUT;
	SETHELP(WHERE);
/*
Command Shells:

This is a list of Command Shells configured for your system.
Command shells are the programmable command and menu structures which
are available for your BBS.

To add a command shell section, select the desired location with the
arrow keys and hit  INS .

To delete a command shell, select it and hit  DEL .

To configure a command shell, select it and hit  ENTER .
*/
	i=uifc.list(j,0,0,45,&shell_dflt,&shell_bar,"Command Shells",opt);
	if((signed)i==-1) {
		j=save_changes(WIN_MID);
		if(j==-1)
			continue;
		if(!j) {
			write_main_cfg(&cfg,backup_level);
            rerun_nodes();
        }
		return;
    }
	if((i&MSK_ON)==MSK_INS) {
		i&=MSK_OFF;
		strcpy(str,"Menu Shell");
		SETHELP(WHERE);
/*
Command Shell Name:

This is the descriptive name of this command shell.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Command Shell Name",str,40
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
		if(uifc.input(WIN_MID|WIN_SAV,0,0,"Command Shell Internal Code",code,8
			,K_EDIT)<1)
			continue;
		if(!code_ok(code)) {
			uifc.helpbuf=invalid_code;
			uifc.msg("Invalid Code");
			uifc.helpbuf=0;
            continue; }
		if((cfg.shell=(shell_t **)REALLOC(cfg.shell
			,sizeof(shell_t *)*(cfg.total_shells+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.total_shells+1);
			cfg.total_shells=0;
			bail(1);
			continue; }
		if(cfg.total_shells)
			for(j=cfg.total_shells;j>i;j--)
				cfg.shell[j]=cfg.shell[j-1];
		if((cfg.shell[i]=(shell_t *)MALLOC(sizeof(shell_t)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,sizeof(shell_t));
			continue; }
		memset((shell_t *)cfg.shell[i],0,sizeof(shell_t));
		strcpy(cfg.shell[i]->name,str);
		strcpy(cfg.shell[i]->code,code);
		cfg.total_shells++;
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_DEL) {
		i&=MSK_OFF;
		FREE(cfg.shell[i]);
		cfg.total_shells--;
		for(j=i;j<cfg.total_shells;j++)
			cfg.shell[j]=cfg.shell[j+1];
		uifc.changes=1;
		continue; }
	if((i&MSK_ON)==MSK_GET) {
		i&=MSK_OFF;
		savshell=*cfg.shell[i];
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		*cfg.shell[i]=savshell;
		uifc.changes=1;
        continue; }
	i=shell_dflt;
	j=0;
	done=0;
	while(!done) {
		k=0;
		sprintf(opt[k++],"%-27.27s%s","Name",cfg.shell[i]->name);
		sprintf(opt[k++],"%-27.27s%s","Access Requirements"
			,cfg.shell[i]->arstr);
		sprintf(opt[k++],"%-27.27s%s","Internal Code",cfg.shell[i]->code);
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
		switch(uifc.list(WIN_ACT|WIN_MID,0,0,60,&j,0,cfg.shell[i]->name
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
				strcpy(str,cfg.shell[i]->name);    /* save */
				if(!uifc.input(WIN_MID|WIN_SAV,0,10
					,"Command Shell Name"
					,cfg.shell[i]->name,40,K_EDIT))
					strcpy(cfg.shell[i]->name,str);
				break;
			case 1:
				sprintf(str,"%s Command Shell",cfg.shell[i]->name);
				getar(str,cfg.shell[i]->arstr);
				break;
			case 2:
				strcpy(str,cfg.shell[i]->code);
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
				uifc.input(WIN_MID|WIN_SAV,0,17,"Internal Code (unique)"
					,str,8,K_EDIT);
				if(code_ok(str))
					strcpy(cfg.shell[i]->code,str);
				else {
					uifc.helpbuf=invalid_code;
					uifc.msg("Invalid Code");
					uifc.helpbuf=0; }
				break; } } }
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
if(uifc.savdepth)
	uifc.savnum++;
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
i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Select Logic",opt);
if(uifc.savdepth && uifc.savnum)
	uifc.savnum--;
return(i);
}

int whichcond()
{
	int i;

i=0;
strcpy(opt[0],"AND (Both/All)");
strcpy(opt[1],"OR  (Either/Any)");
opt[2][0]=0;
if(uifc.savdepth)
	uifc.savnum++;
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
i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Multiple Requirement Logic",opt);
if(uifc.savdepth && uifc.savnum)
	uifc.savnum--;
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
                    strcat(str,ultoa(j,tmp,10));
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
	switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,60,&curar,0,str,opt)) {
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Requirement String",ar,LEN_ARSTR
                ,K_EDIT|K_UPPER);
			break;
		case 1:
			i=1;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			if(uifc.savdepth)
				uifc.savnum++;
			SETHELP(WHERE);
/*
Clear Requirements:

If you wish to clear the current requirement string, select Yes.
Otherwise, select No.
*/
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Are You Sure",opt);
			if(uifc.savdepth && uifc.savnum)
                uifc.savnum--;
			if(!i) {
				ar[0]=0;
				uifc.changes=1; }
			break;
		case 2:
			if(strlen(ar)>=30) {
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Level",str,2,K_NUMBER);
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
				uifc.msg("Maximum string length reached");
                break; }

			for(i=0;i<4;i++)
				sprintf(opt[i],"Flag Set #%d",i+1);
			opt[i][0]=0;
			i=0;
			if(uifc.savdepth)
				uifc.savnum++;
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Select Flag Set",opt);
			if(uifc.savdepth && uifc.savnum)
				uifc.savnum--;
			if(i==-1)
                break;
			str[0]=0;
			SETHELP(WHERE);
/*
Required Flag:

You are being prompted to enter the security flag to be used in this
requirement evaluation. The valid range is A through Z.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Flag (A-Z)",str,1
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
				strcat(ar,ultoa(i+1,tmp,10));
			strcat(ar,str);
			break;
		case 4:
			if(strlen(ar)>=30) {
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Age",str,3,K_NUMBER);
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
				uifc.msg("Maximum string length reached");
                break; }
			str[0]=0;
			SETHELP(WHERE);
/*
Required Sex:

You are being prompted to enter the user's gender to be used in this
requirement evaluation. The valid values are M or F (for male or
female).
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Sex (M or F)",str,1
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Connect Rate (BPS)",str,5,K_NUMBER);
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Post/Call Ratio (Percentage)"
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Required Credits",str,5,K_NUMBER);
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Upload/Download Ratio (Percentage)"
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0
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
				uifc.msg("Maximum string length reached");
                break; }
			i=0;
			strcpy(opt[0],"Before");
			strcpy(opt[1],"After");
			opt[2][0]=0;
			if(uifc.savdepth)
				uifc.savnum++;
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Time Relationship",opt);
			if(uifc.savdepth && uifc.savnum)
				uifc.savnum--;
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Time of Day (HH:MM)",str,5,K_UPPER);
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
				uifc.msg("Maximum string length reached");
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
			if(uifc.savdepth)
				uifc.savnum++;
			n=uifc.list(WIN_MID|WIN_SAV,0,0,0,&n,0,"Select Day of Week",opt);
			if(uifc.savdepth && uifc.savnum)
				uifc.savnum--;
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Node Number",str,3,K_NUMBER);
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"User Number",str,5,K_NUMBER);
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Time Remaining (minutes)"
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
				uifc.msg("Maximum string length reached");
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
			uifc.input(WIN_MID|WIN_SAV,0,0,"Days Till Expiration"
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

int lprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    strip_ctrl(sbuf);
    uifc.msg(sbuf);
    return(0);
}

void bail(int code)
{
    if(code) {
        puts("\nHit a key...");
        getch(); }
    else if(forcesave) {
        uifc.pop("Loading Configs...");
        read_main_cfg(&cfg,error);
        read_msgs_cfg(&cfg,error);
        read_file_cfg(&cfg,error);
        read_chat_cfg(&cfg,error);
        read_xtrn_cfg(&cfg,error);
        uifc.pop(0);
        write_main_cfg(&cfg,backup_level);
        write_msgs_cfg(&cfg,backup_level);
        write_file_cfg(&cfg,backup_level);
        write_chat_cfg(&cfg,backup_level);
        write_xtrn_cfg(&cfg,backup_level); }

    uifc.bail();

    exit(code);
}

/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG in the text dir.     */
/****************************************************************************/
void errormsg(int line, char *source,  char action, char *object, ulong access)
{
    char actstr[256];

#if !defined(__unix__)
	char scrn_buf[MAX_BFLN];
    gettext(1,1,80,uifc.scrn_len,scrn_buf);
    clrscr();
#endif

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
    printf("ERROR -     line: %d\n",line);
    printf("            file: %s\n",source);
    printf("          action: %s\n",actstr);
    printf("          object: %s\n",object);
    printf("          access: %ld (%lx)\n",access,access);
    puts("\r\n<Hit any key>");
    getch();
#if !defined(__unix__)
    puttext(1,1,80,uifc.scrn_len,scrn_buf);
#endif    
}

/* End of SCFG.C */
