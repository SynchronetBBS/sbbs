/* Synchronet FidoNet EchoMail Scanning/Tossing and NetMail Tossing Utility */

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

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sbbs.h"
#include "sbbsecho.h"
#include "filewrap.h"	/* O_DENYNONE */

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/* Supports wildcard of "ALL" in fields										*/
/****************************************************************************/
faddr_t atofaddr(const char *instr)
{
	char *p,str[51];
    faddr_t addr;

	SAFECOPY(str, instr);
	p=str;
	FIND_WHITESPACE(p);
	*p=0;
	if(!stricmp(str,"ALL")) {
		addr.zone=addr.net=addr.node=addr.point=0xffff;
		return(addr); 
	}
	addr.zone=addr.net=addr.node=addr.point=0;
	if((p=strchr(str,':'))!=NULL) {
		if(!strnicmp(str,"ALL:",4))
			addr.zone=0xffff;
		else
			addr.zone=atoi(str);
		p++;
		if(!strnicmp(p,"ALL",3))
			addr.net=0xffff;
		else
			addr.net=atoi(p);
	}
	else {
		addr.zone=1;
		addr.net=atoi(str);
	}
	if(!addr.zone)              /* no such thing as zone 0 */
		addr.zone=1;
	if((p=strchr(str,'/'))!=NULL) {
		p++;
		if(!strnicmp(p,"ALL",3))
			addr.node=0xffff;
		else
			addr.node=atoi(p);
	}
	else {
		if(!addr.net)
			addr.net=1;
		addr.node=atoi(str);
	}
	if((p=strchr(str,'.'))!=NULL) {
		p++;
		if(!strnicmp(p,"ALL",3))
			addr.point=0xffff;
		else
			addr.point=atoi(p);
	}
	return(addr);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/* Supports wildcard of "ALL" in fields										*/
/****************************************************************************/
const char *faddrtoa(const faddr_t* addr)
{
    static char str[25];
	char tmp[25];

	str[0]=0;
	if(addr->zone==0xffff)
		strcpy(str,"ALL");
	else if(addr->zone) {
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

bool faddr_contains_wildcard(const faddr_t* addr)
{
	return addr->zone==0xffff || addr->net==0xffff || addr->node==0xffff || addr->point==0xffff;
}

/******************************************************************************
 This function returns the number of the node in the SBBSECHO.CFG file which
 matches the address passed to it (or cfg.nodecfgs if no match).
 ******************************************************************************/
int matchnode(sbbsecho_cfg_t* cfg, faddr_t addr, int exact)
{
	uint i;

	if(exact!=2) {
		for(i=0;i<cfg->nodecfgs;i++) 				/* Look for exact match */
			if(!memcmp(&cfg->nodecfg[i].addr,&addr,sizeof(faddr_t)))
				break;
		if(exact || i<cfg->nodecfgs)
			return(i);
	}

	for(i=0;i<cfg->nodecfgs;i++) 					/* Look for point match */
		if(cfg->nodecfg[i].addr.point==0xffff
			&& addr.zone==cfg->nodecfg[i].addr.zone
			&& addr.net==cfg->nodecfg[i].addr.net
			&& addr.node==cfg->nodecfg[i].addr.node)
			break;
	if(i<cfg->nodecfgs)
		return(i);

	for(i=0;i<cfg->nodecfgs;i++) 					/* Look for node match */
		if(cfg->nodecfg[i].addr.node==0xffff
			&& addr.zone==cfg->nodecfg[i].addr.zone
			&& addr.net==cfg->nodecfg[i].addr.net)
			break;
	if(i<cfg->nodecfgs)
		return(i);

	for(i=0;i<cfg->nodecfgs;i++) 					/* Look for net match */
		if(cfg->nodecfg[i].addr.net==0xffff
			&& addr.zone==cfg->nodecfg[i].addr.zone)
			break;
	if(i<cfg->nodecfgs)
		return(i);

	for(i=0;i<cfg->nodecfgs;i++) 					/* Look for total wild */
		if(cfg->nodecfg[i].addr.zone==0xffff)
			break;
	return(i);
}

nodecfg_t* findnodecfg(sbbsecho_cfg_t* cfg, faddr_t addr, int exact)
{
	uint node = matchnode(cfg, addr, exact);

	if(node < cfg->nodecfgs)
		return &cfg->nodecfg[node];

	return NULL;
}

void get_default_echocfg(sbbsecho_cfg_t* cfg)
{
	cfg->maxpktsize					= DFLT_PKT_SIZE;
	cfg->maxbdlsize					= DFLT_BDL_SIZE;
	cfg->badecho					= -1;
	cfg->log_level					= LOG_INFO;
	cfg->check_path					= true;
	cfg->zone_blind					= false;
	cfg->zone_blind_threshold		= 0xffff;
	cfg->sysop_alias_list			= strListSplitCopy(NULL, "SYSOP", ",");
	cfg->max_echomail_age			= 60*24*60*60;
	cfg->bsy_timeout				= 12*60*60;
	cfg->bso_lock_attempts			= 60;
	cfg->bso_lock_delay				= 10;
	cfg->delete_packets				= true;
	cfg->delete_netmail				= true;
	cfg->echomail_notify			= true;
	cfg->kill_empty_netmail			= true;
	cfg->use_ftn_domains			= false;
	cfg->strict_packet_passwords	= true;
	cfg->relay_filtered_msgs		= false;
	cfg->umask						= 077;
	cfg->areafile_backups			= 100;
	cfg->cfgfile_backups			= 100;
	cfg->auto_add_subs				= true;
}

char* pktTypeStringList[] = {"2+", "2e", "2.2", "2", NULL};		// Must match enum pkt_type
char* mailStatusStringList[] = {"Normal", "Hold", "Crash", NULL};

bool sbbsecho_read_ini(sbbsecho_cfg_t* cfg)
{
	FILE*		fp;
	str_list_t	ini;
	char		value[INI_MAX_VALUE_LEN];

	get_default_echocfg(cfg);

	if((fp=iniOpenFile(cfg->cfgfile, /* create: */false))==NULL)
		return false;
	ini = iniReadFile(fp);
	iniCloseFile(fp);

	/************************/
	/* Global/root section: */
	/************************/
	iniFreeStringList(cfg->sysop_alias_list);
	SAFECOPY(cfg->inbound		, iniGetString(ini, ROOT_SECTION, "Inbound"			,DEFAULT_INBOUND		, value));
	SAFECOPY(cfg->secure_inbound, iniGetString(ini, ROOT_SECTION, "SecureInbound"	,DEFAULT_SECURE_INBOUND	, value));
	SAFECOPY(cfg->outbound		, iniGetString(ini, ROOT_SECTION, "Outbound"		,DEFAULT_OUTBOUND		, value));
	SAFECOPY(cfg->areafile		, iniGetString(ini, ROOT_SECTION, "AreaFile"		,DEFAULT_AREA_FILE		, value));
	SAFECOPY(cfg->badareafile	, iniGetString(ini, ROOT_SECTION, "BadAreaFile"		,DEFAULT_BAD_AREA_FILE	, value));
	SAFECOPY(cfg->echostats		, iniGetString(ini, ROOT_SECTION, "EchoStats"		,DEFAULT_ECHOSTATS_FILE	, value));
	SAFECOPY(cfg->logfile		, iniGetString(ini, ROOT_SECTION, "LogFile"			,DEFAULT_LOG_FILE		, value));
	SAFECOPY(cfg->logtime		, iniGetString(ini, ROOT_SECTION, "LogTimeFormat"	,DEFAULT_LOG_TIME_FMT	, value));
	SAFECOPY(cfg->temp_dir		, iniGetString(ini, ROOT_SECTION, "TempDirectory"	,DEFAULT_TEMP_DIR		, value));
	SAFECOPY(cfg->outgoing_sem	, iniGetString(ini, ROOT_SECTION, "OutgoingSemaphore",	"", value));
	cfg->log_level				= iniGetLogLevel(ini, ROOT_SECTION, "LogLevel", cfg->log_level);
	cfg->strip_lf				= iniGetBool(ini, ROOT_SECTION, "StripLineFeeds", cfg->strip_lf);
	cfg->flo_mailer				= iniGetBool(ini, ROOT_SECTION, "BinkleyStyleOutbound", cfg->flo_mailer);
	cfg->bsy_timeout			= (ulong)iniGetDuration(ini, ROOT_SECTION, "BsyTimeout", cfg->bsy_timeout);
	cfg->bso_lock_attempts		= iniGetLongInt(ini, ROOT_SECTION, "BsoLockAttempts", cfg->bso_lock_attempts);
	cfg->bso_lock_delay			= (ulong)iniGetDuration(ini, ROOT_SECTION, "BsoLockDelay", cfg->bso_lock_delay);
	cfg->use_ftn_domains		= iniGetBool(ini, ROOT_SECTION, "UseFTNDomains", cfg->use_ftn_domains);
	cfg->strict_packet_passwords= iniGetBool(ini, ROOT_SECTION, "StrictPacketPasswords", cfg->strict_packet_passwords);
	cfg->relay_filtered_msgs	= iniGetBool(ini, ROOT_SECTION, "RelayFilteredMsgs", cfg->relay_filtered_msgs);
	cfg->umask					= iniGetInteger(ini, ROOT_SECTION, "umask", cfg->umask);
	cfg->areafile_backups		= iniGetInteger(ini, ROOT_SECTION, "AreaFileBackups", cfg->areafile_backups);
	cfg->cfgfile_backups		= iniGetInteger(ini, ROOT_SECTION, "CfgFileBackups", cfg->cfgfile_backups);

	/* EchoMail options: */
	cfg->maxbdlsize				= (ulong)iniGetBytes(ini, ROOT_SECTION, "BundleSize", 1, cfg->maxbdlsize);
	cfg->maxpktsize				= (ulong)iniGetBytes(ini, ROOT_SECTION, "PacketSize", 1, cfg->maxpktsize);
	cfg->check_path				= iniGetBool(ini, ROOT_SECTION, "CheckPathsForDupes", cfg->check_path);
	cfg->secure_echomail		= iniGetBool(ini, ROOT_SECTION, "SecureEchomail", cfg->secure_echomail);
	cfg->echomail_notify		= iniGetBool(ini, ROOT_SECTION, "EchomailNotify", cfg->echomail_notify);
	cfg->convert_tear			= iniGetBool(ini, ROOT_SECTION, "ConvertTearLines", cfg->convert_tear);
	cfg->trunc_bundles			= iniGetBool(ini, ROOT_SECTION, "TruncateBundles", cfg->trunc_bundles);
	cfg->zone_blind				= iniGetBool(ini, ROOT_SECTION, "ZoneBlind", cfg->zone_blind);
	cfg->zone_blind_threshold	= iniGetShortInt(ini, ROOT_SECTION, "ZoneBlindThreshold", cfg->zone_blind_threshold);
	cfg->add_from_echolists_only= iniGetBool(ini, ROOT_SECTION, "AreaAddFromEcholistsOnly", cfg->add_from_echolists_only);
	cfg->max_echomail_age		= (ulong)iniGetDuration(ini, ROOT_SECTION, "MaxEchomailAge", cfg->max_echomail_age);
	SAFECOPY(cfg->areamgr,		  iniGetString(ini, ROOT_SECTION, "AreaManager", "SYSOP", value));
	cfg->auto_add_subs			= iniGetBool(ini, ROOT_SECTION, "AutoAddSubs", cfg->auto_add_subs);

	/* NetMail options: */
	SAFECOPY(cfg->default_recipient, iniGetString(ini, ROOT_SECTION, "DefaultRecipient", "", value));
	cfg->sysop_alias_list			= iniGetStringList(ini, ROOT_SECTION, "SysopAliasList", ",", "SYSOP");
	cfg->fuzzy_zone					= iniGetBool(ini, ROOT_SECTION, "FuzzyNetmailZones", cfg->fuzzy_zone);
	cfg->ignore_netmail_dest_addr	= iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailDestAddr", cfg->ignore_netmail_dest_addr);
	cfg->ignore_netmail_sent_attr	= iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailSentAttr", cfg->ignore_netmail_sent_attr);
	cfg->ignore_netmail_kill_attr	= iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailKillAttr", cfg->ignore_netmail_kill_attr);
	cfg->ignore_netmail_recv_attr	= iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailRecvAttr", cfg->ignore_netmail_recv_attr);
	cfg->ignore_netmail_local_attr	= iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailLocalAttr", cfg->ignore_netmail_local_attr);
	cfg->kill_empty_netmail			= iniGetBool(ini, ROOT_SECTION, "KillEmptyNetmail", cfg->kill_empty_netmail);
	cfg->delete_netmail				= iniGetBool(ini, ROOT_SECTION, "DeleteNetmail", cfg->delete_netmail);
	cfg->delete_packets				= iniGetBool(ini, ROOT_SECTION, "DeletePackets", cfg->delete_packets);
	cfg->max_netmail_age			= (ulong)iniGetDuration(ini, ROOT_SECTION, "MaxNetmailAge", cfg->max_netmail_age);

	/******************/
	/* Archive Types: */
	/******************/
	str_list_t archivelist = iniGetSectionList(ini, "archive:");
	cfg->arcdefs = strListCount(archivelist);
	if((cfg->arcdef = realloc(cfg->arcdef, sizeof(arcdef_t)*cfg->arcdefs)) == NULL) {
		strListFree(&archivelist);
		return false;
	}
	cfg->arcdefs = 0;
	char* archive;
	while((archive=strListRemove(&archivelist, 0)) != NULL) {
		arcdef_t* arc = &cfg->arcdef[cfg->arcdefs++];
		memset(arc, 0, sizeof(arcdef_t));
		SAFECOPY(arc->name, truncsp(archive+8));
		arc->byteloc = iniGetInteger(ini, archive, "SigOffset", 0);
		SAFECOPY(arc->hexid, truncsp(iniGetString(ini, archive, "Sig", "", value)));
		SAFECOPY(arc->pack, truncsp(iniGetString(ini, archive, "Pack", "", value)));
		SAFECOPY(arc->unpack, truncsp(iniGetString(ini, archive, "Unpack", "", value)));
	}
	strListFree(&archivelist);

	/****************/
	/* Links/Nodes: */
	/****************/
	str_list_t nodelist = iniGetSectionList(ini, "node:");
	cfg->nodecfgs = strListCount(nodelist);
	if((cfg->nodecfg = realloc(cfg->nodecfg, sizeof(nodecfg_t)*cfg->nodecfgs)) == NULL) {
		strListFree(&nodelist);
		return false;
	}
	cfg->nodecfgs = 0;
	char* node;
	while((node=strListRemove(&nodelist, 0)) != NULL) {
		nodecfg_t* ncfg = &cfg->nodecfg[cfg->nodecfgs++];
		memset(ncfg, 0, sizeof(nodecfg_t));
		ncfg->addr = atofaddr(node+5);
		if(iniGetString(ini, node, "route", NULL, value) != NULL && value[0])
			ncfg->route = atofaddr(value);
		SAFECOPY(ncfg->password	, iniGetString(ini, node, "AreaFixPwd", "", value));
		SAFECOPY(ncfg->pktpwd	, iniGetString(ini, node, "PacketPwd", "", value));
		SAFECOPY(ncfg->ticpwd	, iniGetString(ini, node, "TicFilePwd", "", value));
		SAFECOPY(ncfg->name		, iniGetString(ini, node, "Name", "", value));
		SAFECOPY(ncfg->comment	, iniGetString(ini, node, "Comment", "", value));
		if(!faddr_contains_wildcard(&ncfg->addr)) {
			SAFECOPY(ncfg->inbox	, iniGetString(ini, node, "inbox", "", value));
			SAFECOPY(ncfg->outbox	, iniGetString(ini, node, "outbox", "", value));
			char		inbox[MAX_PATH + 1];
			char		insec[MAX_PATH + 1];
			char		inbound[MAX_PATH + 1];
			char		outbox[MAX_PATH + 1];
			char		outbound[MAX_PATH + 1];
			FULLPATH(inbox, ncfg->inbox, sizeof(inbox))			, backslash(inbox);
			FULLPATH(outbox, ncfg->outbox, sizeof(outbox))		, backslash(outbox);
			FULLPATH(insec, cfg->secure_inbound, sizeof(insec))	, backslash(insec);
			FULLPATH(inbound, cfg->inbound, sizeof(inbound))	, backslash(inbound);
			FULLPATH(outbound, cfg->outbound, sizeof(outbound))	, backslash(outbound);
			if (stricmp(inbound, inbox) == 0 || stricmp(insec, inbox) == 0)
				ncfg->inbox[0] = 0;
			if (stricmp(outbound, outbox) == 0)
				ncfg->outbox[0] = 0;
		}
		ncfg->grphub			= iniGetStringList(ini, node, "GroupHub", ",", "");
		ncfg->keys				= iniGetStringList(ini, node, "keys", ",", "");
		ncfg->pkt_type			= iniGetEnum(ini, node, "PacketType", pktTypeStringList, ncfg->pkt_type);
		ncfg->areafix			= iniGetBool(ini, node, "AreaFix", ncfg->password[0] ? true : false);
		ncfg->send_notify		= iniGetBool(ini, node, "notify", ncfg->send_notify);
		ncfg->passive			= iniGetBool(ini, node, "passive", ncfg->passive);
		ncfg->direct			= iniGetBool(ini, node, "direct", ncfg->direct);
		ncfg->status			= iniGetEnum(ini, node, "status", mailStatusStringList, ncfg->status);
		char* archive = iniGetString(ini, node, "archive", "", value);
		for(uint i=0; i<cfg->arcdefs; i++) {
			if(stricmp(cfg->arcdef[i].name, archive) == 0) {
				ncfg->archive = &cfg->arcdef[i];
				break;
			}
		}
	}
	strListFree(&nodelist);

	/**************/
	/* EchoLists: */
	/**************/
	str_list_t echolists = iniGetSectionList(ini, "echolist:");
	cfg->listcfgs = strListCount(echolists);
	if((cfg->listcfg = realloc(cfg->listcfg, sizeof(echolist_t)*cfg->listcfgs)) == NULL) {
		strListFree(&echolists);
		return false;
	}
	cfg->listcfgs = 0;
	char* echolist;
	while((echolist=strListRemove(&echolists, 0)) != NULL) {
		echolist_t* elist = &cfg->listcfg[cfg->listcfgs++];
		memset(elist, 0, sizeof(echolist_t));
		SAFECOPY(elist->listpath, echolist + 9);
		elist->keys = iniGetStringList(ini, echolist, "keys", ",", "");
		if(iniGetString(ini, echolist, "hub", "", value) != NULL && value[0])
			elist->hub = atofaddr(value);
		elist->forward = iniGetBool(ini, echolist, "fwd", false);
		SAFECOPY(elist->password, iniGetString(ini,echolist, "pwd", "", value));
		SAFECOPY(elist->areamgr, iniGetString(ini, echolist, "AreaMgr", FIDO_AREAMGR_NAME, value));
	}
	strListFree(&echolists);

	/* make sure we have some sane "maximum" size values here: */
	if(cfg->maxpktsize<1024)
		cfg->maxpktsize=DFLT_PKT_SIZE;
	if(cfg->maxbdlsize<1024)
		cfg->maxbdlsize=DFLT_BDL_SIZE;

	strListFree(&ini);

	return true;
}

bool sbbsecho_read_ftn_domains(sbbsecho_cfg_t* cfg, const char * ctrl_dir)
{
	FILE*		fp;
	str_list_t	ini;
	char		path[MAX_PATH+1];
	str_list_t	domains;
	const char *	domain;
	str_list_t	zones;
	const char *	zone;
	struct zone_mapping * mapping;
	struct zone_mapping * old_mapping;

	// First, free any old mappings...
	for (mapping = cfg->zone_map; mapping;) {
		FREE_AND_NULL(mapping->domain);
		FREE_AND_NULL(mapping->root);
		old_mapping = mapping;
		mapping = old_mapping->next;
		FREE_AND_NULL(old_mapping);
	}
	cfg->zone_map = NULL;

	if(cfg->use_ftn_domains) {
		SAFEPRINTF(path, "%sftn_domains.ini", ctrl_dir);
		if((fp=iniOpenFile(path, /* create: */false))==NULL)
			return false;
		ini = iniReadFile(fp);
		iniCloseFile(fp);
		domains = iniGetSectionList(ini, NULL);
		while((domain = strListRemove(&domains, 0)) != NULL) {
			zones = iniGetStringList(ini, domain, "Zones", ",", NULL);
			while((zone = strListRemove(&zones, 0)) != NULL) {
				mapping = (struct zone_mapping *)malloc(sizeof(struct zone_mapping));

				if (mapping == NULL) {
					strListFree(&zones);
					strListFree(&domains);
					return false;
				}
				mapping->zone = (uint16_t)strtol(zone, NULL, 10);
				mapping->domain = strdup(domain);
				mapping->root = strdup(iniGetString(ini, domain, "OutboundRoot", cfg->outbound, path));
				mapping->next = cfg->zone_map;
				cfg->zone_map = mapping;
			}
			strListFree(&zones);
		}
		strListFree(&domains);
		strListFree(&ini);
	}
	return true;
}

bool sbbsecho_write_ini(sbbsecho_cfg_t* cfg)
{
	char section[128];
	FILE* fp;
	str_list_t	ini;

	if(cfg->cfgfile_backups)
		backup(cfg->cfgfile, cfg->cfgfile_backups, /* ren: */false);

	if((fp=iniOpenFile(cfg->cfgfile, /* create: */true))==NULL)
		return false;
	ini = iniReadFile(fp);

	ini_style_t style = {  .value_separator = " = " };
	iniSetDefaultStyle(style);

	/************************/
	/* Global/root section: */
	/************************/
	iniSetString(&ini,		ROOT_SECTION, "Inbound"					,cfg->inbound					,NULL);
	iniSetString(&ini,		ROOT_SECTION, "SecureInbound"			,cfg->secure_inbound			,NULL);
	iniSetString(&ini,		ROOT_SECTION, "Outbound"				,cfg->outbound					,NULL);
	iniSetString(&ini,		ROOT_SECTION, "AreaFile"				,cfg->areafile					,NULL);
	iniSetInteger(&ini,		ROOT_SECTION, "AreaFileBackups"			,cfg->areafile_backups			,NULL);
	iniSetInteger(&ini,		ROOT_SECTION, "CfgFileBackups"			,cfg->cfgfile_backups			,NULL);
	iniSetString(&ini,		ROOT_SECTION, "BadAreaFile"				,cfg->badareafile				,NULL);
	iniSetString(&ini,		ROOT_SECTION, "EchoStats"				,cfg->echostats					,NULL);
	if(cfg->logfile[0])
	iniSetString(&ini,		ROOT_SECTION, "LogFile"					,cfg->logfile					,NULL);
	if(cfg->logtime[0])
	iniSetString(&ini,		ROOT_SECTION, "LogTimeFormat"			,cfg->logtime					,NULL);
	if(cfg->temp_dir[0])
	iniSetString(&ini,		ROOT_SECTION, "TempDirectory"			,cfg->temp_dir					,NULL);
	iniSetString(&ini,		ROOT_SECTION, "OutgoingSemaphore"		,cfg->outgoing_sem				,NULL);
	iniSetBytes(&ini,		ROOT_SECTION, "BundleSize"				,1,cfg->maxbdlsize				,NULL);
	iniSetBytes(&ini,		ROOT_SECTION, "PacketSize"				,1,cfg->maxpktsize				,NULL);
	iniSetStringList(&ini,	ROOT_SECTION, "SysopAliasList", ","		,cfg->sysop_alias_list			,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "ZoneBlind"				,cfg->zone_blind				,NULL);
	iniSetShortInt(&ini,	ROOT_SECTION, "ZoneBlindThreshold"		,cfg->zone_blind_threshold		,NULL);
	iniSetLogLevel(&ini,	ROOT_SECTION, "LogLevel"				,cfg->log_level					,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "CheckPathsForDupes"		,cfg->check_path				,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "StrictPacketPasswords"	,cfg->strict_packet_passwords	,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "SecureEchomail"			,cfg->secure_echomail			,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "EchomailNotify"			,cfg->echomail_notify			,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "StripLineFeeds"			,cfg->strip_lf					,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "ConvertTearLines"		,cfg->convert_tear				,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "FuzzyNetmailZones"		,cfg->fuzzy_zone				,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "BinkleyStyleOutbound"	,cfg->flo_mailer				,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "TruncateBundles"			,cfg->trunc_bundles				,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "AreaAddFromEcholistsOnly",cfg->add_from_echolists_only	,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "AutoAddSubs"				,cfg->auto_add_subs				,NULL);
	iniSetDuration(&ini,	ROOT_SECTION, "BsyTimeout"				,cfg->bsy_timeout				,NULL);
	iniSetDuration(&ini,	ROOT_SECTION, "BsoLockDelay"			,cfg->bso_lock_delay			,NULL);
	iniSetLongInt(&ini,		ROOT_SECTION, "BsoLockAttempts"			,cfg->bso_lock_attempts			,NULL);
	iniSetDuration(&ini,	ROOT_SECTION, "MaxEchomailAge"			,cfg->max_echomail_age			,NULL);
	iniSetDuration(&ini,	ROOT_SECTION, "MaxNetmailAge"			,cfg->max_netmail_age			,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "RelayFilteredMsgs"		,cfg->relay_filtered_msgs		,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "KillEmptyNetmail",		cfg->kill_empty_netmail			,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "DeleteNetmail",			cfg->delete_netmail				,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "DeletePackets",			cfg->delete_packets				,NULL);

	iniSetBool(&ini,		ROOT_SECTION, "IgnoreNetmailDestAddr"	,cfg->ignore_netmail_dest_addr	,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "IgnoreNetmailSentAttr"	,cfg->ignore_netmail_sent_attr	,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "IgnoreNetmailKillAttr"	,cfg->ignore_netmail_kill_attr	,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "IgnoreNetmailRecvAttr"	,cfg->ignore_netmail_recv_attr	,NULL);
	iniSetBool(&ini,		ROOT_SECTION, "IgnoreNetmailLocalAttr"	,cfg->ignore_netmail_local_attr	,NULL);
	iniSetString(&ini,		ROOT_SECTION, "DefaultRecipient"		,cfg->default_recipient			,NULL);

	iniSetBool(&ini,		ROOT_SECTION, "UseFTNDomains"			,cfg->use_ftn_domains			,NULL);

	style.key_prefix = "\t";

	/******************/
	/* Archive Types: */
	/******************/
	iniRemoveSections(&ini, "archive:");
	for(uint i=0; i<cfg->arcdefs; i++) {
		arcdef_t* arc = &cfg->arcdef[i];
		if(arc->name[0] == 0)
			continue;
		SAFEPRINTF(section,"archive:%s", arc->name);
		iniSetString(&ini,	section,	"Sig"			,arc->hexid		,&style);
		iniSetInteger(&ini,	section,	"SigOffset"		,arc->byteloc	,&style);
		iniSetString(&ini,	section,	"Pack"			,arc->pack		,&style);
		iniSetString(&ini,	section,	"Unpack"		,arc->unpack	,&style);
	}

	/****************/
	/* Links/Nodes: */
	/****************/
	iniRemoveSections(&ini, "node:");
	for(uint i=0; i<cfg->nodecfgs; i++) {
		nodecfg_t*	node = &cfg->nodecfg[i];
		SAFEPRINTF(section,"node:%s", faddrtoa(&node->addr));
		iniSetString(&ini	,section,	"Name"			,node->name			,&style);
		iniSetString(&ini	,section,	"Comment"		,node->comment		,&style);
		iniSetString(&ini	,section,	"Archive"		,node->archive == SBBSECHO_ARCHIVE_NONE ? "None" : node->archive->name, &style);
		iniSetEnum(&ini		,section,	"PacketType"	,pktTypeStringList, node->pkt_type, &style);
		iniSetString(&ini	,section,	"PacketPwd"		,node->pktpwd		,&style);
		iniSetBool(&ini		,section,	"AreaFix"		,node->areafix		,&style);
		iniSetString(&ini	,section,	"AreaFixPwd"	,node->password		,&style);
		iniSetString(&ini	,section,	"TicFilePwd"	,node->ticpwd		,&style);
		iniSetString(&ini	,section,	"Inbox"			,node->inbox		,&style);
		iniSetString(&ini	,section,	"Outbox"		,node->outbox		,&style);
		iniSetBool(&ini		,section,	"Passive"		,node->passive		,&style);
		iniSetBool(&ini		,section,	"Direct"		,node->direct		,&style);
		iniSetBool(&ini		,section,	"Notify"		,node->send_notify	,&style);
		iniSetStringList(&ini,section,	"Keys", ","		,node->keys			,&style);
		iniSetEnum(&ini		,section,	"Status"		,mailStatusStringList, node->status, &style);
		if(node->route.zone)
			iniSetString(&ini,section,	"Route"			,faddrtoa(&node->route), &style);
		else
			iniRemoveKey(&ini,section,	"Route");
		iniSetStringList(&ini, section, "GroupHub", ","	,node->grphub		,&style);
	}

	/**************/
	/* EchoLists: */
	/**************/
	iniRemoveSections(&ini, "echolist:");
	for(uint i=0; i<cfg->listcfgs; i++) {
		echolist_t* elist = &cfg->listcfg[i];
		if(elist->listpath[0] == 0)
			continue;
		SAFEPRINTF(section,"echolist:%s", elist->listpath);
		iniSetString(&ini	,section,	"Hub"		,faddrtoa(&elist->hub)				,&style);
		iniSetString(&ini	,section,	"AreaMgr"	,elist->areamgr						,&style);
		iniSetString(&ini	,section,	"Pwd"		,elist->password					,&style);
		iniSetBool(&ini		,section,	"Fwd"		,elist->forward						,&style);
		iniSetStringList(&ini,section,	"Keys", ","	,elist->keys						,&style);
	}

	iniWriteFile(fp, ini);
	iniCloseFile(fp);

	iniFreeStringList(ini);
	return true;
}
