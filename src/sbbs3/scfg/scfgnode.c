/* scfgnode.c */

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

void node_menu()
{
	char	str[81],savnode=0;
	int 	i,j;
	static int node_menu_dflt, node_bar;

while(1) {
	for(i=0;i<cfg.sys_nodes;i++)
		sprintf(opt[i],"Node %d",i+1);
	opt[i][0]=0;
	j=WIN_ORG|WIN_ACT|WIN_INSACT|WIN_DELACT;
	if(cfg.sys_nodes>1)
		j|=WIN_DEL|WIN_GET;
	if(cfg.sys_nodes<MAX_NODES && cfg.sys_nodes<MAX_OPTS)
		j|=WIN_INS;
	if(savnode)
		j|=WIN_PUT;
SETHELP(WHERE);
/*
Node List:

This is the list of configured nodes in your system.

To add a node, hit  INS .

To delete a node, hit  DEL .

To configure a node, select it using the arrow keys and hit  ENTER .

To copy a node's configuration to another node, first select the source
node with the arrow keys and hit  F5 . Then select the destination
node and hit  F6 .
*/

	i=uifc.list(j,0,0,13,&node_menu_dflt,&node_bar,"Nodes",opt);
	if(i==-1) {
		if(savnode) {
			free_node_cfg(&cfg);
			savnode=0; }
		return; }

	if((i&MSK_ON)==MSK_DEL) {
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		sprintf(str,"Delete Node %d",cfg.sys_nodes);
		i=1;
SETHELP(WHERE);
/*
Delete Node:

If you are positive you want to delete this node, select Yes. Otherwise,
select No or hit  ESC .
*/
		i=uifc.list(WIN_MID,0,0,0,&i,0,str,opt);
		if(!i) {
			--cfg.sys_nodes;
/*			FREE(cfg.node_path[cfg.sys_nodes]); */
			write_main_cfg(&cfg,backup_level);
            refresh_cfg(&cfg);
        }
		continue; }
	if((i&MSK_ON)==MSK_INS) {
		strcpy(cfg.node_dir,cfg.node_path[cfg.sys_nodes-1]);
		i=cfg.sys_nodes+1;
		uifc.pop("Reading NODE.CNF...");
		read_node_cfg(&cfg,error);
		uifc.pop(0);
		sprintf(str,"../node%d/",i);
		sprintf(tmp,"Node %d Path",i);
SETHELP(WHERE);
/*
Node Path:

This is the path to this node's private directory where its separate
configuration and data files are stored.

The drive and directory of this path can be set to any valid DOS
directory that can be accessed by ALL nodes and MUST NOT be on a RAM disk
or other volatile media.

If you want to abort the creation of this new node, hit  ESC .
*/
		j=uifc.input(WIN_MID,0,0,tmp,str,50,K_EDIT);
		uifc.changes=0;
		if(j<2)
			continue;
		truncsp(str);
		strcpy(cfg.node_path[i-1],str);
		if(str[strlen(str)-1]=='\\' || str[strlen(str)-1]=='/')
			str[strlen(str)-1]=0;
		MKDIR(str);
		cfg.node_num=++cfg.sys_nodes;
		sprintf(cfg.node_name,"Node %u",cfg.node_num);
		write_node_cfg(&cfg,backup_level);
		write_main_cfg(&cfg,backup_level);
		free_node_cfg(&cfg);
        refresh_cfg(&cfg);
		continue;
    }
	if((i&MSK_ON)==MSK_GET) {
		if(savnode)
			free_node_cfg(&cfg);
		i&=MSK_OFF;
		strcpy(cfg.node_dir,cfg.node_path[i]);
		uifc.pop("Reading NODE.CNF...");
		read_node_cfg(&cfg,error);
		uifc.pop(0);
		savnode=1;
		continue; }
	if((i&MSK_ON)==MSK_PUT) {
		i&=MSK_OFF;
		strcpy(cfg.node_dir,cfg.node_path[i]);
		cfg.node_num=i+1;
		write_node_cfg(&cfg,backup_level);
        refresh_cfg(&cfg);
		uifc.changes=1;
		continue;
    }

	if(savnode) {
		free_node_cfg(&cfg);
		savnode=0; }
	strcpy(cfg.node_dir,cfg.node_path[i]);
	prep_dir(cfg.ctrl_dir, cfg.node_dir, sizeof(cfg.node_dir));

	uifc.pop("Reading NODE.CNF...");
	read_node_cfg(&cfg,error);
	uifc.pop(0);
	if(cfg.node_num!=i+1) { 	/* Node number isn't right? */
		cfg.node_num=i+1;		/* so fix it */
		write_node_cfg(&cfg,backup_level); } /* and write it back */
	node_cfg();

	free_node_cfg(&cfg); }
}

void node_cfg()
{
	static	int node_dflt;
	char	done,str[81];
	static	int adv_dflt,tog_dflt,tog_bar;
	int 	i;

while(1) {
	i=0;
	sprintf(opt[i++],"%-27.27s%.40s","Logon Requirements",cfg.node_arstr);
	strcpy(opt[i++],"Toggle Options...");
	strcpy(opt[i++],"Advanced Options...");
	opt[i][0]=0;
	sprintf(str,"Node %d Configuration",cfg.node_num);
SETHELP(WHERE);
/*
Node Configuration Menu:

This is the node configuration menu. The options available from this
menu will only affect the selected node's configuration.

Options with a trailing ... will produce a sub-menu of more options.
*/
	switch(uifc.list(WIN_ACT|WIN_CHE|WIN_BOT|WIN_RHT,0,0,60,&node_dflt,0
		,str,opt)) {
		case -1:
			i=save_changes(WIN_MID|WIN_SAV);
			if(!i) {
				write_node_cfg(&cfg,backup_level);
                refresh_cfg(&cfg);
            }
			if(i!=-1)
				return;
			break;
		case 0:
			sprintf(str,"Node %u Logon",cfg.node_num);
			getar(str,cfg.node_arstr);
			break;
		case 1:
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","Low Priority String Input"
					,cfg.node_misc&NM_LOWPRIO ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Allow Login by User Number"
					,cfg.node_misc&NM_NO_NUM ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","Allow Login by Real Name"
					,cfg.node_misc&NM_LOGON_R ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Always Prompt for Password"
					,cfg.node_misc&NM_LOGON_P ? "Yes":"No");
				sprintf(opt[i++],"%-27.27s%s","Allow 8-bit Remote Logins"
					,cfg.node_misc&NM_7BITONLY ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","Spinning Pause Prompt"
					,cfg.node_misc&NM_NOPAUSESPIN ? "No":"Yes");
				sprintf(opt[i++],"%-27.27s%s","Keep Node File Open"
					,cfg.node_misc&NM_CLOSENODEDAB ? "No":"Yes");

				opt[i][0]=0;
				uifc.savnum=0;
SETHELP(WHERE);
/*
Node Toggle Options:

This is the toggle options menu for the selected node's configuration.

The available options from this menu can all be toggled between two or
more states, such as Yes and No.
*/
				switch(uifc.list(WIN_BOT|WIN_RHT|WIN_ACT|WIN_SAV,3,2,35,&tog_dflt
					,&tog_bar,"Toggle Options",opt)) {
					case -1:
						done=1;
						break;
					case 0:
						i=cfg.node_misc&NM_LOWPRIO ? 0:1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Low Priority String Input:

Normally, Synchronet will not give up time slices (under a multitasker)
when users are prompted for a string of characters. This is considered
a high priority task.

Setting this option to Yes will force Synchronet to give up time slices
during string input, possibly causing jerky keyboard input from the
user, but improving aggregate system performance under multitaskers.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Low Priority String Input",opt);
						if(i==0 && !(cfg.node_misc&NM_LOWPRIO)) {
							cfg.node_misc|=NM_LOWPRIO;
							uifc.changes=1; }
						else if(i==1 && (cfg.node_misc&NM_LOWPRIO)) {
							cfg.node_misc&=~NM_LOWPRIO;
							uifc.changes=1; }
                        break;
					case 1:
						i=cfg.node_misc&NM_NO_NUM ? 1:0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Allow Login by User Number:

If you want users to be able login using their user number at the NN:
set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow Login by User Number",opt);
						if(i==0 && cfg.node_misc&NM_NO_NUM) {
							cfg.node_misc&=~NM_NO_NUM;
							uifc.changes=1; }
						else if(i==1 && !(cfg.node_misc&NM_NO_NUM)) {
							cfg.node_misc|=NM_NO_NUM;
							uifc.changes=1; }
                        break;
					case 2:
						i=cfg.node_misc&NM_LOGON_R ? 0:1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Allow Login by Real Name:

If you want users to be able login using their real name as well as
their alias, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow Login by Real Name",opt);
						if(i==0 && !(cfg.node_misc&NM_LOGON_R)) {
							cfg.node_misc|=NM_LOGON_R;
							uifc.changes=1; }
						else if(i==1 && (cfg.node_misc&NM_LOGON_R)) {
							cfg.node_misc&=~NM_LOGON_R;
							uifc.changes=1; }
                        break;
					case 3:
						i=cfg.node_misc&NM_LOGON_P ? 0:1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Always Prompt for Password:

If you want to have attempted logins using an unknown user name still
prompt for a password, set this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Always Prompt for Password",opt);
						if(i==0 && !(cfg.node_misc&NM_LOGON_P)) {
							cfg.node_misc|=NM_LOGON_P;
							uifc.changes=1; }
						else if(i==1 && (cfg.node_misc&NM_LOGON_P)) {
							cfg.node_misc&=~NM_LOGON_P;
							uifc.changes=1; }
                        break;
					case 4:
						i=cfg.node_misc&NM_7BITONLY ? 0:1;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Allow 8-bit Remote Input During Login:

If you wish to allow E-7-1 terminals to use this node, you must set this
option to No. This will also eliminate the ability of 8-bit remote users
to send IBM extended ASCII characters during the login sequence.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Allow 8-bit Remote Input During Login",opt);
						if(i==1 && !(cfg.node_misc&NM_7BITONLY)) {
							cfg.node_misc|=NM_7BITONLY;
							uifc.changes=1; }
						else if(i==0 && (cfg.node_misc&NM_7BITONLY)) {
							cfg.node_misc&=~NM_7BITONLY;
							uifc.changes=1; }
                        break;
					case 5:
						i=cfg.node_misc&NM_NOPAUSESPIN ? 1:0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Spinning Pause Prompt:

If you want to have a spinning cursor at the [Hit a key] prompt, set
this option to Yes.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Spinning Cursor at Pause Prompt",opt);
						if(i==0 && cfg.node_misc&NM_NOPAUSESPIN) {
							cfg.node_misc&=~NM_NOPAUSESPIN;
							uifc.changes=1; }
						else if(i==1 && !(cfg.node_misc&NM_NOPAUSESPIN)) {
							cfg.node_misc|=NM_NOPAUSESPIN;
							uifc.changes=1; }
                        break;
					case 6:
						i=cfg.node_misc&NM_CLOSENODEDAB ? 1:0;
						strcpy(opt[0],"Yes");
						strcpy(opt[1],"No");
						opt[2][0]=0;
						uifc.savnum=1;
						SETHELP(WHERE);
/*
Keep Node File Open:

If you want to keep the shared node file (ctrl/node.dab) open,
(for better performance and reliability) set this option to Yes.
If want to keep the file closed (for Samba compatiblity), set this
option to No.
*/
						i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
							,"Keep Node File Open",opt);
						if(i==0 && cfg.node_misc&NM_CLOSENODEDAB) {
							cfg.node_misc&=~NM_CLOSENODEDAB;
							uifc.changes=1; }
						else if(i==1 && !(cfg.node_misc&NM_CLOSENODEDAB)) {
							cfg.node_misc|=NM_CLOSENODEDAB;
							uifc.changes=1; }
                        break;
						} }
			break;
		case 2:
			done=0;
			while(!done) {
				i=0;
				sprintf(opt[i++],"%-27.27s%s","Validation User"
					,cfg.node_valuser ? ultoa(cfg.node_valuser,tmp,10) : "Nobody");
				sprintf(opt[i++],"%-27.27s%u seconds","Semaphore Frequency"
					,cfg.node_sem_check);
				sprintf(opt[i++],"%-27.27s%u seconds","Statistics Frequency"
					,cfg.node_stat_check);
				sprintf(opt[i++],"%-27.27s%u seconds","Inactivity Warning"
					,cfg.sec_warn);
				sprintf(opt[i++],"%-27.27s%u seconds","Inactivity Disconnection"
					,cfg.sec_hangup);
				sprintf(tmp,"$%d.00",cfg.node_dollars_per_call);
				sprintf(opt[i++],"%-27.27s%.40s","Daily Event",cfg.node_daily);
				sprintf(opt[i++],"%-27.27s%.40s","Text Directory",cfg.text_dir);
				opt[i][0]=0;
				uifc.savnum=0;
SETHELP(WHERE);
/*
Node Advanced Options:

This is the advanced options menu for the selected node. The available
options are of an advanced nature and should not be modified unless you
are sure of the consequences and necessary preparation.
*/
				switch(uifc.list(WIN_T2B|WIN_RHT|WIN_ACT|WIN_SAV,2,0,60,&adv_dflt,0
					,"Advanced Options",opt)) {
                    case -1:
						done=1;
                        break;
					case 0:
						ultoa(cfg.node_valuser,str,10);
SETHELP(WHERE);
/*
Validation User Number:

When a caller logs onto the system as New, he or she must send
validation feedback to the sysop. This feature can be disabled by
setting this value to 0, allowing new users to logon without sending
validation feedback. If you want new users on this node to be forced to
send validation feedback, set this value to the number of the user to
whom the feedback is sent. The normal value of this option is 1 for
user number one.
*/
						uifc.input(WIN_MID,0,13,"Validation User Number (0=Nobody)"
							,str,4,K_NUMBER|K_EDIT);
						cfg.node_valuser=atoi(str);
						break;
					case 1:
						ultoa(cfg.node_sem_check,str,10);
SETHELP(WHERE);
/*
Semaphore Check Frequency While Waiting for Call (in seconds):

This is the number of seconds between semaphore checks while this node
is waiting for a caller. Default is 60 seconds.
*/
						uifc.input(WIN_MID|WIN_SAV,0,14
							,"Seconds Between Semaphore Checks"
							,str,3,K_NUMBER|K_EDIT);
						cfg.node_sem_check=atoi(str);
                        break;
					case 2:
						ultoa(cfg.node_stat_check,str,10);
SETHELP(WHERE);
/*
Statistics Check Frequency While Waiting for Call (in seconds):

This is the number of seconds between static checks while this node
is waiting for a caller. Default is 10 seconds.
*/
						uifc.input(WIN_MID|WIN_SAV,0,14
							,"Seconds Between Statistic Checks"
							,str,3,K_NUMBER|K_EDIT);
						cfg.node_stat_check=atoi(str);
                        break;
					case 3:
						ultoa(cfg.sec_warn,str,10);
SETHELP(WHERE);
/*
Seconds Before Inactivity Warning:

This is the number of seconds the user must be inactive before a
warning will be given. Default is 180 seconds.
*/
						uifc.input(WIN_MID|WIN_SAV,0,14
							,"Seconds Before Inactivity Warning"
							,str,4,K_NUMBER|K_EDIT);
						cfg.sec_warn=atoi(str);
                        break;
					case 4:
						ultoa(cfg.sec_hangup,str,10);
SETHELP(WHERE);
/*
Seconds Before Inactivity Disconnection:

This is the number of seconds the user must be inactive before they
will be automatically disconnected. Default is 300 seconds.
*/
						uifc.input(WIN_MID|WIN_SAV,0,14
							,"Seconds Before Inactivity Disconnection"
							,str,4,K_NUMBER|K_EDIT);
						cfg.sec_hangup=atoi(str);
                        break;
					case 5:
SETHELP(WHERE);
/*
Daily Event:

If you have an event that this node should run every day, enter the
command line for that event here.

An event can be any valid DOS command line. If multiple programs or
commands are required, use a batch file.

Remember: The %! command line specifier is an abreviation for your
		  configured EXEC directory path.
*/
						uifc.input(WIN_MID|WIN_SAV,0,10,"Daily Event"
							,cfg.node_daily,50,K_EDIT);
						break;
					case 6:
SETHELP(WHERE);
/*
Text Directory:

Your text directory contains read-only text files. Synchronet never
writes to any files in this directory so it CAN be placed on a RAM
disk or other volatile media. This directory contains the system's menus
and other important text files, so be sure the files and directories are
moved to this directory if you decide to change it.

This option allows you to change the location of your control directory.
The \TEXT\ suffix (sub-directory) cannot be changed or removed.
*/
						uifc.input(WIN_MID|WIN_SAV,0,10,"Text Directory"
							,cfg.text_dir,50,K_EDIT);
						break; 
				} 
			}
			break;
		} 
	}
}
