/* scfgnet.c */

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

void qhub_edit(int num);
void phub_edit(int num);
char *daystr(char days);
void qhub_sub_edit(uint num);
/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(char *str)
{
    char *p;
    faddr_t addr;

addr.zone=addr.net=addr.node=addr.point=0;
if((p=strchr(str,':'))!=NULL) {
    addr.zone=atoi(str);
    addr.net=atoi(p+1); }
else {
    if(cfg.total_faddrs)
		addr.zone=cfg.faddr[0].zone;
    else
        addr.zone=1;
    addr.net=atoi(str); }
if(!addr.zone)              /* no such thing as zone 0 */
    addr.zone=1;
if((p=strchr(str,'/'))!=NULL)
    addr.node=atoi(p+1);
else {
    if(cfg.total_faddrs)
		addr.net=cfg.faddr[0].net;
    else
        addr.net=1;
    addr.node=atoi(str); }
if((p=strchr(str,'.'))!=NULL)
    addr.point=atoi(p+1);
return(addr);
}

uint getsub()
{
	static int grp_dflt,sub_dflt,grp_bar,sub_bar;
	char str[81];
	int i,j,k;
	uint subnum[MAX_OPTS+1];

while(1) {
	for(i=0;i<cfg.total_grps && i<MAX_OPTS;i++)
		sprintf(opt[i],"%-25s",cfg.grp[i]->lname);
	opt[i][0]=0;
	i=uifc.list(WIN_SAV|WIN_RHT|WIN_BOT,0,0,45,&grp_dflt,&grp_bar
		,"Message Groups"
		,opt);
	if(i==-1)
		return(-1);
	for(j=k=0;j<cfg.total_subs && k<MAX_OPTS;j++)
		if(cfg.sub[j]->grp==i) {
			sprintf(opt[k],"%-25s",cfg.sub[j]->lname);
			subnum[k++]=j; }
	opt[k][0]=0;
	sprintf(str,"%s Sub-boards",cfg.grp[i]->sname);
	j=uifc.list(WIN_RHT|WIN_BOT|WIN_SAV,0,0,45,&sub_dflt,&sub_bar,str,opt);
	if(j==-1)
		continue;
	return(subnum[j]); }

}

void net_cfg()
{
	static	int net_dflt,qnet_dflt,pnet_dflt,fnet_dflt,inet_dflt
			,qhub_dflt,phub_dflt;
	char	str[81],done;
	int 	i,j,k,l;

while(1) {
	i=0;
	strcpy(opt[i++],"Internet E-mail");
	strcpy(opt[i++],"QWK Packet Networks");
	strcpy(opt[i++],"FidoNet EchoMail and NetMail");
	strcpy(opt[i++],"PostLink Networks");
	opt[i][0]=0;
	SETHELP(WHERE);
/*
Configure Networks:

This is the network configuration menu. Select the type of network
technology that you want to configure.
*/
	i=uifc.list(WIN_ORG|WIN_ACT|WIN_CHE,0,0,0,&net_dflt,0,"Networks",opt);
	if(i==1) {	/* QWK net stuff */
		done=0;
		while(!done) {
			i=0;
			strcpy(opt[i++],"Network Hubs...");
			strcpy(opt[i++],"Default Tagline");
			opt[i][0]=0;
			SETHELP(WHERE);
/*
QWK Packet Networks:

From this menu you can configure the default tagline to use for
outgoing messages on QWK networked sub-boards, or you can select
Network Hubs... to add, delete, or configure QWK hubs that your system
calls to exchange packets with.
*/
			i=uifc.list(WIN_ACT|WIN_RHT|WIN_BOT|WIN_CHE,0,0,0,&qnet_dflt,0
				,"QWK Packet Networks",opt);
			uifc.savnum=0;
			switch(i) {
				case -1:	/* ESC */
					done=1;
					break;
				case 1:
					SETHELP(WHERE);
/*
QWK Network Default Tagline:

This is the default tagline to use for outgoing messages on QWK
networked sub-boards. This default can be overridden on a per sub-board
basis with the sub-board configuration Network Options....
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,nulstr
						,cfg.qnet_tagline,70,K_MSG|K_EDIT);
					break;
				case 0:
					while(1) {
						for(i=0;i<cfg.total_qhubs && i<MAX_OPTS;i++)
							sprintf(opt[i],"%-8.8s",cfg.qhub[i]->id);
						opt[i][0]=0;
						i=WIN_ACT|WIN_RHT|WIN_SAV;
						if(cfg.total_qhubs<MAX_OPTS)
							i|=WIN_INS|WIN_INSACT|WIN_XTR;
						if(cfg.total_qhubs)
							i|=WIN_DEL;
						uifc.savnum=0;
						SETHELP(WHERE);
/*
QWK Network Hubs:

This is a list of QWK network hubs that your system calls to exchange
packets with.

To add a hub, select the desired location with the arrow keys and hit
 INS .

To delete a hub, select it and hit  DEL .

To configure a hub, select it and hit  ENTER .
*/
						i=uifc.list(i,0,0,0,&qhub_dflt,0
							,"QWK Network Hubs",opt);
						if(i==-1)
							break;
						if((i&MSK_ON)==MSK_INS) {
							i&=MSK_OFF;
							if((cfg.qhub=(qhub_t **)REALLOC(cfg.qhub
                                ,sizeof(qhub_t *)*(cfg.total_qhubs+1)))==NULL) {
                                errormsg(WHERE,ERR_ALLOC,nulstr
                                    ,sizeof(qhub_t *)*(cfg.total_qhubs+1));
								cfg.total_qhubs=0;
								bail(1);
                                continue; }

							uifc.savnum=1;
							SETHELP(WHERE);
/*
QWK Network Hub System ID:

This is the QWK System ID of this hub. It is used for incoming and
outgoing network packets and must be accurate.
*/
							if(uifc.input(WIN_MID|WIN_SAV,0,0
								,"System ID",str,8,K_UPPER)<1)
								continue;

							for(j=cfg.total_qhubs;j>i;j--)
                                cfg.qhub[j]=cfg.qhub[j-1];
							if((cfg.qhub[i]=(qhub_t *)MALLOC(sizeof(qhub_t)))
								==NULL) {
								errormsg(WHERE,ERR_ALLOC,nulstr
									,sizeof(qhub_t));
								continue; }
							memset(cfg.qhub[i],0,sizeof(qhub_t));
							strcpy(cfg.qhub[i]->id,str);
							strcpy(cfg.qhub[i]->pack,"%!pkzip %f %s");
							strcpy(cfg.qhub[i]->unpack,"%!pkunzip -o %f %g %s");
							strcpy(cfg.qhub[i]->call,"*qnet-ftp VERT vert.synchro.net YOURPASS");
							cfg.qhub[i]->node=1;
							cfg.qhub[i]->days=0xff; /* all days */
							cfg.total_qhubs++;
							uifc.changes=1;
							continue; }
						if((i&MSK_ON)==MSK_DEL) {
							i&=MSK_OFF;
							FREE(cfg.qhub[i]->mode);
							FREE(cfg.qhub[i]->conf);
							FREE(cfg.qhub[i]->sub);
							FREE(cfg.qhub[i]);
							cfg.total_qhubs--;
							while(i<cfg.total_qhubs) {
								cfg.qhub[i]=cfg.qhub[i+1];
								i++; }
							uifc.changes=1;
							continue; }
						qhub_edit(i); }
					break; } } }

	else if(i==2) { 	/* FidoNet Stuff */
		done=0;
		while(!done) {
			i=0;
			sprintf(opt[i++],"%-27.27s%s"
				,"System Addresses",cfg.total_faddrs
                	? faddrtoa(&cfg.faddr[0],tmp) : nulstr);
			sprintf(opt[i++],"%-27.27s%s"
				,"Default Outbound Address"
				,cfg.dflt_faddr.zone
                	? faddrtoa(&cfg.dflt_faddr,tmp) : "No");
			sprintf(opt[i++],"%-27.27s"
				,"Default Origin Line");
			sprintf(opt[i++],"%-27.27s%.40s"
				,"NetMail Semaphore",cfg.netmail_sem);
			sprintf(opt[i++],"%-27.27s%.40s"
				,"EchoMail Semaphore",cfg.echomail_sem);
			sprintf(opt[i++],"%-27.27s%.40s"
				,"Inbound File Directory",cfg.fidofile_dir);
			sprintf(opt[i++],"%-27.27s%.40s"
				,"EchoMail Base Directory",cfg.echomail_dir);
			sprintf(opt[i++],"%-27.27s%.40s"
				,"NetMail Directory",cfg.netmail_dir);
			sprintf(opt[i++],"%-27.27s%s"
				,"Allow Sending of NetMail"
				,cfg.netmail_misc&NMAIL_ALLOW ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"Allow File Attachments"
				,cfg.netmail_misc&NMAIL_FILE ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"Send NetMail Using Alias"
				,cfg.netmail_misc&NMAIL_ALIAS ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"NetMail Defaults to Crash"
				,cfg.netmail_misc&NMAIL_CRASH ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"NetMail Defaults to Direct"
				,cfg.netmail_misc&NMAIL_DIRECT ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"NetMail Defaults to Hold"
				,cfg.netmail_misc&NMAIL_HOLD ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"Kill NetMail After Sent"
				,cfg.netmail_misc&NMAIL_KILL ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%lu"
				,"Cost to Send NetMail",cfg.netmail_cost);
			opt[i][0]=0;
			SETHELP(WHERE);
/*
FidoNet EchoMail and NetMail:

This menu contains configuration options that pertain specifically to
networking E-mail (NetMail) and sub-boards (EchoMail) through networks
using FidoNet technology.
*/
			i=uifc.list(WIN_ACT|WIN_MID|WIN_CHE,0,0,60,&fnet_dflt,0
				,"FidoNet EchoMail and NetMail",opt);
			uifc.savnum=0;
			switch(i) {
				case -1:	/* ESC */
					done=1;
					break;
				case 0:
					SETHELP(WHERE);
/*
System FidoNet Addresses:

This is the FidoNet address of this system used to receive NetMail.
The Main address is also used as the default address for sub-boards.
Format: Zone:Net/Node[.Point]
*/
					k=l=0;
					while(1) {
						for(i=0;i<cfg.total_faddrs && i<MAX_OPTS;i++) {
							if(i==0)
								strcpy(str,"Main");
							else
								sprintf(str,"AKA %u",i);
							sprintf(opt[i],"%-8.8s %-16s"
								,str,faddrtoa(&cfg.faddr[i],tmp)); }
						opt[i][0]=0;
						j=WIN_RHT|WIN_SAV|WIN_ACT|WIN_INSACT;
						if(cfg.total_faddrs<MAX_OPTS)
							j|=WIN_INS|WIN_XTR;
						if(cfg.total_faddrs)
							j|=WIN_DEL;
						i=uifc.list(j,0,0,0,&k,&l
							,"System Addresses",opt);
						if(i==-1)
							break;
						if((i&MSK_ON)==MSK_INS) {
							i&=MSK_OFF;

							if(!cfg.total_faddrs)
								strcpy(str,"1:1/0");
							else
								faddrtoa(&cfg.faddr[0],str);
							if(!uifc.input(WIN_MID|WIN_SAV,0,0,"Address"
								,str,25,K_EDIT|K_UPPER))
								continue;

							if((cfg.faddr=(faddr_t *)REALLOC(cfg.faddr
                                ,sizeof(faddr_t)*(cfg.total_faddrs+1)))==NULL) {
                                errormsg(WHERE,ERR_ALLOC,nulstr
                                    ,sizeof(faddr_t)*cfg.total_faddrs+1);
								cfg.total_faddrs=0;
								bail(1);
                                continue; }

							for(j=cfg.total_faddrs;j>i;j--)
                                cfg.faddr[j]=cfg.faddr[j-1];

							cfg.faddr[i]=atofaddr(str);
							cfg.total_faddrs++;
							uifc.changes=1;
							continue; }
						if((i&MSK_ON)==MSK_DEL) {
							i&=MSK_OFF;
							cfg.total_faddrs--;
							while(i<cfg.total_faddrs) {
								cfg.faddr[i]=cfg.faddr[i+1];
								i++; }
							uifc.changes=1;
							continue; }
						faddrtoa(&cfg.faddr[i],str);
						uifc.input(WIN_MID|WIN_SAV,0,0,"Address"
							,str,25,K_EDIT);
						cfg.faddr[i]=atofaddr(str); }
                    break;
				case 1:
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Use Default Outbound NetMail Address:

If you would like to have a default FidoNet address adding to outbound
NetMail mail messages that do not have an address specified, select
Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Use Default Outbound NetMail Address",opt);
					if(i==1) {
						if(cfg.dflt_faddr.zone)
							uifc.changes=1;
						cfg.dflt_faddr.zone=0;
						break; }
					if(i==-1)
						break;
					if(!cfg.dflt_faddr.zone) {
						cfg.dflt_faddr.zone=1;
						uifc.changes=1; }
					faddrtoa(&cfg.dflt_faddr,str);
					SETHELP(WHERE);
/*
Default Outbound FidoNet NetMail Address:

If you would like to automatically add a FidoNet address to outbound
NetMail that does not have an address specified, set this option
to that address. This is useful for Fido/UUCP gateway mail.
Format: Zone:Net/Node[.Point]
*/
					if(uifc.input(WIN_MID|WIN_SAV,0,0,"Outbound Address"
						,str,25,K_EDIT)) {
						cfg.dflt_faddr=atofaddr(str);
						uifc.changes=1; }
                    break;
				case 2:
					SETHELP(WHERE);
/*
Default Origin Line:

This is the default origin line used for sub-boards networked via
EchoMail. This origin line can be overridden on a per sub-board basis
with the sub-board configuration Network Options....
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"* Origin"
						,cfg.origline,50,K_EDIT);
                    break;
				case 3:
					SETHELP(WHERE);
/*
NetMail Semaphore File:

This is a filename that will be used as a semaphore (signal) to your
FidoNet front-end that new NetMail has been created and the messages
should be re-scanned.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"NetMail Semaphore"
						,cfg.netmail_sem,50,K_EDIT);
                    break;
				case 4:
					SETHELP(WHERE);
/*
EchoMail Semaphore File:

This is a filename that will be used as a semaphore (signal) to your
FidoNet front-end that new EchoMail has been created and the messages
should be re-scanned.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"EchoMail Semaphore"
						,cfg.echomail_sem,50,K_EDIT);
                    break;
				case 5:
					SETHELP(WHERE);
/*
Inbound File Directory:

This directory is where inbound files are placed. This directory is
only used when an incoming message has a file attached.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"Inbound Files"
						,cfg.fidofile_dir,50,K_EDIT);
                    break;
				case 6:
					SETHELP(WHERE);
/*
EchoMail Base Directory:

This is an optional field used as a base directory for the location
of EchoMail messages for sub-boards that do not have a specified
EchoMail Storage Directory. If a sub-board does not have a specified
storage directory for EchoMail, its messages will be imported from and
exported to a sub-directory off of this base directory. The name of the
sub-directory is the same as the internal code for the sub-directory.

If all EchoMail sub-boards have specified EchoMail storage directories,
this option is not used at all.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"EchoMail Base"
						,cfg.echomail_dir,50,K_EDIT);
                    break;
				case 7:
					SETHELP(WHERE);
/*
NetMail Directory:

This is the directory where NetMail will be imported from and exported
to.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"NetMail"
						,cfg.netmail_dir,50,K_EDIT);
                    break;
				case 8:
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Allow Users to Send NetMail:

If you are on a FidoNet style network and want your users to be allowed
to send NetMail, set this option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Allow Users to Send NetMail",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_ALLOW)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_ALLOW; }
					else if(i==1 && cfg.netmail_misc&NMAIL_ALLOW) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_ALLOW; }
					break;
				case 9:
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Allow Users to Send NetMail File Attachments:

If you are on a FidoNet style network and want your users to be allowed
to send NetMail file attachments, set this option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Allow Users to Send NetMail File Attachments",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_FILE)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_FILE; }
					else if(i==1 && cfg.netmail_misc&NMAIL_FILE) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_FILE; }
                    break;
				case 10:
					i=1;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Use Aliases in NetMail:

If you allow aliases on your system and wish users to have their NetMail
contain their alias as the From User, set this option to Yes. If you
want all NetMail to be sent using users' real names, set this option to
No.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Use Aliases in NetMail",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_ALIAS)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_ALIAS; }
					else if(i==1 && cfg.netmail_misc&NMAIL_ALIAS) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_ALIAS; }
                    break;
				case 11:
					i=1;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
NetMail Defaults to Crash Status:

If you want all NetMail to default to crash (send immediately) status,
set this option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"NetMail Defaults to Crash Status",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_CRASH)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_CRASH; }
					else if(i==1 && cfg.netmail_misc&NMAIL_CRASH) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_CRASH; }
                    break;
				case 12:
					i=1;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
NetMail Defaults to Direct Status:

If you want all NetMail to default to direct (send directly) status,
set this option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"NetMail Defaults to Direct Status",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_DIRECT)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_DIRECT; }
					else if(i==1 && cfg.netmail_misc&NMAIL_DIRECT) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_DIRECT; }
                    break;
				case 13:
					i=1;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
NetMail Defaults to Hold Status:

If you want all NetMail to default to hold status, set this option to
Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"NetMail Defaults to Hold Status",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_HOLD)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_HOLD; }
					else if(i==1 && cfg.netmail_misc&NMAIL_HOLD) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_HOLD; }
                    break;
				case 14:
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Kill NetMail After it is Sent:

If you want NetMail messages to be deleted after they are successfully
sent, set this option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Kill NetMail After it is Sent",opt);
					if(!i && !(cfg.netmail_misc&NMAIL_KILL)) {
						uifc.changes=1;
						cfg.netmail_misc|=NMAIL_KILL; }
					else if(i==1 && cfg.netmail_misc&NMAIL_KILL) {
						uifc.changes=1;
						cfg.netmail_misc&=~NMAIL_KILL; }
                    break;
				case 15:
					ultoa(cfg.netmail_cost,str,10);
					SETHELP(WHERE);
/*
Cost in Credits to Send NetMail:

This is the number of credits it will cost your users to send NetMail.
If you want the sending of NetMail to be free, set this value to 0.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0
						,"Cost in Credits to Send NetMail"
						,str,10,K_EDIT|K_NUMBER);
					cfg.netmail_cost=atol(str);
					break; } } }
	else if(i==3) {
		done=0;
		while(!done) {
			i=0;
			strcpy(opt[i++],"Network Hubs...");
			sprintf(opt[i++],"%-20.20s%-12s","Site Name",cfg.sys_psname);
			sprintf(opt[i++],"%-20.20s%-lu","Site Number",cfg.sys_psnum);
			opt[i][0]=0;
			SETHELP(WHERE);
/*
PostLink Networks:

From this menu you can configure PostLink or PCRelay Networks.
*/
			i=uifc.list(WIN_ACT|WIN_RHT|WIN_BOT|WIN_CHE,0,0,0,&pnet_dflt,0
				,"PostLink Networks",opt);
			uifc.savnum=0;
			switch(i) {
				case -1:	/* ESC */
					done=1;
					break;
				case 1:
					SETHELP(WHERE);
/*
PostLink Site Name:

If your system is networked via PostLink or PCRelay, this should be the
Site Name for your BBS.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"Site Name"
						,cfg.sys_psname,12,K_UPPER|K_EDIT);
					break;
				case 2:
					SETHELP(WHERE);
/*
PostLink Site Number:

If your system is networked via PostLink or PCRelay, this should be the
Site Number for your BBS.
*/
					ultoa(cfg.sys_psnum,str,10);
					uifc.input(WIN_MID|WIN_SAV,0,0,"Site Number"
						,str,10,K_NUMBER|K_EDIT);
					cfg.sys_psnum=atol(str);
                    break;
				case 0:
					while(1) {
						for(i=0;i<cfg.total_phubs && i<MAX_OPTS;i++)
							sprintf(opt[i],"%-10.10s",cfg.phub[i]->name);
						opt[i][0]=0;
						i=WIN_ACT|WIN_RHT|WIN_SAV;
						if(cfg.total_phubs<MAX_OPTS)
							i|=WIN_INS|WIN_INSACT|WIN_XTR;
						if(cfg.total_phubs)
							i|=WIN_DEL;
						uifc.savnum=0;
						SETHELP(WHERE);
/*
PostLink Network Hubs:

This is a list of PostLink and/or PCRelay network hubs that your system
calls to exchange packets with.

To add a hub, select the desired location with the arrow keys and hit
 INS .

To delete a hub, select it and hit  DEL .

To configure a hub, select it and hit  ENTER .
*/
						i=uifc.list(i,0,0,0,&phub_dflt,0
							,"PostLink Hubs",opt);
						if(i==-1)
							break;
						if((i&MSK_ON)==MSK_INS) {
							i&=MSK_OFF;
							if((cfg.phub=(phub_t **)REALLOC(cfg.phub
                                ,sizeof(phub_t *)*(cfg.total_phubs+1)))==NULL) {
                                errormsg(WHERE,ERR_ALLOC,nulstr
                                    ,sizeof(phub_t *)*(cfg.total_phubs+1));
								cfg.total_phubs=0;
								bail(1);
                                continue; }

							uifc.savnum=1;
							SETHELP(WHERE);
/*
Network Hub Site Name:

This is the Site Name of this hub. It is used for only for reference.
*/
							if(uifc.input(WIN_MID|WIN_SAV,0,0
								,"Site Name",str,10,K_UPPER)<1)
								continue;

							for(j=cfg.total_phubs;j>i;j--)
                                cfg.phub[j]=cfg.phub[j-1];

							if((cfg.phub[i]=(phub_t *)MALLOC(sizeof(phub_t)))
								==NULL) {
								errormsg(WHERE,ERR_ALLOC,nulstr
									,sizeof(phub_t));
								continue; }
							memset(cfg.phub[i],0,sizeof(phub_t));
							strcpy(cfg.phub[i]->name,str);
							strcpy(cfg.phub[i]->call,"%!pnet");
							cfg.phub[i]->node=1;
							cfg.phub[i]->days=0xff; /* all days */
							cfg.total_phubs++;
							uifc.changes=1;
							continue; }
						if((i&MSK_ON)==MSK_DEL) {
							i&=MSK_OFF;
							FREE(cfg.phub[i]);
							cfg.total_phubs--;
							while(i<cfg.total_phubs) {
								cfg.phub[i]=cfg.phub[i+1];
								i++; }
							uifc.changes=1;
							continue; }
						phub_edit(i); }
                    break; } } }

	else if(i==0) { 	/* Internet E-mail */
		done=0;
		while(!done) {
			i=0;
			sprintf(opt[i++],"%-27.27s%s"
				,"System Address",cfg.sys_inetaddr);
			sprintf(opt[i++],"%-27.27s%.40s"
				,"Inbound E-mail Semaphore",cfg.smtpmail_sem);
			sprintf(opt[i++],"%-27.27s%.40s"
				,"Outbound E-mail Semaphore",cfg.inetmail_sem);
			sprintf(opt[i++],"%-27.27s%s"
				,"Allow Sending of E-mail"
				,cfg.inetmail_misc&NMAIL_ALLOW ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"Allow File Attachments"
				,cfg.inetmail_misc&NMAIL_FILE ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%s"
				,"Send E-mail Using Alias"
				,cfg.inetmail_misc&NMAIL_ALIAS ? "Yes":"No");
			sprintf(opt[i++],"%-27.27s%lu"
				,"Cost to Send E-mail",cfg.inetmail_cost);
			opt[i][0]=0;
			SETHELP(WHERE);
/*
Internet E-mail:

This menu contains configuration options that pertain specifically to
Internet E-mail.
*/
			i=uifc.list(WIN_ACT|WIN_MID|WIN_CHE,0,0,60,&inet_dflt,0
				,"Internet E-mail",opt);
			uifc.savnum=0;
			switch(i) {
				case -1:	/* ESC */
					done=1;
					break;
				case 0:
					SETHELP(WHERE);
/*
Sytem Internet Address:

Enter your system's Internet address (hostname or IP address) here
(e.g. joesbbs.com).
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,""
						,cfg.sys_inetaddr,60,K_EDIT);
                    break;
				case 1:
					SETHELP(WHERE);
/*
Inbound Internet E-mail Semaphore File:

This is a filename that will be used as a semaphore (signal) to any
external Internet e-mail processors that new mail has been received
and the message base should be re-scanned.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"Inbound Semaphore"
						,cfg.smtpmail_sem,50,K_EDIT);
                    break;
				case 2:
					SETHELP(WHERE);
/*
Outbound Internet E-mail Semaphore File:

This is a filename that will be used as a semaphore (signal) to any
external Internet gateways (if supported) that new mail has been created
and the message base should be re-scanned.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0,"Outbound Semaphore"
						,cfg.inetmail_sem,50,K_EDIT);
                    break;
				case 3:
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Allow Users to Send Internet E-mail:

If you want your users to be allowed to send Internet E-mail, set this
option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Allow Users to Send E-mail",opt);
					if(!i && !(cfg.inetmail_misc&NMAIL_ALLOW)) {
						uifc.changes=1;
						cfg.inetmail_misc|=NMAIL_ALLOW; }
					else if(i==1 && cfg.inetmail_misc&NMAIL_ALLOW) {
						uifc.changes=1;
						cfg.inetmail_misc&=~NMAIL_ALLOW; }
					break;
				case 4:
					i=0;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Allow Users to Send Internet E-mail File Attachments:

If you want your users to be allowed to send Internet E-mail with file
attachments, set this option to Yes.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Allow Users to Send E-mail with File Attachments",opt);
					if(!i && !(cfg.inetmail_misc&NMAIL_FILE)) {
						uifc.changes=1;
						cfg.inetmail_misc|=NMAIL_FILE; }
					else if(i==1 && cfg.inetmail_misc&NMAIL_FILE) {
						uifc.changes=1;
						cfg.inetmail_misc&=~NMAIL_FILE; }
                    break;
				case 5:
					i=1;
					strcpy(opt[0],"Yes");
					strcpy(opt[1],"No");
					opt[2][0]=0;
					SETHELP(WHERE);
/*
Use Aliases in Internet E-mail:

If you allow aliases on your system and wish users to have their
Internet E-mail contain their alias as the From User, set this option to
Yes. If you want all E-mail to be sent using users' real names, set this
option to No.
*/
					i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
						,"Use Aliases in Internet E-mail",opt);
					if(!i && !(cfg.inetmail_misc&NMAIL_ALIAS)) {
						uifc.changes=1;
						cfg.inetmail_misc|=NMAIL_ALIAS; }
					else if(i==1 && cfg.inetmail_misc&NMAIL_ALIAS) {
						uifc.changes=1;
						cfg.inetmail_misc&=~NMAIL_ALIAS; }
					break;
				case 6:
					ultoa(cfg.inetmail_cost,str,10);
					SETHELP(WHERE);
/*
Cost in Credits to Send Internet E-mail:

This is the number of credits it will cost your users to send Internet
E-mail. If you want the sending of Internet E-mail to be free, set this
value to 0.
*/
					uifc.input(WIN_MID|WIN_SAV,0,0
						,"Cost in Credits to Send Internet E-mail"
						,str,10,K_EDIT|K_NUMBER);
					cfg.inetmail_cost=atol(str);
					break; } } }

	else { /* ESC */
		i=save_changes(WIN_MID|WIN_SAV);
		if(i==-1)
			continue;
		if(!i) {
			write_msgs_cfg(&cfg,backup_level);
            rerun_nodes();
        }
		break;
        }
    }
}

void qhub_edit(int num)
{
	static int qhub_dflt;
	char *p,done=0,str[256];
	int i,j;

while(!done) {
	i=0;
	sprintf(opt[i++],"%-27.27s%s","Hub System ID",cfg.qhub[num]->id);
	sprintf(opt[i++],"%-27.27s%.40s","Pack Command Line",cfg.qhub[num]->pack);
	sprintf(opt[i++],"%-27.27s%.40s","Unpack Command Line",cfg.qhub[num]->unpack);
	sprintf(opt[i++],"%-27.27s%.40s","Call-out Command Line",cfg.qhub[num]->call);
	sprintf(opt[i++],"%-27.27s%u","Call-out Node",cfg.qhub[num]->node);
	sprintf(opt[i++],"%-27.27s%s","Call-out Days",daystr(cfg.qhub[num]->days));
	if(cfg.qhub[num]->freq) {
		sprintf(str,"%u times a day",1440/cfg.qhub[num]->freq);
		sprintf(opt[i++],"%-27.27s%s","Call-out Frequency",str); }
	else {
		sprintf(str,"%2.2u:%2.2u",cfg.qhub[num]->time/60,cfg.qhub[num]->time%60);
		sprintf(opt[i++],"%-27.27s%s","Call-out Time",str); }
	strcpy(opt[i++],"Networked Sub-boards...");
	opt[i][0]=0;
	sprintf(str,"%s Network Hub",cfg.qhub[num]->id);
	uifc.savnum=1;
	SETHELP(WHERE);
/*
QWK Network Hub Configuration:

This menu allows you to configure options specific to this QWK network
hub.
*/
	switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,0,&qhub_dflt,0
		,str,opt)) {
		case -1:
			done=1;
			break;
		case 0:
			SETHELP(WHERE);
/*
QWK Network Hub System ID:

This is the QWK System ID of this hub. It is used for incoming and
outgoing network packets and must be accurate.
*/
			strcpy(str,cfg.qhub[num]->id);	/* save */
			if(!uifc.input(WIN_MID|WIN_SAV,0,0,"QWK Network Hub System ID"
				,cfg.qhub[num]->id,8,K_UPPER|K_EDIT))
				strcpy(cfg.qhub[num]->id,str);
			break;
		case 1:
			SETHELP(WHERE);
/*
REP Packet Creation Command:

This is the command line to use to create (compress) REP packets for
this QWK network hub.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Packet Creation"
				,cfg.qhub[num]->pack,50,K_EDIT);
			break;
		case 2:
			SETHELP(WHERE);
/*
QWK Packet Extraction Command:

This is the command line to use to extract (decompress) QWK packets from
this QWK network hub.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Packet Extraction"
				,cfg.qhub[num]->unpack,50,K_EDIT);
			break;
		case 3:
			SETHELP(WHERE);
/*
QWK Network Hub Call-out Command Line:

This is the command line to use to initiate a call-out to this QWK
network hub.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Call-out Command"
				,cfg.qhub[num]->call,50,K_EDIT);
			break;
		case 4:
			sprintf(str,"%u",cfg.qhub[num]->node);
			SETHELP(WHERE);
/*
Node to Perform Call-out:

This is the number of the node to perform the call-out for this QWK
network hub.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0
				,"Node to Perform Call-out",str,3,K_EDIT|K_NUMBER);
			cfg.qhub[num]->node=atoi(str);
			break;
		case 5:
			j=0;
			while(1) {
				for(i=0;i<7;i++)
					sprintf(opt[i],"%s        %s"
						,wday[i],(cfg.qhub[num]->days&(1<<i)) ? "Yes":"No");
				opt[i][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Days to Perform Call-out:

These are the days that a call-out will be performed for this QWK
network hub.
*/
				i=uifc.list(WIN_MID,0,0,0,&j,0
					,"Days to Perform Call-out",opt);
				if(i==-1)
					break;
				cfg.qhub[num]->days^=(1<<i);
				uifc.changes=1; }
			break;
		case 6:
			i=1;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			uifc.savnum=2;
			SETHELP(WHERE);
/*
Perform Call-out at a Specific Time:

If you want the system call this QWK network hub at a specific time,
set this option to Yes. If you want the system to call this network
hub more than once a day at predetermined intervals, set this option to
No.
*/
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Perform Call-out at a Specific Time",opt);
			if(i==0) {
				sprintf(str,"%2.2u:%2.2u",cfg.qhub[num]->time/60
					,cfg.qhub[num]->time%60);
				SETHELP(WHERE);
/*
Time to Perform Call-out:

This is the time (in 24 hour HH:MM format) to perform the call-out to
this QWK network hub.
*/
				if(uifc.input(WIN_MID|WIN_SAV,0,0
					,"Time to Perform Call-out (HH:MM)"
					,str,5,K_UPPER|K_EDIT)>0) {
					cfg.qhub[num]->freq=0;
					cfg.qhub[num]->time=atoi(str)*60;
					if((p=strchr(str,':'))!=NULL)
						cfg.qhub[num]->time+=atoi(p+1); } }
			else if(i==1) {
				sprintf(str,"%u",cfg.qhub[num]->freq
					&& cfg.qhub[num]->freq<=1440 ? 1440/cfg.qhub[num]->freq : 0);
				SETHELP(WHERE);
/*
Number of Call-outs Per Day:

This is the maximum number of times the system will perform a call-out
per day to this QWK network hub. This value is actually converted by
Synchronet into minutes between call-outs and when the BBS is idle
and this number of minutes since the last call-out is reached, it will
perform a call-out.
*/
				if(uifc.input(WIN_MID|WIN_SAV,0,0
					,"Number of Call-outs Per Day"
					,str,4,K_NUMBER|K_EDIT)>0) {
					cfg.qhub[num]->time=0;
					i=atoi(str);
					if(i && i<=1440)
						cfg.qhub[num]->freq=1440/i;
					else
						cfg.qhub[num]->freq=0; } }
			break;
		case 7:
			qhub_sub_edit(num);
			break; } }
}

void qhub_sub_edit(uint num)
{
	char str[256];
	int j,k,l,m,n,bar=0;

k=0;
while(1) {
	for(j=0;j<cfg.qhub[num]->subs;j++)
		sprintf(opt[j],"%-5u %-*.*s %-*.*s"
			,cfg.qhub[num]->conf[j]
			,LEN_GSNAME,LEN_GSNAME
			,cfg.grp[cfg.sub[cfg.qhub[num]->sub[j]]->grp]->sname
			,LEN_SSNAME,LEN_SSNAME
			,cfg.sub[cfg.qhub[num]->sub[j]]->sname);
	opt[j][0]=0;
	uifc.savnum=2;
	j=WIN_BOT|WIN_SAV|WIN_ACT;
	if(cfg.qhub[num]->subs<MAX_OPTS)
		j|=WIN_INS|WIN_INSACT|WIN_XTR;
	if(cfg.qhub[num]->subs)
		j|=WIN_DEL;
	SETHELP(WHERE);
/*
QWK Networked Sub-boards:

This is a list of the sub-boards that are networked with this QWK
network hub.

To add a sub-board, select the desired location and hit  INS .

To remove a sub-board, select it and hit  DEL .

To configure a sub-board for this QWK network hub, select it and hit
 ENTER .
*/
	j=uifc.list(j,0,0,0,&k,&bar
		,"Networked Sub-boards",opt);
	if(j==-1)
		break;
	if((j&MSK_ON)==MSK_INS) {
		j&=MSK_OFF;
		uifc.savnum=3;
		if((l=getsub())==-1)
			continue;
		uifc.savnum=3;
		SETHELP(WHERE);
/*
Conference Number on Hub:

This is the number of the conference on the QWK network hub, that this
sub-board is networked with. On Synchronet systems, this number is
derived by multiplying the group number by 10 and adding the sub-board
number. For example, group 2, sub-board 3, is conference number 203.

It is important to understand that this is NOT the conference number of
this sub-board on your system. It is the number of the conference this
sub-board is networked with on this QWK network hub.
*/
		if(uifc.input(WIN_MID|WIN_SAV,0,0
			,"Conference Number on Hub"
			,str,5,K_NUMBER)<1)
			continue;
		strcpy(opt[0],"Strip out");
		strcpy(opt[1],"Leave in");
		strcpy(opt[2],"Expand to ANSI");
		opt[3][0]=0;
		m=0;
		SETHELP(WHERE);
/*
Ctrl-A Codes:

You are being prompted for the method of handling Ctrl-A attribute codes
generated by Synchronet. If this QWK network hub is a Synchronet BBS,
set this option to Leave in. If the QWK network hub is not a Synchronet
BBS, but allows ANSI escape sequences in messages, set this option to
Expand to ANSI. If the QWK network hub is not a Synchronet BBS and does
not support ANSI escape sequences in messages (or you're not sure), set
this option to Strip out.
*/
		if((m=uifc.list(WIN_MID|WIN_SAV,0,0,0,&m,0
			,"Ctrl-A Codes",opt))==-1)
			continue;
		if((cfg.qhub[num]->sub=(ushort *)REALLOC(cfg.qhub[num]->sub
			,sizeof(ushort *)*(cfg.qhub[num]->subs+1)))==NULL
		|| (cfg.qhub[num]->conf=(ushort *)REALLOC(cfg.qhub[num]->conf
            ,sizeof(ushort *)*(cfg.qhub[num]->subs+1)))==NULL
		|| (cfg.qhub[num]->mode=(uchar *)REALLOC(cfg.qhub[num]->mode
			,sizeof(uchar *)*(cfg.qhub[num]->subs+1)))==NULL) {
			errormsg(WHERE,ERR_ALLOC,nulstr,cfg.qhub[num]->subs+1);
			continue; }
		if(cfg.qhub[num]->subs) 			  /* insert */
			for(n=cfg.qhub[num]->subs;n>j;n--) {
				cfg.qhub[num]->sub[n]=cfg.qhub[num]->sub[n-1];
				cfg.qhub[num]->conf[n]=cfg.qhub[num]->conf[n-1];
				cfg.qhub[num]->mode[n]=cfg.qhub[num]->mode[n-1]; }
		if(!m)
			cfg.qhub[num]->mode[j]=A_STRIP;
		else if(m==1)
			cfg.qhub[num]->mode[j]=A_LEAVE;
		else
			cfg.qhub[num]->mode[j]=A_EXPAND;
		cfg.qhub[num]->sub[j]=l;
		cfg.qhub[num]->conf[j]=atoi(str);
		cfg.qhub[num]->subs++;
		uifc.changes=1;
		continue; }
	if((j&MSK_ON)==MSK_DEL) {
		j&=MSK_OFF;
		cfg.qhub[num]->subs--;
		while(j<cfg.qhub[num]->subs) {
			cfg.qhub[num]->sub[j]=cfg.qhub[num]->sub[j+1];
			cfg.qhub[num]->mode[j]=cfg.qhub[num]->mode[j+1];
			cfg.qhub[num]->conf[j]=cfg.qhub[num]->conf[j+1];
			j++; }
		uifc.changes=1;
		continue; }
	l=0;
	while(1) {
		n=0;
		sprintf(opt[n++],"%-22.22s%.*s %.*s"
			,"Sub-board"
			,LEN_GSNAME
			,cfg.grp[cfg.sub[cfg.qhub[num]->sub[j]]->grp]->sname
			,LEN_SSNAME
			,cfg.sub[cfg.qhub[num]->sub[j]]->sname);
		sprintf(opt[n++],"%-22.22s%u"
			,"Conference Number",cfg.qhub[num]->conf[j]);
		sprintf(opt[n++],"%-22.22s%s"
			,"Ctrl-A Codes",cfg.qhub[num]->mode[j]==A_STRIP ?
			"Strip out" : cfg.qhub[num]->mode[j]==A_LEAVE ?
			"Leave in" : "Expand to ANSI");
		opt[n][0]=0;
		uifc.savnum=3;
		SETHELP(WHERE);
/*
QWK Netted Sub-board:

You are configuring the options for this sub-board for this QWK network
hub.
*/
		l=uifc.list(WIN_MID|WIN_SAV|WIN_ACT,0,0,
			22+LEN_GSNAME+LEN_SSNAME,&l,0
			,"Netted Sub-board",opt);
		if(l==-1)
			break;
		if(!l) {
			uifc.savnum=4;
			m=getsub();
			if(m!=-1) {
				cfg.qhub[num]->sub[j]=m;
				uifc.changes=1; } }
		else if(l==1) {
			uifc.savnum=4;
			SETHELP(WHERE);
/*
Conference Number on Hub:

This is the number of the conference on the QWK network hub, that this
sub-board is networked with. On Synchronet systems, this number is
derived by multiplying the group number by 10 and adding the sub-board
number. For example, group 2, sub-board 3, is conference number 203.

It is important to understand that this is NOT the conference number of
this sub-board on your system. It is the number of the conference this
sub-board is networked with on this QWK network hub.
*/
			if(uifc.input(WIN_MID|WIN_SAV,0,0
				,"Conference Number on Hub"
				,str,5,K_NUMBER)>0)
				cfg.qhub[num]->conf[j]=atoi(str); }
		else if(l==2) {
			strcpy(opt[0],"Strip out");
			strcpy(opt[1],"Leave in");
			strcpy(opt[2],"Expand to ANSI");
			opt[3][0]=0;
			m=0;
			uifc.savnum=4;
			SETHELP(WHERE);
/*
Ctrl-A Codes:

You are being prompted for the method of handling Ctrl-A attribute codes
generated by Synchronet. If this QWK network hub is a Synchronet BBS,
set this option to Leave in. If the QWK network hub is not a Synchronet
BBS, but allows ANSI escape sequences in messages, set this option to
Expand to ANSI. If the QWK network hub is not a Synchronet BBS and does
not support ANSI escape sequences in messages (or you're not sure), set
this option to Strip out.
*/
			m=uifc.list(WIN_MID|WIN_SAV,0,0,0,&m,0
				,"Ctrl-A Codes",opt);
			uifc.changes=1;
			if(!m)
				cfg.qhub[num]->mode[j]=A_STRIP;
			else if(m==1)
				cfg.qhub[num]->mode[j]=A_LEAVE;
			else if(m==2)
				cfg.qhub[num]->mode[j]=A_EXPAND; } } }
}

void phub_edit(int num)
{
	static int phub_dflt;
	char *p,done=0,str[256];
	int i,j;

while(!done) {
	i=0;
	sprintf(opt[i++],"%-27.27s%s","Hub Site Name",cfg.phub[num]->name);
	sprintf(opt[i++],"%-27.27s%.40s","Call-out Command Line",cfg.phub[num]->call);
	sprintf(opt[i++],"%-27.27s%u","Call-out Node",cfg.phub[num]->node);
	sprintf(opt[i++],"%-27.27s%s","Call-out Days",daystr(cfg.phub[num]->days));
	if(cfg.phub[num]->freq) {
		sprintf(str,"%u times a day",1440/cfg.phub[num]->freq);
		sprintf(opt[i++],"%-27.27s%s","Call-out Frequency",str); }
	else {
		sprintf(str,"%2.2u:%2.2u",cfg.phub[num]->time/60
			,cfg.phub[num]->time%60);
		sprintf(opt[i++],"%-27.27s%s","Call-out Time",str); }
	opt[i][0]=0;
	sprintf(str,"%s Network Hub",cfg.phub[num]->name);
	uifc.savnum=1;
	SETHELP(WHERE);
/*
PostLink Network Hub Configuration:

This menu allows you to configure options specific to this network hub.
*/
	switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,0,&phub_dflt,0
		,str,opt)) {
		case -1:
			done=1;
			break;
		case 0:
			SETHELP(WHERE);
/*
Network Hub Site Name:

This is the Site Name of this hub. It is used for only for reference.
*/
			strcpy(str,cfg.phub[num]->name);	/* save */
			if(!uifc.input(WIN_MID|WIN_SAV,0,0,"Hub Site Name"
				,cfg.phub[num]->name,10,K_UPPER|K_EDIT))
				strcpy(cfg.phub[num]->name,str);	/* restore */
			break;
		case 1:
			SETHELP(WHERE);
/*
Network Hub Call-out Command Line:

This is the command line to use to initiate a call-out to this network
hub.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0,"Call-out Command"
				,cfg.phub[num]->call,50,K_EDIT);
			break;
		case 2:
			sprintf(str,"%u",cfg.phub[num]->node);
			SETHELP(WHERE);
/*
Node to Perform Call-out:

This is the number of the node to perform the call-out for this network
hub.
*/
			uifc.input(WIN_MID|WIN_SAV,0,0
				,"Node to Perform Call-out",str,3,K_EDIT|K_NUMBER);
			cfg.phub[num]->node=atoi(str);
			break;
		case 3:
			j=0;
			while(1) {
				for(i=0;i<7;i++)
					sprintf(opt[i],"%s        %s"
						,wday[i],(cfg.phub[num]->days&(1<<i)) ? "Yes":"No");
				opt[i][0]=0;
				uifc.savnum=2;
				SETHELP(WHERE);
/*
Days to Perform Call-out:

These are the days that a call-out will be performed for this network
hub.
*/
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0
					,"Days to Perform Call-out",opt);
				if(i==-1)
					break;
				cfg.phub[num]->days^=(1<<i);
				uifc.changes=1; }
			break;
		case 4:
			i=1;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			uifc.savnum=2;
			SETHELP(WHERE);
/*
Perform Call-out at a Specific Time:

If you want the system call this network hub at a specific time, set
this option to Yes. If you want the system to call this hub more than
once a day at predetermined intervals, set this option to No.
*/
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Perform Call-out at a Specific Time",opt);
			if(i==0) {
				sprintf(str,"%2.2u:%2.2u",cfg.phub[num]->time/60
					,cfg.phub[num]->time%60);
				SETHELP(WHERE);
/*
Time to Perform Call-out:

This is the time (in 24 hour HH:MM format) to perform the call-out to
this network hub.
*/
				if(uifc.input(WIN_MID|WIN_SAV,0,0
					,"Time to Perform Call-out (HH:MM)"
					,str,5,K_UPPER|K_EDIT)>0) {
					cfg.phub[num]->freq=0;
					cfg.phub[num]->time=atoi(str)*60;
					if((p=strchr(str,':'))!=NULL)
						cfg.phub[num]->time+=atoi(p+1); } }
			else if(i==1) {
				sprintf(str,"%u",cfg.phub[num]->freq
					&& cfg.phub[num]->freq<=1440 ? 1440/cfg.phub[num]->freq : 0);
				SETHELP(WHERE);
/*
Number of Call-outs Per Day:

This is the maximum number of times the system will perform a call-out
per day to this network hub. This value is actually converted by
Synchronet into minutes between call-outs and when the BBS is idle
and this number of minutes since the last call-out is reached, it will
perform a call-out.
*/
				if(uifc.input(WIN_MID|WIN_SAV,0,0
					,"Number of Call-outs Per Day"
					,str,4,K_NUMBER|K_EDIT)>0) {
					cfg.phub[num]->time=0;
					i=atoi(str);
					if(i && i<=1440)
						cfg.phub[num]->freq=1440/i;
					else
						cfg.phub[num]->freq=0; } }
			break; } }
}

char *daystr(char days)
{
	static char str[256];
	int i;

	days&=0x7f;

	if(days==0)		return("None");

	if(days==0x7f)	return("All");

	str[0]=0;
	for(i=0;i<7;i++) {
		if(days&(1<<i))
			strcat(str,wday[i]);
		else
			strcat(str,"   ");
		strcat(str," "); 
	}
	return(str);
}
