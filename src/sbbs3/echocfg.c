/* SBBSecho configuration utility 											*/

/* $Id$ */

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

/* Portions written by Allen Christiansen 1994-1996 						*/

#include <stdio.h>

#undef JAVASCRIPT

/* XPDEV Headers */
#include "gen_defs.h"

#define __COLORS
#include "ciolib.h"
#include "uifc.h"
#include "sbbs.h"
#include "sbbsecho.h"

char **opt;

sbbsecho_cfg_t cfg;
uifcapi_t uifc;

void bail(int code)
{

	if(uifc.bail!=NULL)
		uifc.bail();
	exit(code);
}

/* These correlate with the LOG_* definitions in syslog.h/gen_defs.h */
static char* logLevelStringList[] 
	= {"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Informational", "Debugging", NULL};

int main(int argc, char **argv)
{
	char str[256],*p;
	int i,j,k,x,dflt,nodeop=0,packop=0,listop=0;
	echolist_t savlistcfg;
	nodecfg_t savnodecfg;
	arcdef_t savarcdef;
	BOOL door_mode=FALSE;
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	unsigned int u;
	char	sysop_aliases[256];
	sbbsecho_cfg_t orig_cfg;

	fprintf(stderr,"\nSBBSecho Configuration  Version %u.%02u  Copyright %s "
		"Rob Swindell\n\n",SBBSECHO_VERSION_MAJOR, SBBSECHO_VERSION_MINOR, __DATE__+7);

	memset(&cfg,0,sizeof(cfg));
	str[0]=0;
	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-')
			switch(toupper(argv[i][1])) {
                case 'D':
					printf("NOTICE: The -d option is deprecated, use -id instead\r\n");
					SLEEP(2000);
                    door_mode=TRUE;
                    break;
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
                    break;
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
                    break;
				case 'I':
					switch(toupper(argv[i][2])) {
						case 'A':
							ciolib_mode=CIOLIB_MODE_ANSI;
							break;
						case 'C':
							ciolib_mode=CIOLIB_MODE_CURSES;
							break;
						case 0:
							printf("NOTICE: The -i option is deprecated, use -if instead\r\n");
							SLEEP(2000);
						case 'F':
							ciolib_mode=CIOLIB_MODE_CURSES_IBM;
							break;
						case 'X':
							ciolib_mode=CIOLIB_MODE_X;
							break;
						case 'W':
							ciolib_mode=CIOLIB_MODE_CONIO;
							break;
						case 'D':
		                    door_mode=TRUE;
		                    break;
						default:
							goto USAGE;
					}
					break;
		        case 'M':   /* Monochrome mode */
        			uifc.mode|=UIFC_MONO;
                    break;
                case 'C':
        			uifc.mode|=UIFC_COLOR;
                    break;
                case 'V':
                    textmode(atoi(argv[i]+2));
                    break;
                default:
					USAGE:
                    printf("\nusage: echocfg [path/to/sbbsecho.ini] [options]"
                        "\n\noptions:\n\n"
                        "-c  =  force color mode\r\n"
						"-m  =  force monochrome mode\r\n"
                        "-e# =  set escape delay to #msec\r\n"
						"-iX =  set interface mode to X (default=auto) where X is one of:\r\n"
#ifdef __unix__
						"       X = X11 mode\r\n"
						"       C = Curses mode\r\n"
						"       F = Curses mode with forced IBM charset\r\n"
#else
						"       W = Win32 native mode\r\n"
#endif
						"       A = ANSI mode\r\n"
						"       D = standard input/output/door mode\r\n"
                        "-v# =  set video mode to # (default=auto)\r\n"
                        "-l# =  set screen lines to # (default=auto-detect)\r\n"
                        );
        			exit(0);
		}
		else
			strcpy(str,argv[1]);
	}
	if(str[0]==0) {
		p=getenv("SBBSCTRL");
		if(!p) {
			p=getenv("SBBSNODE");
			if(!p) {
				goto USAGE;
				exit(1); 
			}
			strcpy(str,p);
			backslash(str);
			strcat(str,"../ctrl/sbbsecho.ini"); 
		}
		else {
			strcpy(str,p);
			backslash(str);
			strcat(str,"sbbsecho.ini"); 
		} 
	}
	SAFECOPY(cfg.cfgfile,str);

	if(!sbbsecho_read_ini(&cfg)) {
		fprintf(stderr, "ERROR %d (%s) reading %s\n", errno, strerror(errno), cfg.cfgfile);
		exit(1);
	}
	orig_cfg = cfg;

	// savnum=0;
	if((opt=(char **)malloc(sizeof(char *)*1000))==NULL) {
		puts("memory allocation error\n");
		exit(1); 
	}
	for(i=0;i<1000;i++)
		if((opt[i]=(char *)malloc(MAX_OPLN+1))==NULL) {
			puts("memory allocation error\n");
			exit(1); 
		}
	uifc.size=sizeof(uifc);
	if(!door_mode) {
		i=initciolib(ciolib_mode);
		if(i!=0) {
    		printf("ciolib library init returned error %d\n",i);
    		exit(1);
		}
    	i=uifcini32(&uifc);  /* curses/conio/X/ANSI */
	}
	else
    	i=uifcinix(&uifc);  /* stdio */

	if(i!=0) {
		printf("uifc library init returned error %d\n",i);
		exit(1);
	}

	uifc.timedisplay = NULL;
	sprintf(str,"SBBSecho Config v%u.%02u",SBBSECHO_VERSION_MAJOR, SBBSECHO_VERSION_MINOR);
	uifc.scrn(str);
	p=cfg.cfgfile;
	if(strlen(p) + strlen(str) + 4 > uifc.scrn_width)
		p=getfname(cfg.cfgfile);
	uifc.printf(uifc.scrn_width-(strlen(p)+1),1,uifc.bclr|(uifc.cclr<<4),p);

	dflt=0;
	while(1) {
		if(memcmp(&cfg, &orig_cfg, sizeof(cfg)) != 0)
			uifc.changes = TRUE;
		uifc.helpbuf=
	"~ SBBSecho Configuration ~\r\n\r\n"
	"This program allows you to easily configure the Synchronet BBS\r\n"
	"FidoNet-style EchoMail program known as `SBBSecho`.  Alternatively, you\r\n"
	"may edit the SBBSecho configuration file (e.g. `ctrl/sbbsecho.ini`) using\r\n"
	"an ASCII/plain-text editor.\r\n"
	"\r\n"
	"For detailed documentation, see `http://wiki.synchro.net/util:sbbsecho`\r\n"
	"\r\n"
	"`Mailer Type` should normally be set to `Binkley/FLO` to enable SBBSecho's\r\n"
	"\"Binkley-Style Outbound\" operating mode (a.k.a. `BSO` or `FLO` mode).\r\n"
	"If you are using an `Attach`, `ArcMail`, or `FrontDoor` style FidoNet\r\n"
	"mailer, then set this setting to `ArcMail/Attach`, but know that most\r\n"
	"modern FidoNet mailers are Binkley-Style and therefore that mode of\r\n"
	"operation in SBBSecho is more widely tested and supported.\r\n"
	"\r\n"
	"`Log Level` should normally be set to `Informational` but if you're\r\n"
	"experiencing problems with SBBSecho and would like more verbose log\r\n"
	"output, set this to `Debugging`. If you want less verbose logging,\r\n"
	"set to higher-severity levels to reduce the number of log messages.\r\n"
	"\r\n"
	"The `Linked Nodes` sub-menu is where you configure your FidoNet-style\r\n"
	"links: other FidoNet-style nodes/systems you directly connect with.\r\n"
	"\r\n"
	"The `Archive Types` sub-menu is where you configure your archive\r\n"
	"programs (a.k.a. \"packers\") used for the packing and unpacking of\r\n"
	"EchoMail bundle files (usually in 'zip' format).\r\n"
	"\r\n"
	"The `NetMail Settings` sub-menu is where you configure NetMail-specific\r\n"
	"settings.\r\n"
	"\r\n"
	"The `EchoMail Settings` sub-menu is where you configure EchoMail-specific\r\n"
	"settings.\r\n"
	"\r\n"
	"The `Paths and Filenames` sub-menu is where you configure your system's\r\n"
	"directory and file paths used by SBBSecho.\r\n"
	"\r\n"
	"The `Additional EchoLists` sub-menu is for configuring additional\r\n"
	"(optional) lists of FidoNet-style message areas in `BACKBONE.NA` format.\r\n"
	"These lists, if configured, are used in addition to your main\r\n"
	"`Area File` (e.g. areas.bbs) for advanced AreaFix/AreaMgr operations."
	;
		i=0;
		sprintf(opt[i++],"%-25s %s","Mailer Type"
			,cfg.flo_mailer ? "Binkley/FLO":"ArcMail/Attach");
		sprintf(opt[i++],"%-25s %s","Log Level",logLevelStringList[cfg.log_level]);
		sprintf(opt[i++],"Linked Nodes...");
		sprintf(opt[i++],"Archive Types...");
		sprintf(opt[i++],"NetMail Settings...");
		sprintf(opt[i++],"EchoMail Settings...");
		sprintf(opt[i++],"Paths and Filenames...");
		sprintf(opt[i++],"Additional EchoLists...");
		if(uifc.changes)
			snprintf(opt[i++],MAX_OPLN-1,"Save Changes to %s", getfname(cfg.cfgfile));
		opt[i][0]=0;
		switch(uifc.list(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC,0,0,45,&dflt,0
			,"Configure SBBSecho",opt)) {

			case 0:
				cfg.flo_mailer = !cfg.flo_mailer;
				break;

			case 1:
	uifc.helpbuf=
	"~ Log Level ~\r\n"
	"\r\n"
	"Select the minimum severity of log entries to be logged to the log file.\r\n"
	"The default/normal setting is `Informational`.";
				j=cfg.log_level;
				i=uifc.list(WIN_MID,0,0,0,&j,0,"Log Level",logLevelStringList);
				if(i>=0 && i<=LOG_DEBUG)
					cfg.log_level=i;
				break;


			case 2:
				i=0;
				while(1) {
					uifc.helpbuf=
	"~ Linked Nodes ~\r\n\r\n"
	"From this menu you can configure the settings for your linked\r\n"
	"FidoNet-style nodes (uplinks and downlinks).\r\n"
	"\r\n"
	"A single node configuration can represent one node or a collection\r\n"
	"of nodes, by using the `ALL` wildcard word."
	;

					for(u=0;u<cfg.nodecfgs;u++)
						snprintf(opt[u], MAX_OPLN-1, "%-23s %s"
							,faddrtoa(&cfg.nodecfg[u].addr)
							,cfg.nodecfg[u].comment);
					opt[u][0]=0;
					i=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|WIN_GET|WIN_PUT
						|WIN_INSACT|WIN_DELACT|WIN_XTR
						,0,0,0,&i,0,"Linked Nodes",opt);
					if(i==-1)
						break;
					if((i&MSK_ON)==MSK_INS) {
						i&=MSK_OFF;
						str[0]=0;
	uifc.helpbuf=
	"~ Address ~\r\n\r\n"
	"This is the FidoNet style address of the node you wish to add (4D).\r\n";
						if(uifc.input(WIN_MID|WIN_SAV,0,0
							,"Node Address (ALL wildcard allowed)",str
							,25,K_EDIT)<1)
							continue;
						if((cfg.nodecfg=(nodecfg_t *)realloc(cfg.nodecfg
							,sizeof(nodecfg_t)*(cfg.nodecfgs+1)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); 
						}
						for(j=cfg.nodecfgs;j>i;j--)
							memcpy(&cfg.nodecfg[j],&cfg.nodecfg[j-1]
								,sizeof(nodecfg_t));
						cfg.nodecfgs++;
						memset(&cfg.nodecfg[i],0,sizeof(nodecfg_t));
						cfg.nodecfg[i].addr=atofaddr(str);
						uifc.changes=TRUE;
						continue; 
					}

					if((i&MSK_ON)==MSK_DEL) {
						i&=MSK_OFF;
						cfg.nodecfgs--;
						if(cfg.nodecfgs<=0) {
							cfg.nodecfgs=0;
							continue; 
						}
						for(u=i;u<cfg.nodecfgs;u++)
							memcpy(&cfg.nodecfg[u],&cfg.nodecfg[u+1]
								,sizeof(nodecfg_t));
						if((cfg.nodecfg=(nodecfg_t *)realloc(cfg.nodecfg
							,sizeof(nodecfg_t)*(cfg.nodecfgs)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); 
						}
						uifc.changes=TRUE;
						continue; 
					}
					if((i&MSK_ON)==MSK_GET) {
						i&=MSK_OFF;
						memcpy(&savnodecfg,&cfg.nodecfg[i],sizeof(nodecfg_t));
						continue; 
					}
					if((i&MSK_ON)==MSK_PUT) {
						i&=MSK_OFF;
						memcpy(&cfg.nodecfg[i],&savnodecfg,sizeof(nodecfg_t));
						uifc.changes=TRUE;
						continue; 
					}
					while(1) {
	uifc.helpbuf=
	"~ Linked Node Settings ~\r\n\r\n"
	"These are the settings available for each configured node:\r\n"
	"\r\n"
	"`Address` is the FidoNet-style address in the Zone:Net/Node (3D) or\r\n"
	"Zone:Net/Node.Point (4D) format. The wildcard word '`ALL`' may be used\r\n"
	"in place of one of the fields to create a node configuration which\r\n"
	"will apply to *all* nodes matching that address pattern.\r\n"
	"e.g. '`1:ALL`' matches all nodes within FidoNet Zone 1.\r\n"
	"\r\n"
	"`Comment` is a note to yourself about this node. Setting this to the\r\n"
	"user or sysop name corresponding with the configured node can be\r\n"
	"a helpful reminder to yourself later.\r\n"
	"\r\n"
	"`Archive Type` is the name of an archive type corresponding with one of\r\n"
	"your configured archive types or '`None`'.  This archive type will\r\n"
	"be used when creating EchoMail bundles or if `None`, raw/uncompressed\r\n"
	"EchoMail packets will be sent to this node.\r\n"
	"This setting may be managed by the node using NetMail/AreaFix requests.\r\n"
	"\r\n"
	"`Packet Type` is the type of outbound packet generated for this node.\r\n"
	"Incoming packet types are automatically detected from among the list\r\n"
	"of supported packet types (`2`, `2.2`, `2e`, and `2+`).\r\n"
	"The default outbound packet type is `Type-2+`.\r\n"
	"\r\n"
	"`Packet Password` is an optional password that may be added to outbound\r\n"
	"packets for this node.  Incoming packets from this node must also have\r\n"
	"the same password value if this password is configured (not blank).\r\n"
	"Packet passwords are case insensitive.\r\n"
	"\r\n"
	"`AreaFix Password` is an optional password used to enable AreaFix\r\n"
	"NetMail requests from this node.\r\n"
	"AreaFix Passwords are case insensitive.\r\n"
	"This setting may be managed by the node using NetMail/AreaFix requests.\r\n"
	"\r\n"
	"`AreaFix Keys` is a list of keys which enable access to one or more\r\n"
	"Additional EchoLists.\r\n"
	"\r\n"
	"`Status` is the default mode for sending mail to this node: `Normal`, `Hold`\r\n"
	"(wait for pickup) or `Crash` (immediate).\r\n"
	"\r\n"
	"`Direct` determines whether to connect to this node directly (whenever\r\n"
	"possible) when sending mail to this node.\r\n"
	"\r\n"
	"`Passive` is used to temporarily disable the packing and sending of\r\n"
	"EchoMail for a node.  The opposite of Passive is `Active`.\r\n"
	"This setting may be managed by the node using NetMail/AreaFix requests.\r\n"
	"\r\n"
	"`Send Notify List` is used to flag nodes that you want notified via\r\n"
	"NetMail of their current AreaFix settings whenever SBBSecho is run\r\n"
	"with the '`G`' option.\r\n"
	"\r\n"
	"`Route To` is only used in Binkley-Style Outbound (BSO/FLO) operating\r\n"
	"mode and is used to set the FidoNet address to route mail for this node.\r\n"
	"\r\n"
	"`Inbox Directory` is only used in BSO operating mode and is an optional\r\n"
	"alternate directory to search for incoming files from this node (e.g.\r\n"
	"used in combination with BinkD's ibox setting).\r\n"
	"\r\n"
	"`Outbox Directory` is only used in BSO operating mode and is an optional\r\n"
	"alternate directory to place outbound files for this node (e.g. used\r\n"
	"in combination with BinkD's obox setting).\r\n"
	;
						j=0;
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Address"
							,faddrtoa(&cfg.nodecfg[i].addr));
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Comment"
							,cfg.nodecfg[i].comment);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Archive Type"
							,cfg.nodecfg[i].archive ==  SBBSECHO_ARCHIVE_NONE ?
							"None":cfg.nodecfg[i].archive->name);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Packet Type"
							,pktTypeStringList[cfg.nodecfg[i].pkt_type]);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Packet Password"
							,cfg.nodecfg[i].pktpwd);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","AreaFix Password"
							,cfg.nodecfg[i].password);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","AreaFix Keys"
							,strListCombine(cfg.nodecfg[i].keys,str,sizeof(str),","));
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Status"
							,mailStatusStringList[cfg.nodecfg[i].status]);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Direct"
							,cfg.nodecfg[i].direct ? "Yes":"No");
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Passive"
							,cfg.nodecfg[i].passive ? "Yes":"No");
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Send Notify List"
							,cfg.nodecfg[i].send_notify ? "Yes" : "No");
						if(cfg.flo_mailer) {
							snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Route To"
								,cfg.nodecfg[i].route.zone
								? faddrtoa(&cfg.nodecfg[i].route) : "Disabled");
							snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s", "Inbox Directory", cfg.nodecfg[i].inbox);
							snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s", "Outbox Directory", cfg.nodecfg[i].outbox);
						}
						opt[j][0]=0;
						SAFEPRINTF(str, "Linked Node - %s"
							,cfg.nodecfg[i].comment[0] ? cfg.nodecfg[i].comment : faddrtoa(&cfg.nodecfg[i].addr));
						k=uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,60,&nodeop,0,str,opt);
						if(k==-1)
							break;
						switch(k) {
							case 0:
	uifc.helpbuf=
	"~ Address ~\r\n\r\n"
	"This is the FidoNet style address of this linked node.\r\n";
								strcpy(str,faddrtoa(&cfg.nodecfg[i].addr));
								if(uifc.input(WIN_MID|WIN_SAV,0,0
									,"Node Address (ALL wildcard allowed)",str
									,25,K_EDIT|K_UPPER)>0)
									cfg.nodecfg[i].addr=atofaddr(str);
								break;
							case 1:
	uifc.helpbuf=
	"~ Comment ~\r\n\r\n"
	"This is an optional comment for the node (e.g. the sysop's name).\r\n"
	"This is used for informational purposes only.\r\n";
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Comment"
									,cfg.nodecfg[i].comment,sizeof(cfg.nodecfg[i].comment)-1
									,K_EDIT);
								break;
							case 2:
	uifc.helpbuf=
	"~ Archive Type ~\r\n\r\n"
	"This is the archive type that will be used for compressing packets\r\n"
	"into archive bundles for this node.\r\n";
								int cur=cfg.arcdefs;
								for(u=0;u<cfg.arcdefs;u++) {
									if(cfg.nodecfg[i].archive == &cfg.arcdef[u])
										cur=u;
									strcpy(opt[u],cfg.arcdef[u].name);
								}
								strcpy(opt[u++],"None");
								opt[u][0]=0;
								k=uifc.list(WIN_RHT|WIN_SAV,0,0,0,&cur,0
									,"Archive Type",opt);
								if(k==-1)
									break;
								if((unsigned)k>=cfg.arcdefs)
									cfg.nodecfg[i].archive = SBBSECHO_ARCHIVE_NONE;
								else
									cfg.nodecfg[i].archive = &cfg.arcdef[k];
								uifc.changes=TRUE;
								break;
							case 3:
	uifc.helpbuf=
	"~ Packet Type ~\r\n\r\n"
	"This is the packet header type that will be used in mail packets\r\n"
	"created for this node.  SBBSecho defaults to creating `Type-2+` packets.\r\n"
	"\r\n"
	"`Type-2  ` packets are defined in FTS-0001.16 (Stone Age)\r\n"
	"`Type-2e ` packets are defined in FSC-0039.04 (Sometimes called 2+)\r\n"
	"`Type-2+ ` packets are defined in FSC-0048.02 (4D address support)\r\n"
	"`Type-2.2` packets are defined in FSC-0045.01 (5D address support)\r\n"
	;
								j=cfg.nodecfg[i].pkt_type;
								k=uifc.list(WIN_RHT|WIN_SAV,0,0,0,&j,0,"Packet Type"
									,pktTypeStringList);
								if(k==-1)
									break;
								cfg.nodecfg[i].pkt_type=k;
								uifc.changes=TRUE;
								break;
							case 4:
	uifc.helpbuf=
	"~ Packet Password ~\r\n\r\n"
	"This is an optional password that SBBSecho will place into packets\r\n"
	"destined for this node.\r\n";
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Packet Password (optional)"
									,cfg.nodecfg[i].pktpwd,sizeof(cfg.nodecfg[i].pktpwd)-1
									,K_EDIT|K_UPPER);
								break;
							case 5:
	uifc.helpbuf=
	"~ AreaFix Password ~\r\n\r\n"
	"This is the password that will be used by this node when doing remote\r\n"
	"AreaManager / AreaFix functions.\r\n";
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"AreaFix Password"
									,cfg.nodecfg[i].password,sizeof(cfg.nodecfg[i].password)-1
									,K_EDIT|K_UPPER);
								break;
							case 6:
	uifc.helpbuf=
	"~ AreaFix Keys ~\r\n\r\n"
	"This is a named-key to to be given to this node allowing access to one or\r\n"
	"more of the configured echolists\r\n";
								while(1) {
									for(j=0; cfg.nodecfg[i].keys!=NULL && cfg.nodecfg[i].keys[j]!=NULL ;j++)
										strcpy(opt[j],cfg.nodecfg[i].keys[j]);
									opt[j][0]=0;
									k=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|
										WIN_XTR|WIN_INSACT|WIN_DELACT|WIN_RHT
										,0,0,0,&k,0,"AreaFix Keys",opt);
									if(k==-1)
										break;
									if((k&MSK_ON)==MSK_INS) {
										k&=MSK_OFF;
										if(uifc.input(WIN_MID|WIN_SAV,0,0
											,"AreaFix Key",str,SBBSECHO_MAX_KEY_LEN
											,K_UPPER)<1)
											continue;
										strListInsert(&cfg.nodecfg[i].keys, str, k);
										uifc.changes=TRUE;
										continue; 
									}

									if((k&MSK_ON)==MSK_DEL) {
										k&=MSK_OFF;
										strListRemove(&cfg.nodecfg[i].keys, k);
										uifc.changes=TRUE;
										continue; 
									}
									SAFECOPY(str,cfg.nodecfg[i].keys[k]);
									uifc.input(WIN_MID|WIN_SAV,0,0,"AreaFix Key"
										,str,SBBSECHO_MAX_KEY_LEN,K_EDIT|K_UPPER);
									strListReplace(cfg.nodecfg[i].keys, k, str);
									uifc.changes=TRUE;
									continue; 
								}
								break;
							case 7:
	uifc.helpbuf=
	"~ Mail Status ~\r\n\r\n"
	"Set the mail status for this node: `Normal`, `Hold`, or `Crash`.\r\n";
								j=cfg.nodecfg[i].status;
								k=uifc.list(WIN_RHT|WIN_SAV,0,0,0,&j,0,"Mail Status"
									,mailStatusStringList);
								if(k==-1)
									break;
								if(cfg.nodecfg[i].status!=k) {
									cfg.nodecfg[i].status=k;
									uifc.changes=TRUE;
								}
								break;
							case 8:
								k = !cfg.nodecfg[i].direct;
								switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
									,"Direct Delivery",uifcYesNoOpts)) {
									case 0:	cfg.nodecfg[i].direct = true;	uifc.changes=TRUE; break;
									case 1:	cfg.nodecfg[i].direct = false;	uifc.changes=TRUE; break;
								}
								break;
							case 9:
								k = !cfg.nodecfg[i].passive;
								switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
									,"Passive Node",uifcYesNoOpts)) {
									case 0:	cfg.nodecfg[i].passive = true;	uifc.changes=TRUE; break;
									case 1:	cfg.nodecfg[i].passive = false;	uifc.changes=TRUE; break;
								}
								break;
							case 10:
								k = !cfg.nodecfg[i].send_notify;
								switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
									,"Send AreaFix Notifications",uifcYesNoOpts)) {
									case 0:	cfg.nodecfg[i].send_notify = true;	uifc.changes=TRUE; break;
									case 1:	cfg.nodecfg[i].send_notify = false;	uifc.changes=TRUE; break;
								}
								break;
							case 11:
	uifc.helpbuf=
	"~ Route To ~\r\n\r\n"
	"When using a BSO/FLO type mailer, this is the Fido address to route mail\r\n"
	"for this node(s) to.\r\n"
	"\r\n"
	"This option is normally only used with wildcard type node entries\r\n"
	"(e.g. `ALL`, or `1:ALL`, `2:ALL`, etc.) and is used to route non-direct\r\n"
	"NetMail packets to your uplink node (hub).\r\n";
								strcpy(str,faddrtoa(&cfg.nodecfg[i].route));
								if(uifc.input(WIN_MID|WIN_SAV,0,0
									,"Node Address to Route To",str
									,25,K_EDIT) >= 0) {
									if(str[0])
										cfg.nodecfg[i].route=atofaddr(str);
									else
										cfg.nodecfg[i].route.zone=0;
									uifc.changes=TRUE;
								}
								break;
							case 12:
								uifc.input(WIN_MID|WIN_SAV,0,0,"Inbound FileBox Directory"
									,cfg.nodecfg[i].inbox, sizeof(cfg.nodecfg[i].inbox)-1
									,K_EDIT);
								break;
							case 13:
								uifc.input(WIN_MID|WIN_SAV,0,0,"Outbound FileBox Directory"
									,cfg.nodecfg[i].outbox, sizeof(cfg.nodecfg[i].outbox)-1
									,K_EDIT);
								break;
						} 
					} 
				}
				break;

			case 6:	/* Paths and Filenames... */
				j=0;
				while(1) {
					i=0;
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Non-secure Inbound Directory"
						,cfg.inbound);
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Secure Inbound Directory"
						,cfg.secure_inbound[0] ? cfg.secure_inbound : "<None>");
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Outbound Directory"
						,cfg.outbound);
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Area File"
						,cfg.areafile);
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Bad Area File"
						,cfg.badareafile);
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Log File"
						,cfg.logfile[0] ? cfg.logfile
						: "SCFG->data/sbbsecho.log");
					snprintf(opt[i++],MAX_OPLN-1,"%-30.30s %s","Temporary File Directory"
						,cfg.temp_dir[0] ? cfg.temp_dir
						: "../temp/sbbsecho");
					opt[i][0]=0;
					uifc.helpbuf=
						"~ Paths and Filenames ~\r\n\r\n"
						"From this menu you can configure the paths that SBBSecho will use\r\n"
						"when importing, exporting and logging.\r\n";
					j=uifc.list(WIN_MID|WIN_ACT,0,0,60,&j,0
						,"Paths and Filenames",opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
	uifc.helpbuf=
	"~ Non-secure Inbound Directory ~\r\n\r\n"
	"This is the path where your FTN mailer stores, and where SBBSecho will\r\n"
	"look for, incoming files (potentially including message bundles and\r\n"
	"packets) from unauthenticated (non-secure) mailer sessions."
	;
							uifc.input(WIN_MID|WIN_SAV,0,0,"Non-secure Inbound Directory"
								,cfg.inbound,sizeof(cfg.inbound)-1
								,K_EDIT);
							break;

						case 1:
	uifc.helpbuf=
	"~ Secure Inbound Directory ~\r\n\r\n"
	"This is the path where your FTN mailer stores, and where SBBSecho will\r\n"
	"look for, incoming message bundles and packets for `Secure` (password\r\n"
	"protected) sessions.";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Secure Inbound Directory"
								,cfg.secure_inbound,sizeof(cfg.secure_inbound)-1
								,K_EDIT);
							break;

						case 2:
	uifc.helpbuf=
	"~ Outbound Directory ~\r\n\r\n"
	"This is the path where your FTN mailer will look for, and where SBBSecho\r\n"
	"will place, outgoing message bundles and packets.\r\n"
	"\r\n"
	"In Binkley-Style Outbound mode, this serves as the base directory\r\n"
	"name for special foreign zone and point destination nodes as well."
	;
							uifc.input(WIN_MID|WIN_SAV,0,0,"Outbound Directory"
								,cfg.outbound,sizeof(cfg.outbound)-1
								,K_EDIT);
							break;

						case 3:
	uifc.helpbuf=
	"~ Area File ~\r\n\r\n"
	"This is the path of the file SBBSecho will use as your primary\r\n"
	"list of FidoNet-style message areas (default is `data/areas.bbs`).\r\n"
	"\r\n"
	"Each line in the file defines an FTN message area (echo) of the format:\r\n"
	"\r\n"
	"   <`code`> <`tag`> [[`link`] [`link`] [...]]\r\n"
	"\r\n"
	"Each field is separated by one or more white-space characters:\r\n"
	"\r\n"
	"   `<code>` is the Synchronet `internal code` for the local sub-board\r\n"
	"   `<tag>`  is the network's agreed-upon `echo tag` for the message area\r\n"
	"   `[link]` is an `FTN address` to send and receive messages for this area\r\n"
	"          (there may be many linked nodes for each area)\r\n"
	"\r\n"
	"Notes:\r\n"
	"\r\n"
	" * Only the `<code>` and `<tag>` fields are required\r\n"
	" * The '`<`' and '`>`', '`[`' and '`]`' characters are not part of the syntax\r\n"
	" * Lines beginning with a semicolon (`;`) are ignored (i.e. comments)\r\n"
	" * Leading white-space characters are ignored\r\n"
	" * Blank lines are ignored\r\n"
	" * This file may be import/exported to/from your `Message Areas` in `SCFG`\r\n"
	" * This file may be remotely modified by authorized nodes using `AreaFix`\r\n"
	;
							uifc.input(WIN_MID|WIN_SAV,0,0,"Area File"
								,cfg.areafile,sizeof(cfg.areafile)-1
								,K_EDIT);
							break;

						case 4:
	uifc.helpbuf=
	"~ Bad Area File ~\r\n\r\n"
	"This is the path of the file SBBSecho will use to record the names\r\n"
	"(echo tags) and descriptions of FTN message areas (echoes) that your\r\n"
	"system has received EchoMail for, but does not carry locally (default\r\n"
	"is `data/badarea.lst`).\r\n"
	"\r\n"
	"The descriptions of the areas will only be included if the corresponding\r\n"
	"echo tags can be located in one of your configured `Additional EchoLists`.\r\n"
	"\r\n"
	"The format of the file is the same as `BACKBONE.NA` and suitable for\r\n"
	"importing into a Synchronet Message Group using `SCFG`.\r\n"
	"\r\n"
	"SBBSecho will automatically remove areas from this list when they\r\n"
	"are added to your configuration (SCFG->Message Areas and Area File).\r\n"
	;
							uifc.input(WIN_MID|WIN_SAV,0,0,"Bad Area File"
								,cfg.badareafile,sizeof(cfg.badareafile)-1
								,K_EDIT);
							break;

						case 5:
	uifc.helpbuf=
	"~ Log File ~\r\n\r\n"
	"This is the path of the file SBBSecho will use to log information each time\r\n"
	"it is run (default is `data/sbbsecho.log`)."
	;
							uifc.input(WIN_MID|WIN_SAV,0,0,"Log File"
								,cfg.logfile,sizeof(cfg.logfile)-1
								,K_EDIT);
							break; 

						case 6:
	uifc.helpbuf=
	"~ Temporary File Directory ~\r\n\r\n"
	"This is the directory where SBBSecho will store temporary files that\r\n"
	"it creates and uses during its run-time.\r\n"
	"(default is `../temp/sbbsecho`)."
	;
							uifc.input(WIN_MID|WIN_SAV,0,0,"Temp Dir"
								,cfg.temp_dir,sizeof(cfg.temp_dir)-1
								,K_EDIT);
							break; 
					} 
				}
				break;

			case 4:	/* NetMail Settings */
				j=0;
				while(1) {
					uifc.helpbuf=
	"~ NetMail Settings ~\r\n"
	"\r\n"
	"`Sysop Aliases` is a comma-separated list of names by which the sysop\r\n"
	"    (user #1) may receive NetMail messages, in addition to the alias\r\n"
	"    and real name associated with their BBS user account.\r\n"
	"    This setting defaults to just '`SYSOP`'.\r\n"
	"\r\n"
    "`Default Recipient` is the name of the user account you wish to receive\r\n"
	"    inbound NetMail messages that have been addressed to an unrecognized\r\n"
	"    user name or alias.\r\n"
	"\r\n"
	"`Fuzzy Zone Operation` when set to `Yes`, if SBBSecho receives an inbound\r\n"
	"    netmail with no international zone information, it will compare the\r\n"
	"    net/node of the destination to the net/node information in your AKAs\r\n"
	"    and assume the zone of a matching AKA.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Kill/Ignore Empty NetMail Messages` will instruct SBBSecho to simply\r\n"
	"    discard (not import or export) NetMail messages without any body.\r\n"
	"    This setting defaults to `Yes`.\r\n"
	"\r\n"
	"`Delete Processed NetMail Messages` will instruct SBBSecho to delete\r\n"
	"    NetMail messages/files after they have been sent or imported.\r\n"
	"    When set to `No`, SBBSecho will mark them as Sent or Received instead.\r\n"
	"    This setting defaults to `Yes`.\r\n"
	"\r\n"
	"`Ignore NetMail Destination Address` will instruct SBBSecho to treat\r\n"
	"    all NetMail as though it is destined for one of your systems's FTN\r\n"
	"    addresses (AKAs) and potentially import it.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Ignore Netmail 'Sent' Attribute` will instruct SBBSecho to export\r\n"
	"    NetMail messages even when their 'Sent' attribute flag is set.\r\n"
	"    This setting `should not` be set to `Yes` when `Delete NetMail` is\r\n"
	"    disabled.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Ignore Netmail 'Received' Attribute` will instruct SBBSecho to import\r\n"
	"    NetMail messages even when their 'Received' attribute flag is set.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Ignore NetMail 'Local' Attribute` will instruct SBBSecho to import\r\n"
	"    NetMail messages even when their 'Local' attribute flag is set.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Maximum Age of Imported NetMail` allows you to optionally set an age\r\n"
	"    limit (in days) of NetMail messages that may be imported.\r\n"
	"    This setting defaults to `None` (no maximum age).\r\n"
;
					i=0;
					strListCombine(cfg.sysop_alias_list, sysop_aliases, sizeof(sysop_aliases)-1, ",");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%s", "Sysop Aliases",sysop_aliases);
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%s", "Default Recipient"
						,cfg.default_recipient);
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Fuzzy Zone Operation"
						,cfg.fuzzy_zone ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Kill/Ignore Empty NetMail "
						"Messages",cfg.kill_empty_netmail ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Delete Processed NetMail"
						,cfg.delete_netmail ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Ignore NetMail Destination Address"
						,cfg.ignore_netmail_dest_addr ? "Yes" : "No");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Ignore NetMail 'Sent' Attribute"
						,cfg.ignore_netmail_sent_attr ? "Yes" : "No");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Ignore NetMail 'Received' Attribute"
						,cfg.ignore_netmail_recv_attr ? "Yes" : "No");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%-3.3s","Ignore NetMail 'Local' Attribute"
						,cfg.ignore_netmail_local_attr ? "Yes" : "No");
					if(cfg.max_netmail_age)
						sprintf(str,"%1.0f days", cfg.max_netmail_age / (24.0*60.0*60.0));
					else
						strcpy(str, "None");
					snprintf(opt[i++],MAX_OPLN-1,"%-40.40s%s","Maximum Age of Imported NetMail"	, str);
					opt[i][0]=0;
					j=uifc.list(WIN_ACT,0,0,60,&j,0,"NetMail Settings",opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
							uifc.helpbuf=
							"~ Sysop Aliases ~\r\n\r\n"
							"This is a comma-separated list of additional `To` names that the sysop\r\n"
							"(user #1) can receive netmail by. When specifying multiple aliases,\r\n"
							"they must be separated by a single comma and no extra white-space\r\n"
							"(e.g. \"SYSOP,COORDINATOR\"). The default value is just `SYSOP`.\r\n";
							if(uifc.input(WIN_MID|WIN_BOT|WIN_SAV,0,0,"Sysop Aliases (comma separated)"
								,sysop_aliases
								,sizeof(sysop_aliases)-1,K_EDIT|K_UPPER) >= 0) {
								strListFree(&cfg.sysop_alias_list);
								cfg.sysop_alias_list = strListSplit(NULL, sysop_aliases, ",");
							}
							break;
						case 1:
							uifc.input(WIN_MID|WIN_SAV,0,0,"Default Recipient"
								,cfg.default_recipient, sizeof(cfg.default_recipient)-1
								,K_EDIT|K_UPPER);
							break;
						case 2:
							k = !cfg.fuzzy_zone;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Fuzzy Zone Operation",uifcYesNoOpts)) {
								case 0:	cfg.fuzzy_zone = true;	break;
								case 1:	cfg.fuzzy_zone = false;	break;
							}
							break;
						case 3:
							k = !cfg.kill_empty_netmail;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Delete Empty NetMail",uifcYesNoOpts)) {
								case 0:	cfg.kill_empty_netmail = true;	break;
								case 1:	cfg.kill_empty_netmail = false;	break;
							}
							break;
						case 4:
							k = !cfg.delete_netmail;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Delete Processed NetMail",uifcYesNoOpts)) {
								case 0:	cfg.delete_netmail = true;	break;
								case 1:	cfg.delete_netmail = false;	break;
							}
							break;
						case 5:
							k = !cfg.ignore_netmail_dest_addr;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Ignore NetMail Destination Address",uifcYesNoOpts)) {
								case 0:	cfg.ignore_netmail_dest_addr = true;	break;
								case 1:	cfg.ignore_netmail_dest_addr = false;	break;
							}
							break;
						case 6:
							k = !cfg.ignore_netmail_sent_attr;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Ignore NetMail 'Sent' Attribute",uifcYesNoOpts)) {
								case 0:	cfg.ignore_netmail_sent_attr = true;	break;
								case 1:	cfg.ignore_netmail_sent_attr = false;	break;
							}
							break;
						case 7:
							k = !cfg.ignore_netmail_recv_attr;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Ignore NetMail 'Received' Attribute",uifcYesNoOpts)) {
								case 0:	cfg.ignore_netmail_recv_attr = true;	break;
								case 1:	cfg.ignore_netmail_recv_attr = false;	break;
							}
							break;
						case 8:
							k = !cfg.ignore_netmail_local_attr;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Ignore NetMail 'Local' Attribute",uifcYesNoOpts)) {
								case 0:	cfg.ignore_netmail_local_attr = true;	break;
								case 1:	cfg.ignore_netmail_local_attr = false;	break;
							}
							break;
						case 9:
							uifc.helpbuf=
							"~ Maximum Age of Imported NetMail ~\r\n\r\n"
							"Maximum age (in days) of NetMail that may be imported. The age is based\r\n"
							"on the date supplied in the message header and may be incorrect in some\r\n"
							"conditions (e.g. erroneous software or incorrect system date).\r\n"
							"Set this value to `0` to disable this feature (no maximum age imposed)."
							;
							if(cfg.max_netmail_age)
								sprintf(str,"%1.0f", cfg.max_netmail_age / (24.0*60.0*60.0));
							else
								strcpy(str, "None");
							if(uifc.input(WIN_MID|WIN_BOT|WIN_SAV,0,0,"Maximum NetMail Age (in Days)"
								,str, 5, K_EDIT) >= 0)
								cfg.max_netmail_age = (long) (strtod(str, NULL) * (24.0*60.0*60.0));
							break;

					} 
				}
				break;

			case 5:	/* EchoMail Settings */
				j=0;
				while(1) {
					uifc.helpbuf=
	"~ EchoMail Settings ~\r\n"
	"\r\n"
	"`Area Manager` is the BBS user name or alias to notify (via email) of\r\n"
	"    AreaFix activities and errors.  This setting defaults to `SYSOP`.\r\n"
	"\r\n"
	"`Maximum Packet Size` is the largest packet file size that SBBSecho will\r\n"
	"    normally create (in bytes).\r\n"
	"    This settings defaults to `250K` (250 Kilobytes, or 256,000 bytes).\r\n"
	"\r\n"
	"`Maximum Bundle Size` is the largest bundle file size that SBBSecho will\r\n"
	"    normally create (in bytes).\r\n"
	"    This settings defaults to `250K` (250 Kilobytes, or 256,000 bytes).\r\n"
	"\r\n"
	"`Secure Operation` tells SBBSecho to check the Area File (e.g. areas.bbs)\r\n"
	"    to insure that the packet origin (FTN address) of EchoMail messages\r\n"
	"    is already linked to the EchoMail area where the message was posted.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Notify Users of Received EchoMail` tells SBBSecho to send telegrams\r\n"
	"    (short messages) to BBS users when EchoMail addressed to their name\r\n"
	"    or alias has been imported into a message base that the user has\r\n"
	"    access to read.\r\n"
	"\r\n"
	"`Convert Existing Tear Lines` tells SBBSecho to convert any tear lines\r\n"
	"    (`---`) existing in the message text to `===`.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Allow Nodes to Add Areas from Area File` when set to `Yes` allows linked\r\n"
	"    nodes to add areas listed in your Area File (e.g. `areas.bbs`).\r\n"
	"    This setting defaults to `Yes`.\r\n"
	"\r\n"
	"`Strip Line Feeds From Outgoing Messages` when set to `Yes` instructs\r\n"
	"    SBBSecho to remove any line-feed (ASCII 10) characters from the body\r\n"
	"    text of messages being exported to FidoNet EchoMail.\r\n"
	"    This setting defaults to `No`.\r\n"
	"\r\n"
	"`Circular Path Detection` when `Enabled` will cause SBBSecho, during\r\n"
	"    EchoMail import, to check the PATH kludge lines for any of the\r\n"
	"    system's AKAs and if found (indicating a message loop), not import\r\n"
	"    the message.\r\n"
	"\r\n"
	"`Outbound Bundle Attachments` may be either `Deleted` (killed) or `Truncated`\r\n"
	"    (changed to 0-bytes in length) after being sent by your mailer.\r\n"
	"\r\n"
	"`Zone Blind SEEN-BY and PATH Lines` when `Enabled` will cause SBBSecho\r\n"
	"    to assume that node numbers are not duplicated across zones and\r\n"
	"    that a net/node combination in either of these Kludge lines should\r\n"
	"    be used to identify a specific node regardless of which zone that\r\n"
	"    node is located (thus breaking the rules of FidoNet 3D addressing).\r\n"
	"\r\n"
	"`Maximum Age of Imported EchoMail` allows you to optionally set an age\r\n"
	"    limit (in days) of EchoMail messages that may be imported.\r\n"
	"    This setting defaults to `60` (60 days).\r\n"
	;

					i=0;
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%s", "Area Manager",cfg.areamgr);
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%luK","Maximum Packet Size"
						,cfg.maxpktsize/1024UL);
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%luK","Maximum Bundle Size"
						,cfg.maxbdlsize/1024UL);
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%-3.3s","Secure Operation"
						,cfg.secure_echomail ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%-3.3s","Notify Users of Received EchoMail"
						,cfg.echomail_notify ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%-3.3s","Convert Existing Tear Lines"
						,cfg.convert_tear ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%-3.3s","Allow Nodes to Add Areas "
						"from Area File",cfg.add_from_echolists_only ?"No":"Yes");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%-3.3s","Strip Line Feeds "
						"from Outgoing Messages",cfg.strip_lf ? "Yes":"No");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%s","Circular Path Detection"
						,cfg.check_path ? "Enabled" : "Disabled");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%s","Outbound Bundle Attachments"
						,cfg.trunc_bundles ? "Truncate" : "Delete");
					if(cfg.zone_blind)
						sprintf(str,"Zones 1-%u", cfg.zone_blind_threshold);
					else
						strcpy(str,"Disabled");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%s","Zone Blind SEEN-BY and PATH Lines", str);
					if(cfg.max_echomail_age)
						sprintf(str,"%1.0f days", ((float)cfg.max_echomail_age) / (24.0*60.0*60.0));
					else
						strcpy(str, "None");
					snprintf(opt[i++],MAX_OPLN-1,"%-45.45s%s","Maximum Age of Imported EchoMail", str);
					opt[i][0]=0;
					j=uifc.list(WIN_ACT|WIN_RHT|WIN_BOT,0,0,64,&j,0,"EchoMail Settings",opt);
					if(j==-1)
						break;
					switch(j) {
						case 0:
				uifc.helpbuf=
				"~ Area Manager ~\r\n\r\n"
				"User to notify of AreaFix activity and errors.\r\n";
							uifc.input(WIN_MID|WIN_BOT|WIN_SAV,0,0,"Area Manager (user name or alias)"
								,cfg.areamgr
								,LEN_ALIAS,K_EDIT);
							break;

						case 1:
				uifc.helpbuf=
				"~ Maximum Packet Size ~\r\n\r\n"
				"This is the maximum file size that SBBSecho will create when placing\r\n"
				"outgoing messages into packets.  The default max size is 250 Kilobytes.\r\n";
							sprintf(str,"%lu",cfg.maxpktsize);
							uifc.input(WIN_MID|WIN_BOT|WIN_SAV,0,0,"Maximum Packet Size (in Bytes)",str
								,9,K_EDIT|K_NUMBER);
							cfg.maxpktsize=atol(str);
							break;

						case 2:
				uifc.helpbuf=
				"~ Maximum Bundle Size ~\r\n\r\n"
				"This is the maximum file size that SBBSecho will create when placing\r\n"
				"outgoing packets into bundles.  The default max size is 250 Kilobytes.\r\n";
							sprintf(str,"%lu",cfg.maxbdlsize);
							uifc.input(WIN_MID|WIN_BOT|WIN_SAV,0,0,"Maximum Bundle Size (in Bytes)",str
								,9,K_EDIT|K_NUMBER);
							cfg.maxbdlsize=atol(str);
							break;
						case 3:
							k = !cfg.secure_echomail;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Secure Operation",uifcYesNoOpts)) {
								case 0:	cfg.secure_echomail = true;		break;
								case 1:	cfg.secure_echomail = false;	break;
							}
							break;
						case 4:
							k = !cfg.echomail_notify;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Notify Users",uifcYesNoOpts)) {
								case 0:	cfg.echomail_notify = true;		break;
								case 1:	cfg.echomail_notify = false;	break;
							}
							break;
						case 5:
							k = !cfg.convert_tear;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Convert Tear Lines",uifcYesNoOpts)) {
								case 0:	cfg.convert_tear = true;	break;
								case 1:	cfg.convert_tear = false;	break;
							}
							break;
						case 6:
							k = cfg.add_from_echolists_only;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Allow Add from Area File",uifcYesNoOpts)) {
								case 0:	cfg.add_from_echolists_only = false;	break;
								case 1:	cfg.add_from_echolists_only = true;		break;
							}
							break;
						case 7:
							k = !cfg.strip_lf;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Strip Line Feeds",uifcYesNoOpts)) {
								case 0:	cfg.strip_lf = true;	break;
								case 1:	cfg.strip_lf = false;	break;
							}
							break;
						case 8:
							k = !cfg.check_path;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Circular Path Detection",uifcYesNoOpts)) {
								case 0:	cfg.check_path = true;	break;
								case 1:	cfg.check_path = false;	break;
							}
							break;
						case 9:
						{
							k = cfg.trunc_bundles;
							char* opt[] = {"Delete after Sent", "Truncate after Sent", NULL };
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0
								,"Outbound Bundles",opt)) {
								case 0:	cfg.trunc_bundles = false;	break;
								case 1:	cfg.trunc_bundles = true;	break;
							}
							break;
						}
						case 10:
							k = !cfg.zone_blind;
							switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&k,0,"Zone Blind",uifcYesNoOpts)) {
								case 0:
									cfg.zone_blind = true;
									uifc.helpbuf=
									"Zone Blind Threshold";
									sprintf(str,"%u",cfg.zone_blind_threshold);
									if(uifc.input(WIN_MID|WIN_SAV,0,0
										,"Zone Blind Threshold (highest zone in the blind range)"
										, str, 5, K_EDIT|K_NUMBER) >= 0)
										cfg.zone_blind_threshold = (uint16_t)atol(str);
									break;
								case 1:
									cfg.zone_blind = false;
									break;
							}
							break;
						case 11:
							uifc.helpbuf=
							"~ Maximum Age of Imported EchoMail ~\r\n\r\n"
							"Maximum age (in days) of EchoMail that may be imported. The age is based\r\n"
							"on the date supplied in the message header and may be incorrect in some\r\n"
							"conditions (e.g. erroneous software or incorrect system date).\r\n"
							"Set this value to `0` to disable this feature (no maximum age imposed)."
							;
							if(cfg.max_echomail_age)
								sprintf(str,"%1.0f", cfg.max_echomail_age / (24.0*60.0*60.0));
							else
								strcpy(str, "None");
							if(uifc.input(WIN_MID|WIN_BOT|WIN_SAV,0,0,"Maximum EchoMail Age (in Days)"
								,str, 5, K_EDIT) >= 0)
								cfg.max_echomail_age = (long) (strtod(str, NULL) * (24.0*60.0*60.0));
							break;

					} 
				}
				break;

			case 3:	/* Archive Types */
				i=0;
				while(1) {
					uifc.helpbuf=
	"~ Archive Types ~\r\n\r\n"
	"These are the archive file types that have been configured along with\r\n"
	"their corresponding archive programs and command-lines for the packing\r\n"
	"and unpacking of EchoMail bundle files.\r\n"
	"\r\n"
	"The corresponding archive programs are sometimes referred to as `packers`."
	;
					for(u=0;u<cfg.arcdefs;u++)
						snprintf(opt[u],MAX_OPLN-1,"%-30.30s",cfg.arcdef[u].name);
					opt[u][0]=0;
					i=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|WIN_GET|WIN_PUT
						|WIN_INSACT|WIN_DELACT|WIN_XTR
						,0,0,0,&i,0,"Archive Types",opt);
					if(i==-1)
						break;
					if((i&MSK_ON)==MSK_INS) {
						i&=MSK_OFF;
						str[0]=0;
	uifc.helpbuf=
	"~ Archive Type ~\r\n\r\n"
	"This is the identifying name of the archiving program (packer).\r\n";
						if(uifc.input(WIN_MID,0,0
							,"Archive Type",str,25,K_EDIT|K_UPPER)<1)
							continue;
						if((cfg.arcdef=(arcdef_t *)realloc(cfg.arcdef
							,sizeof(arcdef_t)*(cfg.arcdefs+1)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); 
						}
						for(j=cfg.arcdefs;j>i;j--)
							memcpy(&cfg.arcdef[j],&cfg.arcdef[j-1]
								,sizeof(arcdef_t));
							strcpy(cfg.arcdef[j].name
								,cfg.arcdef[j-1].name);
						cfg.arcdefs++;
						memset(&cfg.arcdef[i],0,sizeof(arcdef_t));
						strcpy(cfg.arcdef[i].name,str);
						continue; 
					}

					if((i&MSK_ON)==MSK_DEL) {
						i&=MSK_OFF;
						cfg.arcdefs--;
						if(cfg.arcdefs<=0) {
							cfg.arcdefs=0;
							continue; 
						}
						for(u=i;u<cfg.arcdefs;u++)
							memcpy(&cfg.arcdef[u],&cfg.arcdef[u+1]
								,sizeof(arcdef_t));
						if((cfg.arcdef=(arcdef_t *)realloc(cfg.arcdef
							,sizeof(arcdef_t)*(cfg.arcdefs)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); 
						}
						continue; 
					}
					if((i&MSK_ON)==MSK_GET) {
						i&=MSK_OFF;
						memcpy(&savarcdef,&cfg.arcdef[i],sizeof(arcdef_t));
						continue; 
					}
					if((i&MSK_ON)==MSK_PUT) {
						i&=MSK_OFF;
						memcpy(&cfg.arcdef[i],&savarcdef,sizeof(arcdef_t));
						continue; 
					}
					while(1) {
						uifc.helpbuf=
	"Archive Type and Program configuration";
						j=0;
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Archive Type"
							,cfg.arcdef[i].name);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Signature"
							,cfg.arcdef[i].hexid);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %u","Signature Offset"
							,cfg.arcdef[i].byteloc);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Pack Command Line"
							,cfg.arcdef[i].pack);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Unpack Command Line"
							,cfg.arcdef[i].unpack);
						opt[j][0]=0;
						SAFEPRINTF(str,"Archive Type - %s", cfg.arcdef[i].name);
						k=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,0,0,60,&packop,0,str,opt);
						if(k==-1)
							break;
						switch(k) {
							case 0:
								uifc.helpbuf=
								"~ Archive Type ~\r\n\r\n"
								"This is the identifying name of the archive file type. Usually this name\r\n"
								"corresponds with the common file extension or suffix denoting this type\r\n"
								"of archive file (e.g. `zip`, `arc`, etc.)."
								;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Archive Type"
									,cfg.arcdef[i].name,sizeof(cfg.arcdef[i].name)-1
									,K_EDIT|K_UPPER);
								break;
							case 1:
								uifc.helpbuf=
								"~ Archive Signature ~\r\n\r\n"
								"This is the identifying signature of the archive file format (in\r\n"
								"hexadecimal notation).  This signature is used in combination with the\r\n"
								"Archive `Signature Offset` for the automatic detection of proper archive\r\n"
								"program to extract (unarchive) inbound EchoMail bundle files."
								;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Archive Signature"
									,cfg.arcdef[i].hexid,sizeof(cfg.arcdef[i].hexid)-1
									,K_EDIT|K_UPPER);
								break;
							case 2:
								uifc.helpbuf=
								"~ Archive Signature Offset ~\r\n\r\n"
								"This is the byte-offset of the identifying signature of the archive file\r\n"
								"format.  This offset is used in combination with the Archive `Signature`\r\n"
								"for the automatic detection of proper archive program to extract\r\n"
								"(unarchive) inbound EchoMail bundle files."
								;
								sprintf(str,"%u",cfg.arcdef[i].byteloc);
								if(uifc.input(WIN_MID|WIN_SAV,0,0
									,"Archive Signature Offset",str,5
									,K_NUMBER|K_EDIT) > 0)
									cfg.arcdef[i].byteloc=atoi(str);
								break;
							case 3:
								uifc.helpbuf=
								"~ Pack Command Line ~\r\n\r\n"
								"This is the command-line to execute to create an archive file of the\r\n"
								"selected type.  The following command-line specifiers may be used for\r\n"
								"dynamic variable replacement:\r\n"
								"\r\n"
								"  `%f`  The path/filename of the archive file to be created\r\n"
								"  `%s`  The path/filename of the file(s) to be added to the archive\r\n"
								"  `%!`  The Synchronet `exec` directory\r\n"
								"  `%@`  The Synchronet `exec` directory only for non-Unix systems\r\n"
								"  `%.`  Blank for Unix systems, '`.exe`' otherwise\r\n"
								"  `%?`  The current platform description (e.g. 'linux', 'win32')\r\n"
								"  `%j`  The Synchronet `data` directory\r\n"
								"  `%k`  The Synchronet `ctrl` directory\r\n"
								"  `%o`  The configured system operator name\r\n"
								"  `%q`  The configured system QWK-ID\r\n"
								"  `%g`  The configured temporary file directory\r\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Pack Command Line"
									,cfg.arcdef[i].pack,sizeof(cfg.arcdef[i].pack)-1
									,K_EDIT);
								break;
							case 4:
								uifc.helpbuf=
								"~ Unpack Command Line ~\r\n\r\n"
								"This is the command-line to execute to extract an archive file of the\r\n"
								"selected type.  The following command-line specifiers may be used for\r\n"
								"dynamic variable replacement:\r\n"
								"\r\n"
								"  `%f`  The path/filename of the archive file to be extracted\r\n"
								"  `%s`  The path/filename of the file(s) to extracted from the archive\r\n"
								"  `%!`  The Synchronet `exec` directory\r\n"
								"  `%@`  The Synchronet `exec` directory only for non-Unix systems\r\n"
								"  `%.`  Blank for Unix systems, '`.exe`' otherwise\r\n"
								"  `%?`  The current platform description (e.g. 'linux', 'win32')\r\n"
								"  `%j`  The Synchronet `data` directory\r\n"
								"  `%k`  The Synchronet `ctrl` directory\r\n"
								"  `%o`  The configured system operator name\r\n"
								"  `%q`  The configured system QWK-ID\r\n"
								"  `%g`  The configured temporary file directory\r\n"
								;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Unpack Command Line"
									,cfg.arcdef[i].unpack,sizeof(cfg.arcdef[i].unpack)-1
									,K_EDIT);
								break;
						} 
					} 
				}
				break;

			case 7:
	uifc.helpbuf=
	"~ Additional EchoLists ~\r\n\r\n"
	"This feature allows you to specify lists of echoes, in `BACKBONE.NA` format\r\n"
	"which are utilized in addition to your Area File (e.g. `areas.bbs`)\r\n"
	"for advanced AreaFix (Area Management) operations.\r\n";
				i=0;
				while(1) {
					for(u=0;u<cfg.listcfgs;u++)
						snprintf(opt[u],MAX_OPLN-1,"%s",cfg.listcfg[u].listpath);
					opt[u][0]=0;
					i=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|WIN_GET|WIN_PUT
						|WIN_INSACT|WIN_DELACT|WIN_XTR
						,0,0,0,&i,0,"Additional EchoLists",opt);
					if(i==-1)
						break;
					if((i&MSK_ON)==MSK_INS) {
						i&=MSK_OFF;
						str[0]=0;
	uifc.helpbuf=
	"~ EchoList ~\r\n\r\n"
	"This is the path and filename of the echolist file you wish\r\n"
	"to add.\r\n";
						if(uifc.input(WIN_MID|WIN_SAV,0,0
							,"EchoList Path/Name",str,50,K_EDIT)<1)
							continue;
						if((cfg.listcfg=(echolist_t *)realloc(cfg.listcfg
							,sizeof(echolist_t)*(cfg.listcfgs+1)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); 
						}
						for(j=cfg.listcfgs;j>i;j--)
							memcpy(&cfg.listcfg[j],&cfg.listcfg[j-1]
								,sizeof(echolist_t));
						cfg.listcfgs++;
						memset(&cfg.listcfg[i],0,sizeof(echolist_t));
						strcpy(cfg.listcfg[i].listpath,str);
						continue; 
					}

					if((i&MSK_ON)==MSK_DEL) {
						i&=MSK_OFF;
						cfg.listcfgs--;
						if(cfg.listcfgs<=0) {
							cfg.listcfgs=0;
							continue; 
						}
						for(u=i;u<cfg.listcfgs;u++)
							memcpy(&cfg.listcfg[u],&cfg.listcfg[u+1]
								,sizeof(echolist_t));
						if((cfg.listcfg=(echolist_t *)realloc(cfg.listcfg
							,sizeof(echolist_t)*(cfg.listcfgs)))==NULL) {
							printf("\nMemory Allocation Error\n");
							exit(1); 
						}
						continue; 
					}
					if((i&MSK_ON)==MSK_GET) {
						i&=MSK_OFF;
						memcpy(&savlistcfg,&cfg.listcfg[i],sizeof(echolist_t));
						continue; 
					}
					if((i&MSK_ON)==MSK_PUT) {
						i&=MSK_OFF;
						memcpy(&cfg.listcfg[i],&savlistcfg,sizeof(echolist_t));
						continue; 
					}
					while(1) {
						j=0;
						uifc.helpbuf=
						"Configuring an Additional EchoList"
						;
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","EchoList Path/Name"
							,cfg.listcfg[i].listpath);
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Hub Address"
							,(cfg.listcfg[i].hub.zone) ?
							 faddrtoa(&cfg.listcfg[i].hub) : "None");
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Forward Password"
							,(cfg.listcfg[i].password[0]) ?
							 cfg.listcfg[i].password : "None");
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Forward Requests"
							,cfg.listcfg[i].forward ? "Yes" : "No");
						snprintf(opt[j++],MAX_OPLN-1,"%-30.30s %s","Areafix keys"
							,strListCombine(cfg.listcfg[i].keys, str, sizeof(str), ","));
						opt[j][0]=0;
						SAFEPRINTF(str, "Additional EchoList - %s", getfname(cfg.listcfg[i].listpath));
						k=uifc.list(WIN_ACT|WIN_SAV|WIN_RHT|WIN_BOT,0,0,60,&listop,0,str,opt);
						if(k==-1)
							break;
						switch(k) {
							case 0:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"EchoList Path/Name"
									,cfg.listcfg[i].listpath,sizeof(cfg.listcfg[i].listpath)-1
									,K_EDIT);
								break;
							case 1:
								if(cfg.listcfg[i].hub.zone)
									strcpy(str,faddrtoa(&cfg.listcfg[i].hub));
								else
									str[0]=0;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Hub Address",str
									,25,K_EDIT);
								if(str[0])
									cfg.listcfg[i].hub=atofaddr(str);
								else
									memset(&cfg.listcfg[i].hub,0
										,sizeof(faddr_t));
								break;
							case 2:
								uifc.input(WIN_MID|WIN_SAV,0,0
									,"Password to use when forwarding requests"
									,cfg.listcfg[i].password,sizeof(cfg.listcfg[i].password)-1
									,K_EDIT|K_UPPER);
								break;
							case 3:
								cfg.listcfg[i].forward = !cfg.listcfg[i].forward;
								break;
							case 4:
								while(1) {
									for(u=0; cfg.listcfg[i].keys!=NULL && cfg.listcfg[i].keys[u] != NULL;u++)
										strcpy(opt[u],cfg.listcfg[i].keys[u]);
									opt[u][0]=0;
									x=uifc.list(WIN_SAV|WIN_INS|WIN_DEL|WIN_ACT|
										WIN_XTR|WIN_INSACT|WIN_DELACT|WIN_RHT
										,0,0,0,&x,0,"EchoList keys",opt);
									if(x==-1)
										break;
									if((x&MSK_ON)==MSK_INS) {
										x&=MSK_OFF;
										str[0]=0;
	uifc.helpbuf=
	"~ EchoList Keys ~\r\n\r\n"
	"These keys determine which nodes have access to the current\r\n"
	"echolist file via AreaFix requests.\r\n";
										if(uifc.input(WIN_MID|WIN_SAV,0,0
											,"EchoList Key",str,SBBSECHO_MAX_KEY_LEN
											,K_EDIT|K_UPPER)<1)
											continue;
										strListInsert(&cfg.listcfg[i].keys,str,x);
										continue; 
									}

									if((x&MSK_ON)==MSK_DEL) {
										x&=MSK_OFF;
										strListRemove(&cfg.listcfg[i].keys,x);
										continue; 
									}
									SAFECOPY(str,cfg.listcfg[i].keys[x]);
	uifc.helpbuf=
	"~ EchoList Keys ~\r\n\r\n"
	"These keys determine which nodes have access to the current\r\n"
	"echolist file via AreaFix requests.\r\n";
										uifc.input(WIN_MID|WIN_SAV,0,0,"EchoList Key"
											,str,SBBSECHO_MAX_KEY_LEN,K_EDIT|K_UPPER);
										strListReplace(cfg.listcfg[i].keys,x,str);
										continue; 
								}
								break;
						} 
					} 
				}
				break;

			case 8:
				if(!sbbsecho_write_ini(&cfg))
					uifc.msg("Error saving configuration file");
				else {
					orig_cfg = cfg;
					uifc.changes = FALSE;
				}
				break;
			case -1:
				if(uifc.changes) {
		uifc.helpbuf=
		"~ Save Configuration File ~\r\n\r\n"
		"Select `Yes` to save the config file, `No` to quit without saving,\r\n"
		"or hit ~ ESC ~ to go back to the menu.\r\n\r\n";
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					i=uifc.list(WIN_MID,0,0,0,&i,0,"Save Config File",opt);
					if(i==-1) break;
					if(i) {uifc.bail(); exit(0);}
					if(!sbbsecho_write_ini(&cfg))
						uifc.msg("Error saving configuration file");
				}
				uifc.bail();
				exit(0);
		}
	}
}
