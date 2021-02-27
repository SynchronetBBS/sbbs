/* $Id: scfgnode.c,v 1.37 2020/08/08 19:24:27 rswindell Exp $ */

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

/* These correlate with the LOG_* definitions in syslog.h/gen_defs.h */
static char* errLevelStringList[]
	= {"Emergency", "Alert", "Critical", "Error", NULL};

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
			j|=WIN_DEL|WIN_COPY;
		if(cfg.sys_nodes<MAX_NODES && cfg.sys_nodes<MAX_OPTS)
			j|=WIN_INS;
		if(savnode)
			j|=WIN_PASTE;
		uifc.helpbuf=
			"`Nodes:`\n"
			"\n"
			"This is the list of configured terminal server nodes. A node is required\n"
			"for each supported simultaneous 'caller'.\n"
			"\n"
			"`Note:` The `FirstNode` (e.g. Node 1) configuration settings are used for\n"
			"      all the nodes supported by a single terminal server instance.\n"
			"\n"
			"`Note:` When nodes are added to this list, the `LastNode` value must be\n"
			"      adjusted accordingly. See the `ctrl/sbbs.ini` file for more details.\n"
			"\n"
			"To add a node, hit ~ INS ~.\n"
			"\n"
			"To delete a node, hit ~ DEL ~.\n"
			"\n"
			"To configure a node, select it using the arrow keys and hit ~ ENTER ~.\n"
			"\n"
			"To copy a node's configuration to another node, first select the source\n"
			"node with the arrow keys and hit ~ F5 ~. Then select the destination\n"
			"node and hit ~ F6 ~.\n"
		;

		i=uifc.list(j,0,0,13,&node_menu_dflt,&node_bar,"Nodes",opt);
		if(i==-1) {
			if(savnode) {
				free_node_cfg(&cfg);
				savnode=0; 
			}
			return; 
		}
		int msk = i & MSK_ON;
		if(msk == MSK_DEL) {
			sprintf(str,"Delete Node %d",cfg.sys_nodes);
			i=1;
			uifc.helpbuf=
				"`Delete Node:`\n"
				"\n"
				"If you are positive you want to delete this node, select `Yes`. Otherwise,\n"
				"select `No` or hit ~ ESC ~.\n"
			;
			i=uifc.list(WIN_MID,0,0,0,&i,0,str,uifcYesNoOpts);
			if(!i) {
				--cfg.sys_nodes;
				cfg.new_install=new_install;
				save_main_cfg(&cfg,backup_level);
				refresh_cfg(&cfg);
			}
			continue; 
		}
		if(msk == MSK_INS) {
			SAFECOPY(cfg.node_dir,cfg.node_path[cfg.sys_nodes-1]);
			i=cfg.sys_nodes+1;
			load_node_cfg(&cfg,error);
			sprintf(str,"../node%d/",i);
			sprintf(tmp,"Node %d Path",i);
			uifc.helpbuf=
				"`Node Path:`\n"
				"\n"
				"This is the path to this node's private directory where its separate\n"
				"configuration and data files are stored.\n"
				"\n"
				"The drive and directory of this path can be set to any valid DOS\n"
				"directory that can be accessed by `ALL` nodes and `MUST NOT` be on a RAM disk\n"
				"or other volatile media.\n"
				"\n"
				"If you want to abort the creation of this new node, hit ~ ESC ~.\n"
			;
			j=uifc.input(WIN_MID,0,0,tmp,str,50,K_EDIT);
			uifc.changes=0;
			if(j<2)
				continue;
			truncsp(str);
			SAFECOPY(cfg.node_path[i-1],str);
			md(str);
			cfg.node_num=++cfg.sys_nodes;
			SAFEPRINTF(cfg.node_name,"Node %u",cfg.node_num);
			SAFECOPY(cfg.node_phone,"N/A");
			cfg.new_install=new_install;
			save_node_cfg(&cfg,backup_level);
			save_main_cfg(&cfg,backup_level);
			free_node_cfg(&cfg);
			refresh_cfg(&cfg);
			continue;
		}
		if(msk == MSK_COPY) {
			if(savnode)
				free_node_cfg(&cfg);
			i&=MSK_OFF;
			SAFECOPY(cfg.node_dir,cfg.node_path[i]);
			load_node_cfg(&cfg,error);
			savnode=1;
			continue; 
		}
		if(msk == MSK_PASTE) {
			i&=MSK_OFF;
			SAFECOPY(cfg.node_dir,cfg.node_path[i]);
			cfg.node_num=i+1;
			save_node_cfg(&cfg,backup_level);
			refresh_cfg(&cfg);
			continue;
		}

		if(savnode) {
			free_node_cfg(&cfg);
			savnode=0; 
		}
		SAFECOPY(cfg.node_dir,cfg.node_path[i]);
		prep_dir(cfg.ctrl_dir, cfg.node_dir, sizeof(cfg.node_dir));

		load_node_cfg(&cfg,error);
		if (cfg.node_num != i + 1) { 	/* Node number isn't right? */
			cfg.node_num = i + 1;		/* so fix it */
			save_node_cfg(&cfg, backup_level); /* and write it back */
		}
		node_cfg();

		free_node_cfg(&cfg); 
	}
}

void node_cfg()
{
	static	int node_dflt;
	char	done,str[81];
	static	int adv_dflt,tog_dflt,tog_bar;
	int 	i;

	while(1) {
		i=0;
		sprintf(opt[i++],"%-27.27s%s","Phone Number",cfg.node_phone);
		sprintf(opt[i++],"%-27.27s%.40s","Logon Requirements",cfg.node_arstr);
		strcpy(opt[i++],"Toggle Options...");
		strcpy(opt[i++],"Advanced Options...");
		opt[i][0]=0;
		sprintf(str,"Node %d Configuration",cfg.node_num);
		uifc.helpbuf=
			"`Node Configuration:`\n"
			"\n"
			"The configuration settings of the `FirstNode` will determine the behavior\n"
			"of all nodes of a single terminal server instance  (through `LastNode`).\n"
			"See the `ctrl/sbbs.ini` file for details.\n"
		;
		switch(uifc.list(WIN_ACT|WIN_CHE|WIN_BOT|WIN_RHT,0,0,60,&node_dflt,0
			,str,opt)) {
			case -1:
				i=save_changes(WIN_MID|WIN_SAV);
				if(!i) {
					save_node_cfg(&cfg,backup_level);
					refresh_cfg(&cfg);
				}
				if(i!=-1)
					return;
				break;
			case 0:
				uifc.helpbuf=
					"`Node Phone Number:`\n"
					"\n"
					"This is the phone number to access the selected node (e.g. for `SEXPOTS`).\n"
					"This value is used for information purposes only.\n"
				;
				uifc.input(WIN_MID|WIN_SAV,0,10,"Phone Number",cfg.node_phone,sizeof(cfg.node_phone)-1,K_EDIT);
				break;
			case 1:
				sprintf(str,"Node %u Logon",cfg.node_num);
				getar(str,cfg.node_arstr);
				break;
			case 2:
				done=0;
				while(!done) {
					i=0;
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
					uifc.helpbuf=
						"`Node Toggle Options:`\n"
						"\n"
						"This is the toggle options menu for the selected node's configuration.\n"
						"\n"
						"The available options from this menu can all be toggled between two or\n"
						"more states, such as `Yes` and `No``.\n"
					;
					switch(uifc.list(WIN_BOT|WIN_RHT|WIN_ACT|WIN_SAV,3,2,35,&tog_dflt
						,&tog_bar,"Toggle Options",opt)) {
						case -1:
							done=1;
							break;
						case 0:
							i=cfg.node_misc&NM_NO_NUM ? 1:0;
							uifc.helpbuf=
								"`Allow Login by User Number:`\n"
								"\n"
								"If you want users to be able login using their user number at the\n"
								"login prompt, set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
								,"Allow Login by User Number",uifcYesNoOpts);
							if(i==0 && cfg.node_misc&NM_NO_NUM) {
								cfg.node_misc&=~NM_NO_NUM;
								uifc.changes=1; 
							}
							else if(i==1 && !(cfg.node_misc&NM_NO_NUM)) {
								cfg.node_misc|=NM_NO_NUM;
								uifc.changes=1; 
							}
							break;
						case 1:
							i=cfg.node_misc&NM_LOGON_R ? 0:1;
							uifc.helpbuf=
								"`Allow Login by Real Name:`\n"
								"\n"
								"If you want users to be able login using their real name as well as\n"
								"their alias, set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
								,"Allow Login by Real Name",uifcYesNoOpts);
							if(i==0 && !(cfg.node_misc&NM_LOGON_R)) {
								cfg.node_misc|=NM_LOGON_R;
								uifc.changes=1; 
							}
							else if(i==1 && (cfg.node_misc&NM_LOGON_R)) {
								cfg.node_misc&=~NM_LOGON_R;
								uifc.changes=1; 
							}
							break;
						case 2:
							i=cfg.node_misc&NM_LOGON_P ? 0:1;
							uifc.helpbuf=
								"`Always Prompt for Password:`\n"
								"\n"
								"If you want to have attempted logins using an unknown user name still\n"
								"prompt for a password (i.e. for enhanced security), set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
								,"Always Prompt for Password",uifcYesNoOpts);
							if(i==0 && !(cfg.node_misc&NM_LOGON_P)) {
								cfg.node_misc|=NM_LOGON_P;
								uifc.changes=1; 
							}
							else if(i==1 && (cfg.node_misc&NM_LOGON_P)) {
								cfg.node_misc&=~NM_LOGON_P;
								uifc.changes=1; 
							}
							break;
						case 3:
							i=cfg.node_misc&NM_7BITONLY ? 0:1;
							uifc.helpbuf=
								"`Allow 8-bit Remote Input During Login:`\n"
								"\n"
								"If you wish to allow E-7-1 terminals to use this node, you must set this\n"
								"option to `No`. This will also eliminate the ability of 8-bit remote users\n"
								"to send IBM extended ASCII characters during the login sequence.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
								,"Allow 8-bit Remote Input During Login",uifcYesNoOpts);
							if(i==1 && !(cfg.node_misc&NM_7BITONLY)) {
								cfg.node_misc|=NM_7BITONLY;
								uifc.changes=1; 
							}
							else if(i==0 && (cfg.node_misc&NM_7BITONLY)) {
								cfg.node_misc&=~NM_7BITONLY;
								uifc.changes=1; 
							}
							break;
						case 4:
							i=cfg.node_misc&NM_NOPAUSESPIN ? 1:0;
							uifc.helpbuf=
								"`Spinning Pause Prompt:`\n"
								"\n"
								"If you want to display a spinning cursor at the [Hit a key] prompt,\n"
								"set this option to `Yes`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
								,"Spinning Cursor at Pause Prompt",uifcYesNoOpts);
							if(i==0 && cfg.node_misc&NM_NOPAUSESPIN) {
								cfg.node_misc&=~NM_NOPAUSESPIN;
								uifc.changes=1; 
							}
							else if(i==1 && !(cfg.node_misc&NM_NOPAUSESPIN)) {
								cfg.node_misc|=NM_NOPAUSESPIN;
								uifc.changes=1; 
							}
							break;
						case 5:
							i=cfg.node_misc&NM_CLOSENODEDAB ? 1:0;
							uifc.helpbuf=
								"`Keep Node File Open:`\n"
								"\n"
								"If you want to keep the shared node file (`ctrl/node.dab`) open,\n"
								"(for better performance and reliability) set this option to `Yes`.\n"
								"If want to keep the file closed (for `Samba` compatibility), set this\n"
								"option to `No`.\n"
							;
							i=uifc.list(WIN_MID|WIN_SAV,0,10,0,&i,0
								,"Keep Node File Open",uifcYesNoOpts);
							if(i==0 && cfg.node_misc&NM_CLOSENODEDAB) {
								cfg.node_misc&=~NM_CLOSENODEDAB;
								uifc.changes=1; 
							}
							else if(i==1 && !(cfg.node_misc&NM_CLOSENODEDAB)) {
								cfg.node_misc|=NM_CLOSENODEDAB;
								uifc.changes=1; 
							}
							break;
						}
					}
				break;
			case 3:
				done=0;
				while(!done) {
					i=0;
					sprintf(opt[i++],"%-27.27s%s","Validation User"
						,cfg.node_valuser ? ultoa(cfg.node_valuser,tmp,10) : "Nobody");
					sprintf(opt[i++],"%-27.27s%s","Notification User"
						,cfg.node_erruser ? ultoa(cfg.node_erruser,tmp,10) : "Nobody");
					sprintf(opt[i++],"%-27.27s%s","Notification Error Level"
						,errLevelStringList[cfg.node_errlevel]);
					sprintf(opt[i++],"%-27.27s%u seconds","Semaphore Frequency"
						,cfg.node_sem_check);
					sprintf(opt[i++],"%-27.27s%u seconds","Statistics Frequency"
						,cfg.node_stat_check);
					sprintf(opt[i++],"%-27.27s%u seconds","Inactivity Warning"
						,cfg.sec_warn);
					sprintf(opt[i++],"%-27.27s%u seconds","Inactivity Disconnection"
						,cfg.sec_hangup);
					sprintf(opt[i++],"%-27.27s%.40s","Daily Event",cfg.node_daily);
					sprintf(opt[i++],"%-27.27s%.40s","Text Directory",cfg.text_dir);
					opt[i][0]=0;
					uifc.helpbuf=
						"`Node Advanced Options:`\n"
						"\n"
						"This is the advanced options menu for the selected node. The available\n"
						"options are of an advanced nature and should not be modified unless you\n"
						"are sure of the consequences and necessary preparation.\n"
					;
					switch(uifc.list(WIN_T2B|WIN_RHT|WIN_ACT|WIN_SAV,2,0,60,&adv_dflt,0
						,"Advanced Options",opt)) {
						case -1:
							done=1;
							break;
						case __COUNTER__:
							ultoa(cfg.node_valuser,str,10);
							uifc.helpbuf=
								"`Validation User Number:`\n"
								"\n"
								"When a caller logs onto the system as `New`, he or she must send\n"
								"validation feedback to the sysop. This feature can be disabled by\n"
								"setting this value to `0`, allowing new users to logon without sending\n"
								"validation feedback. If you want new users on this node to be forced to\n"
								"send validation feedback, set this value to the number of the user to\n"
								"whom the feedback is sent. The normal value of this option is `1` for\n"
								"user number one.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,13,"Validation User Number (0=Nobody)"
								,str,4,K_NUMBER|K_EDIT);
							cfg.node_valuser=atoi(str);
							break;
						case __COUNTER__:
							ultoa(cfg.node_erruser,str,10);
							uifc.helpbuf=
								"`Notification User Number:`\n"
								"\n"
								"When an error has occurred, a notification message can be sent to a\n"
								"configured user number (i.e. a sysop). This feature can be disabled by\n"
								"setting this value to `0`. The normal value of this option is `1` for\n"
								"user number one.\n"
								"\n"
								"Note: error messages are always logged as well (e.g. to `data/error.log`)."
							;
							uifc.input(WIN_MID|WIN_SAV,0,13,"Notification User Number (0=Nobody)"
								,str,4,K_NUMBER|K_EDIT);
							cfg.node_erruser=atoi(str);
							break;
						case __COUNTER__:
							uifc.helpbuf=
								"`Notification Error Level`\n"
								"\n"
								"Select the minimum severity of error messages that should be forwarded\n"
								"to the Notification User. The normal setting would be `Critical`.";
							int i = cfg.node_errlevel;
							i = uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Notification Error Level",errLevelStringList);
							if(i>=0 && i<=LOG_ERR) {
								if(cfg.node_errlevel != i)
									uifc.changes = TRUE;
								cfg.node_errlevel=i;
							}
							break;
						case __COUNTER__:
							ultoa(cfg.node_sem_check,str,10);
							uifc.helpbuf=
								"`Semaphore Check Frequency While Waiting for Call (in seconds):`\n"
								"\n"
								"This is the number of seconds between semaphore checks while this node\n"
								"is waiting for a caller. Default is `5` seconds.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,14
								,"Seconds Between Semaphore Checks"
								,str,3,K_NUMBER|K_EDIT);
							cfg.node_sem_check=atoi(str);
							break;
						case __COUNTER__:
							ultoa(cfg.node_stat_check,str,10);
							uifc.helpbuf=
								"`Statistics Check Frequency While Waiting for Call (in seconds):`\n"
								"\n"
								"This is the number of seconds between static checks while this node\n"
								"is waiting for a caller. Default is `5` seconds.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,14
								,"Seconds Between Statistic Checks"
								,str,3,K_NUMBER|K_EDIT);
							cfg.node_stat_check=atoi(str);
							break;
						case __COUNTER__:
							ultoa(cfg.sec_warn,str,10);
							uifc.helpbuf=
								"`Seconds Before Inactivity Warning:`\n"
								"\n"
								"This is the number of seconds the user must be inactive before a\n"
								"warning will be given. Default is `180` seconds.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,14
								,"Seconds Before Inactivity Warning"
								,str,4,K_NUMBER|K_EDIT);
							cfg.sec_warn=atoi(str);
							break;
						case __COUNTER__:
							ultoa(cfg.sec_hangup,str,10);
							uifc.helpbuf=
								"`Seconds Before Inactivity Disconnection:`\n"
								"\n"
								"This is the number of seconds the user must be inactive before they\n"
								"will be automatically disconnected. Default is `300` seconds.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,14
								,"Seconds Before Inactivity Disconnection"
								,str,4,K_NUMBER|K_EDIT);
							cfg.sec_hangup=atoi(str);
							break;
						case __COUNTER__:
							uifc.helpbuf=
								"`Daily Event:`\n"
								"\n"
								"If you have an event that this node's terminal server should run every\n"
								"day, enter the command line for that event here.\n"
								"\n"
								"An event can be any valid command line. If multiple programs or commands\n"
								"are required, use a batch file or shell script.\n"
								SCFG_CMDLINE_PREFIX_HELP
								SCFG_CMDLINE_SPEC_HELP
							;
							uifc.input(WIN_MID|WIN_SAV,0,10,"Daily Event"
								,cfg.node_daily,sizeof(cfg.node_daily)-1,K_EDIT);
							break;
						case __COUNTER__:
							uifc.helpbuf=
								"`Text Directory:`\n"
								"\n"
								"Your text directory contains `read-only` text files. Synchronet never\n"
								"`writes` to any files in this directory so it `CAN` be placed on a RAM\n"
								"disk or other volatile media. This directory contains the system's menus\n"
								"and other important text files, so be sure the files and directories are\n"
								"moved to this directory if you decide to change it.\n"
							;
							uifc.input(WIN_MID|WIN_SAV,0,10,"Text Directory"
								,cfg.text_dir,sizeof(cfg.text_dir)-1,K_EDIT);
							break; 
					} 
				}
				break;
		} 
	}
}
