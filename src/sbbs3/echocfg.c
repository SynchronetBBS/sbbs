/* echocfg.c */

/* SBBSecho configuration utility 											*/

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

/* Portions written by Allen Christiansen 1994-1996 						*/

#include <stdio.h>

#undef JAVASCRIPT

#include "uifc.h"
#include "sbbs.h"
#include "sbbsecho.h"

char **opt;

// void uifc.bail(int code);
int main();

uchar node_swap=1;
long misc=0;
config_t cfg;

unsigned _stklen=16000;

uifcapi_t uifc;

void bail(int code)
{

	uifc.bail();
	exit(code);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *wcfaddrtoa(faddr_t* addr)
{
    static char str[25];
	char tmp[25];

	if(addr->zone==0xffff)
		strcpy(str,"ALL");
	else {
		sprintf(str,"%u:",addr->zone);
		if(addr->net==0xffff)
			strcat(str,"ALL");
		else {
			sprintf(tmp,"%u/",addr->net);
			strcat(str,tmp);
			if(addr->node==0xffff)
				strcat(str,"ALL");
			else {
				sprintf(tmp,"%u",addr->node);
				strcat(str,tmp);
				if(addr->point==0xffff)
					strcat(str,".ALL");
				else if(addr->point) {
					sprintf(tmp,".%u",addr->point);
					strcat(str,tmp); 
				} 
			} 
		} 
	}
	return(str);
}


// ToDo Fix This.
#define lprintf printf

int main(int argc, char **argv)
{
	char str[256],*p;
	int i,j,k,x,dflt,nodeop=0;
	FILE *stream;
	echolist_t savlistcfg;
	nodecfg_t savnodecfg;
	arcdef_t savarcdef;
	BOOL door_mode=FALSE;
	BOOL gui_mode=TRUE;

	fprintf(stderr,"\nSBBSecho Configuration  Version %s  Copyright 2003 "
		"Rob Swindell\n\n",SBBSECHO_VER);

	memset(&cfg,0,sizeof(config_t));
	str[0]=0;
	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-')
			switch(toupper(argv[i][1])) {
				case 'T':
					gui_mode=FALSE;
					break;
				case 'D':
					door_mode=TRUE;
					break;
		}
		else
			strcpy(str,argv[1]);
	}
	if(str[0]==0) {
		p=getenv("SBBSCTRL");
		if(!p) {
			p=getenv("SBBSNODE");
			if(!p) {
				printf("usage: echocfg [cfg_file]\n");
				exit(1); }
			strcpy(str,p);
			backslash(str);
			strcat(str,"../ctrl/sbbsecho.cfg"); }
		else {
			strcpy(str,p);
			backslash(str);
			strcat(str,"sbbsecho.cfg"); 
		} 
	}
	strcpy(cfg.cfgfile,str);

	read_echo_cfg();

	// savnum=0;
	if((opt=(char **)MALLOC(sizeof(char *)*300))==NULL) {
	// ToDo fix this.
	//	cputs("memory allocation error\r\n");
	//	uifc.bail(); }
		exit(1); 
	}
	for(i=0;i<300;i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL) {
	// ToDo fix this.
	//		cputs("memory allocation error\r\n");
	//      uifc.bail(); }
			exit(1); 
		}
	uifc.size=sizeof(uifc);
#if defined(USE_FLTK)
	if(!door_mode && gui_mode==TRUE
#if defined(__unix__)
		&& (getenv("DISPLAY")!=NULL)
#endif
		)
		i=uifcinifltk(&uifc);  /* dialog */
	else
#endif
#ifdef USE_DIALOG
	if(!door_mode)
		i=uifcinid(&uifc);
	else
#elif defined USE_CURSES
	if(!door_mode)
		i=uifcinic(&uifc);
	else
#elif !defined(__unix__)
	if(!door_mode)
		i=uifcini(&uifc);
	else
#endif
	i=uifcinix(&uifc);

	if(i!=0) {
		printf("uifc library init returned error %d\n",i);
		exit(1);
	}

	sprintf(str,"SBBSecho Configuration v%s",SBBSECHO_VER);
	uifc.scrn(str);

	dflt=0;
	while(1) {
		uifc.helpbuf=
	" SBBSecho Configuration \r\n\r\n"
	"Move through the various options using the arrow keys.  Select the\r\n"
	"highlighted options by pressing ENTER.\r\n\r\n";
		i=0;
		sprintf(opt[i++],"%-30.30s %s","Mailer Type"
			,misc&FLO_MAILER ? "Binkley/FLO":"FrontDoor/Attach");
		sprintf(opt[i++],"%-30.30s %luK","Maximum Packet Size"
			,cfg.maxpktsize/1024UL);
		sprintf(opt[i++],"%-30.30s %luK","Maximum Bundle Size"
			,cfg.maxbdlsize/1024UL);
		if(cfg.notify)
			sprintf(str,"User #%u",cfg.notify);
		else
			strcpy(str,"Disabled");
		sprintf(opt[i++],"%-30.30s %s","Areafix Failure Notification",str);
		sprintf(opt[i++],"Nodes...");
		sprintf(opt[i++],"Paths...");
		sprintf(opt[i++],"Log Options...");
		sprintf(opt[i++],"Toggle Options...");
		sprintf(opt[i++],"Archive Programs...");
		sprintf(opt[i++],"Additional Echo Lists...");
		opt[i][0]=0;
		switch(uifc.list(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC,0,0,52,&dflt,0
		,cfg.cfgfile,opt)) {

			case 0:
				misc^=FLO_MAILER;
				break;

			case 1:
	uifc.helpbuf=
	" Maximum Packet Size \r\n\r\n"
	"This is the maximum file size that SBBSecho will create when placing\r\n"
	"outgoing messages into packets.  The default size is 250k.\r\n";
				sprintf(str,"%lu",cfg.maxpktsize);
				uifc.input(WIN_MID|WIN_BOT,0,0,"Maximum Packet Size",str
					,9,K_EDIT|K_NUMBER);
				cfg.maxpktsize=atol(str);
				break;

			case 2:
	uifc.helpbuf=
	" Maximum Bundle Size \r\n\r\n"
	"This is the maximum file size that SBBSecho will create when placing\r\n"
	"outgoing packets into bundles.  The default size is 250k.\r\n";
				sprintf(str,"%lu",cfg.maxbdlsize);
				uifc.input(WIN_MID|WIN_BOT,0,0,"Maximum Bundle Size",str
					,9,K_EDIT|K_NUMBER);
				cfg.maxbdlsize=atol(str);
				break;

			case 3:
	uifc.helpbuf=
	" Areafix Failure Notification \r\n\r\n"
	"Setting this option to a user number (usually #1), enables the\r\n"
	"automatic notification of that user, via e-mail, of failed areafix\r\n"
	"attempts. Setting this option to 0, disables this feature.\r\n";
				sprintf(str,"%u",cfg.notify);
				uifc.input(WIN_MID|WIN_BOT,0,0,"Areafix Notification User Number",str
					,5,K_EDIT|K_NUMBER);
				cfg.notify=atoi(str);
				break;

			case 4:
	uifc.helpbuf=
	" Nodes... \r\n\r\n"
	"From this menu you can configure the area manager options for your\r\n"
	"uplink nodes.\r\n";
				i=0;
				while(1) {
					for(j=0;j<cfg.nodecfgs;j++)
						strcpy(opt[j],wcfaddrtoa(&cfg.nodecfg[j].faddr));
					opt[j][0]=0;
					i=uifc.list(WIN_ORG|WIN_INS|WIN_DEL|WIN_ACT|WIN_GET|WIN_PUT
						|WIN_INSACT|WIN_DELACT|WIN_XTR
						,0,0,0,&i,0,"Nodes",opt);
					if(i==-1)
						break;
					if((i&MSK_ON)==MSK_INS) {
						i&=MSK_OFF;
						str[0]=0;
	uifc.helpbuf=
	" Address \r\n\r\n"
	"This is the FidoNet style address of the node you wish to add\r\n";
						if(uifc.input(WIN_MID,0,0
							,"Node Address (ALL wildcard allowed)",str
							,25,K_EDIT)<1)
							continue;
						if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
							,sizeof(nodecfg_t)*(cfg.nodecfgs+1)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); }
						for(j=cfg.nodecfgs;j>i;j--)
							memcpy(&cfg.nodecfg[j],&cfg.nodecfg[j-1]
								,sizeof(nodecfg_t));
						cfg.nodecfgs++;
						memset(&cfg.nodecfg[i],0,sizeof(nodecfg_t));
						cfg.nodecfg[i].faddr=atofaddr(str);
						continue; }

					if((i&MSK_ON)==MSK_DEL) {
						i&=MSK_OFF;
						cfg.nodecfgs--;
						if(cfg.nodecfgs<=0) {
							cfg.nodecfgs=0;
							continue; }
						for(j=i;j<cfg.nodecfgs;j++)
							memcpy(&cfg.nodecfg[j],&cfg.nodecfg[j+1]
								,sizeof(nodecfg_t));
						if((cfg.nodecfg=(nodecfg_t *)REALLOC(cfg.nodecfg
							,sizeof(nodecfg_t)*(cfg.nodecfgs)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); }
						continue; }
					if((i&MSK_ON)==MSK_GET) {
						i&=MSK_OFF;
						memcpy(&savnodecfg,&cfg.nodecfg[i],sizeof(nodecfg_t));
						continue; }
					if((i&MSK_ON)==MSK_PUT) {
						i&=MSK_OFF;
						memcpy(&cfg.nodecfg[i],&savnodecfg,sizeof(nodecfg_t));
						continue; }
					while(1) {
	uifc.helpbuf=
	" Node Options \r\n\r\n"
	"These are the configurable options available for this node.\r\n";
						j=0;
						sprintf(opt[j++],"%-20.20s %s","Address"
							,wcfaddrtoa(&cfg.nodecfg[i].faddr));
						sprintf(opt[j++],"%-20.20s %s","Archive Type"
							,cfg.nodecfg[i].arctype>cfg.arcdefs ?
							"None":cfg.arcdef[cfg.nodecfg[i].arctype].name);
						sprintf(opt[j++],"%-20.20s %s","Packet Type"
							,cfg.nodecfg[i].pkt_type==PKT_TWO ? "2"
							:cfg.nodecfg[i].pkt_type==PKT_TWO_TWO ? "2.2":"2+");
						sprintf(opt[j++],"%-20.20s %s","Packet Password"
							,cfg.nodecfg[i].pktpwd);
						sprintf(opt[j++],"%-20.20s %s","Areafix Password"
							,cfg.nodecfg[i].password);
						str[0]=0;
						for(k=0;k<cfg.nodecfg[i].numflags;k++) {
							strcat(str,cfg.nodecfg[i].flag[k].flag);
							strcat(str," "); }
						sprintf(opt[j++],"%-20.20s %s","Areafix Flags",str);
						sprintf(opt[j++],"%-20.20s %s","Status"
							,cfg.nodecfg[i].attr&ATTR_CRASH ? "Crash"
							:cfg.nodecfg[i].attr&ATTR_HOLD ? "Hold" : "None");
						sprintf(opt[j++],"%-20.20s %s","Direct"
							,cfg.nodecfg[i].attr&ATTR_DIRECT ? "Yes":"No");
						sprintf(opt[j++],"%-20.20s %s","Passive"
							,cfg.nodecfg[i].attr&ATTR_PASSIVE ? "Yes":"No");
						sprintf(opt[j++],"%-20.20s %s","Send Notify List"
							,cfg.nodecfg[i].attr&SEND_NOTIFY ? "Yes" : "No");
						if(misc&FLO_MAILER)
							sprintf(opt[j++],"%-20.20s %s","Route To"
								,cfg.nodecfg[i].route.zone
								? wcfaddrtoa(&cfg.nodecfg[i].route) : "Disabled");
						opt[j][0]=0;
						k=uifc.list(WIN_MID|WIN_ACT,0,0,40,&nodeop,0
							,wcfaddrtoa(&cfg.nodecfg[i].faddr),opt);
						if(k==-1)
							break;
						switch(k) {
							case 0:
	uifc.helpbuf=
	" Address \r\n\r\n"
	"This is the FidoNet style address of this node.\r\n";
								strcpy(str,wcfaddrtoa(&cfg.nodecfg[i].faddr));
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Node Address (ALL wildcard allowed)",str
									,25,K_EDIT|K_UPPER);
								cfg.nodecfg[i].faddr=atofaddr(str);
								break;
							case 1:
	uifc.helpbuf=
	" Archive Type \r\n\r\n"
	"This is the compression type that will be used for compressing packets\r\n"
	"to and decompressing packets from this node.\r\n";
								for(j=0;j<cfg.arcdefs;j++)
									strcpy(opt[j],cfg.arcdef[j].name);
								strcpy(opt[j++],"None");
								opt[j][0]=0;
								if(cfg.nodecfg[i].arctype<j)
									j=cfg.nodecfg[i].arctype;
								k=uifc.list(WIN_RHT|WIN_SAV,0,0,0,&j,0
									,"Archive Type",opt);
								if(k==-1)
									break;
								if(k>=cfg.arcdefs)
									cfg.nodecfg[i].arctype=0xffff;
								else
									cfg.nodecfg[i].arctype=k;
								break;
							case 2:
	uifc.helpbuf=
	" Packet Type \r\n\r\n"
	"This is the packet header type that will be used in mail packets to\r\n"
	"this node.  SBBSecho defaults to using type 2.2.\r\n";
								j=0;
								strcpy(opt[j++],"2+");
								strcpy(opt[j++],"2.2");
								strcpy(opt[j++],"2");
								opt[j][0]=0;
								j=cfg.nodecfg[i].pkt_type;
								k=uifc.list(WIN_RHT|WIN_SAV,0,0,0,&j,0,"Packet Type"
									,opt);
								if(k==-1)
									break;
								cfg.nodecfg[i].pkt_type=k;
								break;
							case 3:
	uifc.helpbuf=
	" Packet Password \r\n\r\n"
	"This is an optional password that SBBSecho will place into packets\r\n"
	"destined for this node.\r\n";
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Packet Password (optional)"
									,cfg.nodecfg[i].pktpwd,8,K_EDIT|K_UPPER);
								break;
							case 4:
	uifc.helpbuf=
	" Areafix Password \r\n\r\n"
	"This is the password that will be used by this node when doing remote\r\n"
	"areamanager functions.\r\n";
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Areafix Password"
									,cfg.nodecfg[i].password,8,K_EDIT|K_UPPER);
								break;
							case 5:
	uifc.helpbuf=
	" Areafix Flag \r\n\r\n"
	"This is a flag to to be given to this node allowing access to one or\r\n"
	"more of the configured echo lists\r\n";
								while(1) {
									for(j=0;j<cfg.nodecfg[i].numflags;j++)
										strcpy(opt[j],cfg.nodecfg[i].flag[j].flag);
									opt[j][0]=0;
									k=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|
										WIN_XTR|WIN_INSACT|WIN_DELACT|WIN_RHT
										,0,0,0,&k,0,"Areafix Flags",opt);
									if(k==-1)
										break;
									if((k&MSK_ON)==MSK_INS) {
										k&=MSK_OFF;
										str[0]=0;
										if(uifc.input(WIN_MID|WIN_SAV,0,0
											,"Areafix Flag",str,4
											,K_EDIT|K_UPPER)<1)
											continue;
										if((cfg.nodecfg[i].flag=(flag_t *)
											REALLOC(cfg.nodecfg[i].flag
											,sizeof(flag_t)*
											(cfg.nodecfg[i].numflags+1)))==NULL) {
											printf("\nMemory Allocation Error\n");
											exit(1); }
										for(j=cfg.nodecfg[i].numflags;j>i;j--)
											memcpy(&cfg.nodecfg[i].flag[j]
												,&cfg.nodecfg[i].flag[j-1]
												,sizeof(flag_t));
										cfg.nodecfg[i].numflags++;
										memset(&cfg.nodecfg[i].flag[k].flag
											,0,sizeof(flag_t));
										strcpy(cfg.nodecfg[i].flag[k].flag,str);
										continue; }

									if((k&MSK_ON)==MSK_DEL) {
										k&=MSK_OFF;
										cfg.nodecfg[i].numflags--;
										if(cfg.nodecfg[i].numflags<=0) {
											cfg.nodecfg[i].numflags=0;
											continue; }
										for(j=k;j<cfg.nodecfg[i].numflags;j++)
											strcpy(cfg.nodecfg[i].flag[j].flag
												,cfg.nodecfg[i].flag[j+1].flag);
										if((cfg.nodecfg[i].flag=(flag_t *)
											REALLOC(cfg.nodecfg[i].flag
											,sizeof(flag_t)*
											(cfg.nodecfg[i].numflags)))==NULL) {
											printf("\nMemory Allocation Error\n");
											exit(1); }
										continue; }
									strcpy(str,cfg.nodecfg[i].flag[k].flag);
									uifc.input(WIN_MID|WIN_SAV,0,0,"Areafix Flag"
										,str,4,K_EDIT|K_UPPER);
									strcpy(cfg.nodecfg[i].flag[k].flag,str);
									continue; }
								break;
							case 6:
								if(cfg.nodecfg[i].attr&ATTR_CRASH) {
									cfg.nodecfg[i].attr^=ATTR_CRASH;
									cfg.nodecfg[i].attr|=ATTR_HOLD;
									break; }
								if(cfg.nodecfg[i].attr&ATTR_HOLD) {
									cfg.nodecfg[i].attr^=ATTR_HOLD;
									break; }
								cfg.nodecfg[i].attr|=ATTR_CRASH;
								break;
							case 7:
								cfg.nodecfg[i].attr^=ATTR_DIRECT;
								break;
							case 8:
								cfg.nodecfg[i].attr^=ATTR_PASSIVE;
								break;
							case 9:
								cfg.nodecfg[i].attr^=SEND_NOTIFY;
								break;
							case 10:
	uifc.helpbuf=
	" Route To \r\n\r\n"
	"When using a FLO type mailer, this is the node number of an address\r\n"
	"to route mail to for this node.\r\n";
								strcpy(str,wcfaddrtoa(&cfg.nodecfg[i].route));
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Node Address to Route To",str
									,25,K_EDIT);
									if(str[0])
										cfg.nodecfg[i].route=atofaddr(str);
								break;
								} } }
				break;

			case 5:
	uifc.helpbuf=
	" Paths... \r\n\r\n"
	"From this menu you can configure the paths that SBBSecho will use\r\n"
	"when importing and exporting.\r\n";
				j=0;
				while(1) {
					i=0;
					sprintf(opt[i++],"%-30.30s %s","Inbound Directory"
						,cfg.inbound[0] ? cfg.inbound : "<Specified in SCFG>");
					sprintf(opt[i++],"%-30.30s %s","Secure Inbound (optional)"
						,cfg.secure[0] ? cfg.secure : "None Specified");
					sprintf(opt[i++],"%-30.30s %s","Outbound Directory"
						,cfg.outbound);
					sprintf(opt[i++],"%-30.30s %s","Area File"
						,cfg.areafile[0] ? cfg.areafile
						: "SCFG->data/areas.bbs");
					sprintf(opt[i++],"%-30.30s %s","Log File"
						,cfg.logfile[0] ? cfg.logfile
						: "SCFG->data/sbbsecho.log");
					opt[i][0]=0;
					j=uifc.list(WIN_MID|WIN_ACT,0,0,60,&j,0
						,"Paths and Filenames",opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
	uifc.helpbuf=
	" Inbound Directory \r\n\r\n"
	"This is the complete path (drive and directory) where your front\r\n"
	"end mailer stores, and where SBBSecho will look for, incoming message\r\n"
	"bundles and packets.";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Inbound",cfg.inbound
								,50,K_EDIT);
							break;

						case 1:
	uifc.helpbuf=
	" Secure Inbound Directory \r\n\r\n"
	"This is the complete path (drive and directory) where your front\r\n"
	"end mailer stores, and where SBBSecho will look for, incoming message\r\n"
	"bundles and packets for SECURE sessions.";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Secure Inbound",cfg.secure
								,50,K_EDIT);
							break;

						case 2:
	uifc.helpbuf=
	" Outbound Directory \r\n\r\n"
	"This is the complete path (drive and directory) where your front\r\n"
	"end mailer will look for, and where SBBSecho will place, outgoing\r\n"
	"message bundles and packets.";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Outbound",cfg.outbound
								,50,K_EDIT);
							break;

						case 3:
	uifc.helpbuf=
	" Area File \r\n\r\n"
	"This is the complete path (drive, directory, and filename) of the\r\n"
	"file SBBSecho will use as your AREAS.BBS file.";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Areafile",cfg.areafile
								,50,K_EDIT);
							break;

						case 4:
	uifc.helpbuf=
	" Log File \r\n\r\n"
	"This is the complete path (drive, directory, and filename) of the\r\n"
	"file SBBSecho will use to log information each time it is run.";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Logfile",cfg.logfile
								,50,K_EDIT);
							break; } }
				break;
			case 6:
	uifc.helpbuf=
	" Log Options \r\n"
	"\r\n"
	"Each loggable item can be toggled off or on from this menu. You must run\r\n"
	"SBBSecho with the /L command line option for any of these items to be\r\n"
	"logged.";
				j=0;
				while(1) {
					i=0;
					strcpy(opt[i++],"ALL");
					strcpy(opt[i++],"NONE");
					strcpy(opt[i++],"DEFAULT");
					sprintf(opt[i++],"%-35.35s%-3.3s","Ignored NetMail Messages"
						,cfg.log&LOG_IGNORED ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","NetMail for Unknown Users"
						,cfg.log&LOG_UNKNOWN ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Areafix NetMail Messages"
						,cfg.log&LOG_AREAFIX ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Imported NetMail Messages"
						,cfg.log&LOG_IMPORTED ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Packing Out-bound NetMail"
						,cfg.log&LOG_PACKING ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Routing Out-bound NetMail"
						,cfg.log&LOG_ROUTING ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","In-bound Packet Information"
						,cfg.log&LOG_PACKETS ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","In-bound Security Violations"
						,cfg.log&LOG_SECURITY ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","In-bound Grunged Messages"
						,cfg.log&LOG_GRUNGED ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Disallowed Private EchoMail"
						,cfg.log&LOG_PRIVATE ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Circular EchoMail Messages"
						,cfg.log&LOG_CIRCULAR ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Duplicate EchoMail Messages"
						,cfg.log&LOG_DUPES ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Area Totals"
						,cfg.log&LOG_AREA_TOTALS ? "Yes":"No");
					sprintf(opt[i++],"%-35.35s%-3.3s","Over-All Totals"
						,cfg.log&LOG_TOTALS ? "Yes":"No");
					opt[i][0]=0;
					j=uifc.list(0,0,0,43,&j,0,"Log Options",opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							cfg.log=~0L;
							break;
						case 1:
							cfg.log=0;
							break;
						case 2:
							cfg.log=LOG_DEFAULTS;
							break;
						case 3:
							cfg.log^=LOG_IGNORED;
							break;
						case 4:
							cfg.log^=LOG_UNKNOWN;
							break;
						case 5:
							cfg.log^=LOG_AREAFIX;
							break;
						case 6:
							cfg.log^=LOG_IMPORTED;
							break;
						case 7:
							cfg.log^=LOG_PACKING;
							break;
						case 8:
							cfg.log^=LOG_ROUTING;
							break;
						case 9:
							cfg.log^=LOG_PACKETS;
							break;
						case 10:
							cfg.log^=LOG_SECURITY;
							break;
						case 11:
							cfg.log^=LOG_GRUNGED;
							break;
						case 12:
							cfg.log^=LOG_PRIVATE;
							break;
						case 13:
							cfg.log^=LOG_CIRCULAR;
							break;
						case 14:
							cfg.log^=LOG_DUPES;
							break;
						case 15:
							cfg.log^=LOG_AREA_TOTALS;
							break;
						case 16:
							cfg.log^=LOG_TOTALS;
							break; } }
				break;


			case 7:
	uifc.helpbuf=
	"Secure Operation tells SBBSecho to check the AREAS.BBS file to insure\r\n"
	"    that the packet origin exists there as well as check the password of\r\n"
	"    that node (if configured).\r\n\r\n"
	"Swap for Executables tells SBBSecho whether or not it should swap\r\n"
	"    out of memory when executing external executables.\r\n\r\n"
	"Fuzzy Zone Operation when set to yes if SBBSecho receives an inbound\r\n"
	"    netmail with no international zone information, it will compare the\r\n"
	"    net/node of the destination to the net/node information in your AKAs\r\n"
	"    and assume the zone of a matching AKA.\r\n\r\n"
	"Store PATH/SEEN-BY/Unkown Kludge Lines in Message Base allows you to\r\n"
	"    determine whether or not SBBSecho will store this information from\r\n"
	"    incoming messages in the Synchronet message base.\r\n\r\n"
	"Allow Nodes to Add Areas in the AREAS.BBS List when set to YES allows\r\n"
	"    uplinks to add areas listed in the AREAS.BBS file\r\n";
				j=0;
				while(1) {
					i=0;
					sprintf(opt[i++],"%-50.50s%-3.3s","Secure Operation"
						,misc&SECURE ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Swap for Executables"
						,node_swap ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Fuzzy Zone Operation"
						,misc&FUZZY_ZONE ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Store PATH Lines in "
						"Message Base",misc&STORE_SEENBY ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Store SEEN-BY Lines in "
						"Message Base",misc&STORE_PATH ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Store Unknown Kludge Lines "
						"in Message Base",misc&STORE_KLUDGE ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Allow Nodes to Add Areas "
						"in the AREAS.BBS List",misc&ELIST_ONLY?"No":"Yes");
					sprintf(opt[i++],"%-50.50s%-3.3s","Strip Line Feeds "
						"From Outgoing Messages",misc&STRIP_LF ? "Yes":"No");
					sprintf(opt[i++],"%-50.50s%-3.3s","Kill/Ignore Empty NetMail "
						"Messages",misc&KILL_EMPTY_MAIL ? "Yes":"No");
					opt[i][0]=0;
					j=uifc.list(0,0,0,60,&j,0,"Toggle Options",opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							misc^=SECURE;
							break;
						case 1:
							if(node_swap)
								node_swap=0;
							else
								node_swap=1;
							break;
						case 2:
							misc^=FUZZY_ZONE;
							break;
						case 3:
							misc^=STORE_SEENBY;
							break;
						case 4:
							misc^=STORE_PATH;
							break;
						case 5:
							misc^=STORE_KLUDGE;
							break;
						case 6:
							misc^=ELIST_ONLY;
							break;
						case 7:
							misc^=STRIP_LF;
							break;
						case 8:
							misc^=KILL_EMPTY_MAIL;
							break;
					} 
				}
				break;
			case 8:
	uifc.helpbuf=
	" Archive Programs \r\n\r\n"
	"These are the archiving programs (types) which are available for\r\n"
	"compressing outgoing packets.\r\n";
				i=0;
				while(1) {
					for(j=0;j<cfg.arcdefs;j++)
						sprintf(opt[j],"%-30.30s",cfg.arcdef[j].name);
					opt[j][0]=0;
					i=uifc.list(WIN_ORG|WIN_INS|WIN_DEL|WIN_ACT|WIN_GET|WIN_PUT
						|WIN_INSACT|WIN_DELACT|WIN_XTR
						,0,0,0,&i,0,"Archive Programs",opt);
					if(i==-1)
						break;
					if((i&MSK_ON)==MSK_INS) {
						i&=MSK_OFF;
						str[0]=0;
	uifc.helpbuf=
	" Packer Name \r\n\r\n"
	"This is the identifying name of the archiving program\r\n";
						if(uifc.input(WIN_MID,0,0
							,"Packer Name",str,25,K_EDIT|K_UPPER)<1)
							continue;
						if((cfg.arcdef=(arcdef_t *)REALLOC(cfg.arcdef
							,sizeof(arcdef_t)*(cfg.arcdefs+1)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); }
						for(j=cfg.arcdefs;j>i;j--)
							memcpy(&cfg.arcdef[j],&cfg.arcdef[j-1]
								,sizeof(arcdef_t));
							strcpy(cfg.arcdef[j].name
								,cfg.arcdef[j-1].name);
						cfg.arcdefs++;
						memset(&cfg.arcdef[i],0,sizeof(arcdef_t));
						strcpy(cfg.arcdef[i].name,str);
						continue; }

					if((i&MSK_ON)==MSK_DEL) {
						i&=MSK_OFF;
						cfg.arcdefs--;
						if(cfg.arcdefs<=0) {
							cfg.arcdefs=0;
							continue; }
						for(j=i;j<cfg.arcdefs;j++)
							memcpy(&cfg.arcdef[j],&cfg.arcdef[j+1]
								,sizeof(arcdef_t));
						if((cfg.arcdef=(arcdef_t *)REALLOC(cfg.arcdef
							,sizeof(arcdef_t)*(cfg.arcdefs)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); }
						continue; }
					if((i&MSK_ON)==MSK_GET) {
						i&=MSK_OFF;
						memcpy(&savarcdef,&cfg.arcdef[i],sizeof(arcdef_t));
						continue; }
					if((i&MSK_ON)==MSK_PUT) {
						i&=MSK_OFF;
						memcpy(&cfg.arcdef[i],&savarcdef,sizeof(arcdef_t));
						continue; }
					while(1) {
						j=0;
						sprintf(opt[j++],"%-20.20s %s","Packer Name"
							,cfg.arcdef[i].name);
						sprintf(opt[j++],"%-20.20s %s","Hexadecimal ID"
							,cfg.arcdef[i].hexid);
						sprintf(opt[j++],"%-20.20s %u","Offset to Hex ID"
							,cfg.arcdef[i].byteloc);
						sprintf(opt[j++],"%-20.20s %s","Pack Command Line"
							,cfg.arcdef[i].pack);
						sprintf(opt[j++],"%-20.20s %s","Unpack Command Line"
							,cfg.arcdef[i].unpack);
						opt[j][0]=0;
						sprintf(str,"%.30s",cfg.arcdef[i].name);
						k=uifc.list(WIN_MID|WIN_ACT,0,0,60,&nodeop,0,str,opt);
						if(k==-1)
							break;
						switch(k) {
							case 0:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Packer Name",cfg.arcdef[i].name,25
									,K_EDIT|K_UPPER);
								break;
							case 1:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Hexadecimal ID",cfg.arcdef[i].hexid,25
									,K_EDIT|K_UPPER);
								break;
							case 2:
								sprintf(str,"%u",cfg.arcdef[i].byteloc);
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Offset to Hex ID",str,5
									,K_NUMBER|K_EDIT);
								cfg.arcdef[i].byteloc=atoi(str);
								break;
							case 3:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Pack Command Line",cfg.arcdef[i].pack,50
									,K_EDIT);
								break;
							case 4:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Unpack Command Line",cfg.arcdef[i].unpack,50
									,K_EDIT);
								break;
								} } }
				break;
			case 9:
	uifc.helpbuf=
	" Additional Echo Lists \r\n\r\n"
	"This feature allows you to specify echo lists (in addition to your\r\n"
	"AREAS.BBS file) for SBBSecho to search for area add requests.\r\n";
				i=0;
				while(1) {
					for(j=0;j<cfg.listcfgs;j++)
						sprintf(opt[j],"%-50.50s",cfg.listcfg[j].listpath);
					opt[j][0]=0;
					i=uifc.list(WIN_ORG|WIN_INS|WIN_DEL|WIN_ACT|WIN_GET|WIN_PUT
						|WIN_INSACT|WIN_DELACT|WIN_XTR
						,0,0,0,&i,0,"Additional Echo Lists",opt);
					if(i==-1)
						break;
					if((i&MSK_ON)==MSK_INS) {
						i&=MSK_OFF;
						str[0]=0;
	uifc.helpbuf=
	" Echo List \r\n\r\n"
	"This is the path and filename of the echo list file you wish\r\n"
	"to add.\r\n";
						if(uifc.input(WIN_MID|WIN_SAV,0,0
							,"Echo List Path/Name",str,50,K_EDIT)<1)
							continue;
						if((cfg.listcfg=(echolist_t *)REALLOC(cfg.listcfg
							,sizeof(echolist_t)*(cfg.listcfgs+1)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); }
						for(j=cfg.listcfgs;j>i;j--)
							memcpy(&cfg.listcfg[j],&cfg.listcfg[j-1]
								,sizeof(echolist_t));
						cfg.listcfgs++;
						memset(&cfg.listcfg[i],0,sizeof(echolist_t));
						strcpy(cfg.listcfg[i].listpath,str);
						continue; }

					if((i&MSK_ON)==MSK_DEL) {
						i&=MSK_OFF;
						cfg.listcfgs--;
						if(cfg.listcfgs<=0) {
							cfg.listcfgs=0;
							continue; }
						for(j=i;j<cfg.listcfgs;j++)
							memcpy(&cfg.listcfg[j],&cfg.listcfg[j+1]
								,sizeof(echolist_t));
						if((cfg.listcfg=(echolist_t *)REALLOC(cfg.listcfg
							,sizeof(echolist_t)*(cfg.listcfgs)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); }
						continue; }
					if((i&MSK_ON)==MSK_GET) {
						i&=MSK_OFF;
						memcpy(&savlistcfg,&cfg.listcfg[i],sizeof(echolist_t));
						continue; }
					if((i&MSK_ON)==MSK_PUT) {
						i&=MSK_OFF;
						memcpy(&cfg.listcfg[i],&savlistcfg,sizeof(echolist_t));
						continue; }
					while(1) {
						j=0;
						sprintf(opt[j++],"%-20.20s %.19s","Echo List Path/Name"
							,cfg.listcfg[i].listpath);
						sprintf(opt[j++],"%-20.20s %s","Hub Address"
							,(cfg.listcfg[i].forward.zone) ?
							 wcfaddrtoa(&cfg.listcfg[i].forward) : "None");
						sprintf(opt[j++],"%-20.20s %s","Forward Password"
							,(cfg.listcfg[i].password[0]) ?
							 cfg.listcfg[i].password : "None");
						sprintf(opt[j++],"%-20.20s %s","Forward Requests"
							,(cfg.listcfg[i].misc&NOFWD) ? "No" : "Yes");
						str[0]=0;
						for(k=0;k<cfg.listcfg[i].numflags;k++) {
							strcat(str,cfg.listcfg[i].flag[k].flag);
							strcat(str," "); }
						sprintf(opt[j++],"%-20.20s %s","Echo List Flags",str);
						opt[j][0]=0;
						k=uifc.list(WIN_MID|WIN_ACT,0,0,60,&nodeop,0,"Echo List",opt);
						if(k==-1)
							break;
						switch(k) {
							case 0:
								strcpy(str,cfg.listcfg[i].listpath);
								if(uifc.input(WIN_MID|WIN_SAV,0,0
									,"Echo List Path/Name",str,50
									,K_EDIT)<1)
									continue;
								strcpy(cfg.listcfg[i].listpath,str);
								break;
							case 1:
								if(cfg.listcfg[i].forward.zone)
									strcpy(str,wcfaddrtoa(&cfg.listcfg[i].forward));
								else
									str[0]=0;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Hub Address",str
									,25,K_EDIT);
								if(str[0])
									cfg.listcfg[i].forward=atofaddr(str);
								else
									memset(&cfg.listcfg[i].forward,0
										,sizeof(faddr_t));
								break;
							case 2:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Password to use when forwarding requests"
									,cfg.listcfg[i].password,25,K_EDIT|K_UPPER);
								break;
							case 3:
								cfg.listcfg[i].misc^=NOFWD;
								if(cfg.listcfg[i].misc&NOFWD)
									cfg.listcfg[i].password[0]=0;
								break;
							case 4:
								while(1) {
									for(j=0;j<cfg.listcfg[i].numflags;j++)
										strcpy(opt[j],cfg.listcfg[i].flag[j].flag);
									opt[j][0]=0;
									x=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|
										WIN_XTR|WIN_INSACT|WIN_DELACT|WIN_RHT
										,0,0,0,&x,0,"Echo List Flags",opt);
									if(x==-1)
										break;
									if((x&MSK_ON)==MSK_INS) {
										x&=MSK_OFF;
										str[0]=0;
	uifc.helpbuf=
	" Echo List Flag \r\n\r\n"
	"These flags determine which nodes have access to the current\r\n"
	"echolist file\r\n";
										if(uifc.input(WIN_MID|WIN_SAV,0,0
											,"Echo List Flag",str,4
											,K_EDIT|K_UPPER)<1)
											continue;
										if((cfg.listcfg[i].flag=(flag_t *)
											REALLOC(cfg.listcfg[i].flag
											,sizeof(flag_t)*
											(cfg.listcfg[i].numflags+1)))==NULL) {
											printf("\nMemory Allocation Error\n");
											exit(1); }
										for(j=cfg.listcfg[i].numflags;j>x;j--)
											memcpy(&cfg.listcfg[i].flag[j]
												,&cfg.listcfg[i].flag[j-1]
												,sizeof(flag_t));
										cfg.listcfg[i].numflags++;
										memset(&cfg.listcfg[i].flag[x].flag
											,0,sizeof(flag_t));
										strcpy(cfg.listcfg[i].flag[x].flag,str);
										continue; }

									if((x&MSK_ON)==MSK_DEL) {
										x&=MSK_OFF;
										cfg.listcfg[i].numflags--;
										if(cfg.listcfg[i].numflags<=0) {
											cfg.listcfg[i].numflags=0;
											continue; }
										for(j=x;j<cfg.listcfg[i].numflags;j++)
											strcpy(cfg.listcfg[i].flag[j].flag
												,cfg.listcfg[i].flag[j+1].flag);
										if((cfg.listcfg[i].flag=(flag_t *)
											REALLOC(cfg.listcfg[i].flag
											,sizeof(flag_t)*
											(cfg.listcfg[i].numflags)))==NULL) {
											printf("\nMemory Allocation Error\n");
											exit(1); }
										continue; }
									strcpy(str,cfg.listcfg[i].flag[x].flag);
	uifc.helpbuf=
	" Echo List Flag \r\n\r\n"
	"These flags determine which nodes have access to the current\r\n"
	"echolist file\r\n";
										uifc.input(WIN_MID|WIN_SAV,0,0,"Echo List Flag"
											,str,4,K_EDIT|K_UPPER);
										strcpy(cfg.listcfg[i].flag[x].flag,str);
										continue; }
								break;
								} } }
				break;

			case -1:
	uifc.helpbuf=
	" Save Configuration File \r\n\r\n"
	"Select Yes to save the config file, No to quit without saving,\r\n"
	"or hit  ESC  to go back to the menu.\r\n\r\n";
				i=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=uifc.list(WIN_MID,0,0,0,&i,0,"Save Config File",opt);
				if(i==-1) break;
				if(i) {uifc.bail(); exit(0);}
				if((stream=fnopen(NULL,cfg.cfgfile,O_CREAT|O_TRUNC|O_WRONLY))==NULL) {
					lprintf("Error %d opening %s\n",errno,cfg.cfgfile);
					uifc.bail(); exit(1); 
				}
				if(!node_swap)
					fprintf(stream,"NOSWAP\n");
				if(cfg.notify)
					fprintf(stream,"NOTIFY %u\n",cfg.notify);
				if(misc&SECURE)
					fprintf(stream,"SECURE_ECHOMAIL\n");
				if(misc&KILL_EMPTY_MAIL)
					fprintf(stream,"KILL_EMPTY\n");
				if(misc&STORE_SEENBY)
					fprintf(stream,"STORE_SEENBY\n");
				if(misc&STORE_PATH)
					fprintf(stream,"STORE_PATH\n");
				if(misc&STORE_KLUDGE)
					fprintf(stream,"STORE_KLUDGE\n");
				if(misc&FUZZY_ZONE)
					fprintf(stream,"FUZZY_ZONE\n");
				if(misc&FLO_MAILER)
					fprintf(stream,"FLO_MAILER\n");
				if(misc&ELIST_ONLY)
					fprintf(stream,"ELIST_ONLY\n");
				if(misc&STRIP_LF)
					fprintf(stream,"STRIP_LF\n");

				if(cfg.areafile[0])
					fprintf(stream,"AREAFILE %s\n",cfg.areafile);
				if(cfg.logfile[0])
					fprintf(stream,"LOGFILE %s\n",cfg.logfile);
				if(cfg.log!=LOG_DEFAULTS) {
					if(cfg.log==0xffffffffUL)
						fprintf(stream,"LOG ALL\n");
					else if(cfg.log==0L)
						fprintf(stream,"LOG NONE\n");
					else
						fprintf(stream,"LOG %08lX\n",cfg.log); }
				if(cfg.inbound[0])
					fprintf(stream,"INBOUND %s\n",cfg.inbound);
				if(cfg.secure[0])
					fprintf(stream,"SECURE_INBOUND %s\n",cfg.secure);
				if(cfg.outbound[0])
					fprintf(stream,"OUTBOUND %s\n",cfg.outbound);
				if(cfg.maxbdlsize!=DFLT_BDL_SIZE)
					fprintf(stream,"ARCSIZE %lu\n",cfg.maxbdlsize);
				if(cfg.maxpktsize!=DFLT_PKT_SIZE)
					fprintf(stream,"PKTSIZE %lu\n",cfg.maxpktsize);
				for(i=j=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].attr&SEND_NOTIFY) {
						if(!j) fprintf(stream,"SEND_NOTIFY");
						fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[i].faddr));
						j++; }
				if(j) fprintf(stream,"\n");
				for(i=j=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].attr&ATTR_HOLD) {
						if(!j) fprintf(stream,"HOLD");
						fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[i].faddr));
						j++; }
				if(j) fprintf(stream,"\n");
				for(i=j=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].attr&ATTR_DIRECT) {
						if(!j) fprintf(stream,"DIRECT");
						fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[i].faddr));
						j++; }
				if(j) fprintf(stream,"\n");
				for(i=j=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].attr&ATTR_CRASH) {
						if(!j) fprintf(stream,"CRASH");
						fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[i].faddr));
						j++; }
				if(j) fprintf(stream,"\n");
				for(i=j=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].attr&ATTR_PASSIVE) {
						if(!j) fprintf(stream,"PASSIVE");
						fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[i].faddr));
						j++; }
				if(j) fprintf(stream,"\n");

				for(i=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].pktpwd[0])
						fprintf(stream,"PKTPWD %s %s\n"
							,wcfaddrtoa(&cfg.nodecfg[i].faddr),cfg.nodecfg[i].pktpwd);

				for(i=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].pkt_type)
						fprintf(stream,"PKTTYPE %s %s\n"
							,cfg.nodecfg[i].pkt_type==PKT_TWO_TWO ? "2.2":"2"
							,wcfaddrtoa(&cfg.nodecfg[i].faddr));

				for(i=0;i<cfg.arcdefs;i++)
					fprintf(stream,"PACKER %s %u %s\n  PACK %s\n"
						"  UNPACK %s\nEND\n"
						,cfg.arcdef[i].name
						,cfg.arcdef[i].byteloc
						,cfg.arcdef[i].hexid
						,cfg.arcdef[i].pack
						,cfg.arcdef[i].unpack
						);
				for(i=0;i<cfg.arcdefs;i++) {
					for(j=k=0;j<cfg.nodecfgs;j++)
						if(cfg.nodecfg[j].arctype==i) {
							if(!k)
								fprintf(stream,"%-10s %s","USEPACKER"
									,cfg.arcdef[i].name);
							k++;
							fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[j].faddr)); }
					if(k)
						fprintf(stream,"\n"); }

				for(i=j=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].arctype==0xffff) {
						if(!j)
							fprintf(stream,"%-10s %s","USEPACKER","NONE");
						j++;
						fprintf(stream," %s",wcfaddrtoa(&cfg.nodecfg[i].faddr)); }
				if(j)
					fprintf(stream,"\n");

				for(i=0;i<cfg.listcfgs;i++) {
					fprintf(stream,"%-10s","ECHOLIST");
					if(cfg.listcfg[i].password[0])
						fprintf(stream," FORWARD %s %s"
							,wcfaddrtoa(&cfg.listcfg[i].forward)
							,cfg.listcfg[i].password);
					else if(cfg.listcfg[i].misc&NOFWD &&
						cfg.listcfg[i].forward.zone)
						fprintf(stream," HUB %s"
							,wcfaddrtoa(&cfg.listcfg[i].forward));
					fprintf(stream," %s",cfg.listcfg[i].listpath);
					for(j=0;j<cfg.listcfg[i].numflags;j++)
						fprintf(stream," %s",cfg.listcfg[i].flag[j].flag);
					fprintf(stream,"\n"); }

				for(i=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].password[0]) {
						fprintf(stream,"%-10s %s %s","AREAFIX"
							,wcfaddrtoa(&cfg.nodecfg[i].faddr)
							,cfg.nodecfg[i].password);
						for(j=0;j<cfg.nodecfg[i].numflags;j++)
							fprintf(stream," %s",cfg.nodecfg[i].flag[j].flag);
						fprintf(stream,"\n"); }

				for(i=0;i<cfg.nodecfgs;i++)
					if(cfg.nodecfg[i].route.zone) {
						fprintf(stream,"%-10s %s","ROUTE_TO"
							,wcfaddrtoa(&cfg.nodecfg[i].route));
						fprintf(stream," %s"
							,wcfaddrtoa(&cfg.nodecfg[i].faddr));
						for(j=i+1;j<cfg.nodecfgs;j++)
							if(!memcmp(&cfg.nodecfg[j].route,&cfg.nodecfg[i].route
								,sizeof(faddr_t))) {
								fprintf(stream," %s"
									,wcfaddrtoa(&cfg.nodecfg[j].faddr));
								cfg.nodecfg[j].route.zone=0; }
						fprintf(stream,"\n"); }

				fclose(stream);
				uifc.bail();
				exit(0);
		}
	}
}
