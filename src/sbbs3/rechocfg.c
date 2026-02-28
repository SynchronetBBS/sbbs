/* Synchronet FidoNet EchoMail Scanning/Tossing and NetMail Tossing Utility */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
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

#include "sbbsecho.h"
#include "filewrap.h"   /* O_DENYNONE */
#include "nopen.h"      /* backup() */

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/* Supports wildcard of "ALL" in fields										*/
/****************************************************************************/
faddr_t atofaddr(const char *instr)
{
	char *  p, str[51];
	faddr_t addr;

	SAFECOPY(str, instr);
	p = str;
	FIND_WHITESPACE(p);
	*p = 0;
	if (!stricmp(str, "ALL")) {
		addr.zone = addr.net = addr.node = addr.point = 0xffff;
		return addr;
	}
	addr.zone = addr.net = addr.node = addr.point = 0;
	if ((p = strchr(str, ':')) != NULL) {
		if (!strnicmp(str, "ALL:", 4))
			addr.zone = 0xffff;
		else
			addr.zone = atoi(str);
		p++;
		if (!strnicmp(p, "ALL", 3))
			addr.net = 0xffff;
		else
			addr.net = atoi(p);
	}
	else {
		addr.zone = 1;
		addr.net = atoi(str);
	}
	if (!addr.zone)              /* no such thing as zone 0 */
		addr.zone = 1;
	if ((p = strchr(str, '/')) != NULL) {
		p++;
		if (!strnicmp(p, "ALL", 3))
			addr.node = 0xffff;
		else
			addr.node = atoi(p);
	}
	else {
		if (!addr.net)
			addr.net = 1;
		addr.node = atoi(str);
	}
	if ((p = strchr(str, '.')) != NULL) {
		p++;
		if (!strnicmp(p, "ALL", 3))
			addr.point = 0xffff;
		else
			addr.point = atoi(p);
	}
	return addr;
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/* Supports wildcard of "ALL" in fields										*/
/****************************************************************************/
const char *faddrtoa(const faddr_t* addr)
{
	static char str[25];
	char        tmp[25];

	str[0] = 0;
	if (addr->zone == 0xffff)
		strcpy(str, "ALL");
	else if (addr->zone) {
		sprintf(str, "%u:", addr->zone);
		if (addr->net == 0xffff)
			strcat(str, "ALL");
		else {
			sprintf(tmp, "%u/", addr->net);
			strcat(str, tmp);
			if (addr->node == 0xffff)
				strcat(str, "ALL");
			else {
				sprintf(tmp, "%u", addr->node);
				strcat(str, tmp);
				if (addr->point == 0xffff)
					strcat(str, ".ALL");
				else if (addr->point) {
					sprintf(tmp, ".%u", addr->point);
					strcat(str, tmp);
				}
			}
		}
	}
	return str;
}

bool faddr_contains_wildcard(const faddr_t* addr)
{
	return addr->zone == 0xffff || addr->net == 0xffff || addr->node == 0xffff || addr->point == 0xffff;
}

/******************************************************************************
 This function returns the number of the node in the SBBSECHO.CFG file which
 matches the address passed to it (or cfg.nodecfgs if no match).
 ******************************************************************************/
int matchnode(sbbsecho_cfg_t* cfg, faddr_t addr, int exact)
{
	uint i;

	if (exact != 2) {
		for (i = 0; i < cfg->nodecfgs; i++)                /* Look for exact match */
			if (!memcmp(&cfg->nodecfg[i].addr, &addr, sizeof(faddr_t)))
				break;
		if (exact || i < cfg->nodecfgs)
			return i;
	}

	for (i = 0; i < cfg->nodecfgs; i++)                    /* Look for point match */
		if (cfg->nodecfg[i].addr.point == 0xffff
		    && addr.zone == cfg->nodecfg[i].addr.zone
		    && addr.net == cfg->nodecfg[i].addr.net
		    && addr.node == cfg->nodecfg[i].addr.node)
			break;
	if (i < cfg->nodecfgs)
		return i;

	for (i = 0; i < cfg->nodecfgs; i++)                    /* Look for node match */
		if (cfg->nodecfg[i].addr.node == 0xffff
		    && addr.zone == cfg->nodecfg[i].addr.zone
		    && addr.net == cfg->nodecfg[i].addr.net)
			break;
	if (i < cfg->nodecfgs)
		return i;

	for (i = 0; i < cfg->nodecfgs; i++)                    /* Look for net match */
		if (cfg->nodecfg[i].addr.net == 0xffff
		    && addr.zone == cfg->nodecfg[i].addr.zone)
			break;
	if (i < cfg->nodecfgs)
		return i;

	for (i = 0; i < cfg->nodecfgs; i++)                    /* Look for total wild */
		if (cfg->nodecfg[i].addr.zone == 0xffff)
			break;
	return i;
}

nodecfg_t* findnodecfg(sbbsecho_cfg_t* cfg, faddr_t addr, int exact)
{
	uint node = matchnode(cfg, addr, exact);

	if (node < cfg->nodecfgs)
		return &cfg->nodecfg[node];

	return NULL;
}

void get_default_echocfg(sbbsecho_cfg_t* cfg)
{
	cfg->maxpktsize                 = DFLT_PKT_SIZE;
	cfg->maxbdlsize                 = DFLT_BDL_SIZE;
	cfg->badecho                    = -1;
	cfg->log_level                  = LOG_INFO;
	cfg->flo_mailer                 = true;
	cfg->check_path                 = true;
	cfg->zone_blind                 = false;
	cfg->zone_blind_threshold       = 0xffff;
	cfg->sysop_alias_list           = strListSplitCopy(NULL, "SYSOP", ",");
	cfg->max_echomail_age           = 60 * 24 * 60 * 60;
	cfg->bsy_timeout                = 12 * 60 * 60;
	cfg->bso_lock_attempts          = 60;
	cfg->bso_lock_delay             = 10;
	cfg->delete_packets             = true;
	cfg->delete_bad_packets         = false;
	cfg->verbose_bad_packet_names   = true;
	cfg->delete_netmail             = true;
	cfg->echomail_notify            = true;
	cfg->kill_empty_netmail         = true;
	cfg->strict_packet_passwords    = true;
	cfg->relay_filtered_msgs        = false;
	cfg->use_outboxes               = true;
	cfg->umask                      = 077;
	cfg->areafile_backups           = 100;
	cfg->cfgfile_backups            = 100;
	cfg->auto_add_subs              = true;
	cfg->auto_add_to_areafile       = true;
	cfg->auto_utf8                  = true;
	cfg->strip_soft_cr              = true;
	cfg->require_linked_node_cfg    = true;
	cfg->min_free_diskspace         = 10 * 1024 * 1024;
	cfg->max_logs_kept              = 10;
	cfg->max_log_size               = 10 * 1024 * 1024;
}

char* pktTypeStringList[] = {"2+", "2e", "2.2", "2", NULL};     // Must match enum pkt_type
char* mailStatusStringList[] = {"Normal", "Hold", "Crash", NULL};

bool sbbsecho_read_ini(sbbsecho_cfg_t* cfg)
{
	FILE*      fp;
	str_list_t ini;
	char       value[INI_MAX_VALUE_LEN];

	for (unsigned i = 0; i < cfg->domain_count; i++) {
		FREE_AND_NULL(cfg->domain_list[i].zone_list);
		cfg->domain_list[i].zone_count = 0;
	}

	get_default_echocfg(cfg);

	if ((fp = iniOpenFile(cfg->cfgfile, /* for_modify: */ false)) == NULL)
		return false;
	ini = iniReadFiles(fp, /* includes: */ true);
	iniCloseFile(fp);

	/************************/
	/* Global/root section: */
	/************************/
	iniFreeStringList(cfg->sysop_alias_list);
	SAFECOPY(cfg->inbound, iniGetString(ini, ROOT_SECTION, "Inbound", DEFAULT_INBOUND, value));
	SAFECOPY(cfg->secure_inbound, iniGetString(ini, ROOT_SECTION, "SecureInbound", DEFAULT_SECURE_INBOUND, value));
	SAFECOPY(cfg->outbound, iniGetString(ini, ROOT_SECTION, "Outbound", DEFAULT_OUTBOUND, value));
	SAFECOPY(cfg->areafile, iniGetString(ini, ROOT_SECTION, "AreaFile", DEFAULT_AREA_FILE, value));
	SAFECOPY(cfg->badareafile, iniGetString(ini, ROOT_SECTION, "BadAreaFile", DEFAULT_BAD_AREA_FILE, value));
	SAFECOPY(cfg->echostats, iniGetString(ini, ROOT_SECTION, "EchoStats", DEFAULT_ECHOSTATS_FILE, value));
	SAFECOPY(cfg->logfile, iniGetString(ini, ROOT_SECTION, "LogFile", DEFAULT_LOG_FILE, value));
	SAFECOPY(cfg->logtime, iniGetString(ini, ROOT_SECTION, "LogTimeFormat", DEFAULT_LOG_TIME_FMT, value));
	SAFECOPY(cfg->temp_dir, iniGetString(ini, ROOT_SECTION, "TempDirectory", DEFAULT_TEMP_DIR, value));
	SAFECOPY(cfg->outgoing_sem, iniGetString(ini, ROOT_SECTION, "OutgoingSemaphore",  "", value));
	cfg->log_level              = iniGetLogLevel(ini, ROOT_SECTION, "LogLevel", cfg->log_level);
	cfg->flo_mailer             = iniGetBool(ini, ROOT_SECTION, "BinkleyStyleOutbound", cfg->flo_mailer);
	cfg->bsy_timeout            = (ulong)iniGetDuration(ini, ROOT_SECTION, "BsyTimeout", cfg->bsy_timeout);
	cfg->bso_lock_attempts      = iniGetLongInt(ini, ROOT_SECTION, "BsoLockAttempts", cfg->bso_lock_attempts);
	cfg->bso_lock_delay         = (ulong)iniGetDuration(ini, ROOT_SECTION, "BsoLockDelay", cfg->bso_lock_delay);
	cfg->strict_packet_passwords = iniGetBool(ini, ROOT_SECTION, "StrictPacketPasswords", cfg->strict_packet_passwords);
	cfg->relay_filtered_msgs    = iniGetBool(ini, ROOT_SECTION, "RelayFilteredMsgs", cfg->relay_filtered_msgs);
	cfg->umask                  = iniGetInteger(ini, ROOT_SECTION, "umask", cfg->umask);
	cfg->areafile_backups       = iniGetInteger(ini, ROOT_SECTION, "AreaFileBackups", cfg->areafile_backups);
	cfg->cfgfile_backups        = iniGetInteger(ini, ROOT_SECTION, "CfgFileBackups", cfg->cfgfile_backups);
	cfg->max_logs_kept          = iniGetInteger(ini, ROOT_SECTION, "MaxLogsKept", cfg->max_logs_kept);
	cfg->max_log_size           = iniGetBytes(ini, ROOT_SECTION, "MaxLogSize", 1, cfg->max_log_size);
	cfg->min_free_diskspace     = iniGetBytes(ini, ROOT_SECTION, "MinFreeDiskSpace", 1, cfg->min_free_diskspace);
	cfg->strip_lf               = iniGetBool(ini, ROOT_SECTION, "StripLineFeeds", cfg->strip_lf);
	cfg->strip_soft_cr          = iniGetBool(ini, ROOT_SECTION, "StripSoftCRs", cfg->strip_soft_cr);
	cfg->use_outboxes           = iniGetBool(ini, ROOT_SECTION, "UseOutboxes", cfg->use_outboxes);
	cfg->auto_utf8              = iniGetBool(ini, ROOT_SECTION, "AutoUTF8", cfg->auto_utf8);
	cfg->sort_nodelist          = iniGetBool(ini, ROOT_SECTION, "SortNodeList", cfg->sort_nodelist);
	cfg->delete_packets         = iniGetBool(ini, ROOT_SECTION, "DeletePackets", cfg->delete_packets);
	cfg->delete_bad_packets     = iniGetBool(ini, ROOT_SECTION, "DeleteBadPackets", cfg->delete_bad_packets);
	cfg->verbose_bad_packet_names = iniGetBool(ini, ROOT_SECTION, "VerboseBadPacketNames", cfg->verbose_bad_packet_names);
	cfg->default_packet_type    = iniGetEnum(ini, ROOT_SECTION, "DefaultPacketType", pktTypeStringList, cfg->default_packet_type);

	/* EchoMail options: */
	cfg->maxbdlsize             = (ulong)iniGetBytes(ini, ROOT_SECTION, "BundleSize", 1, cfg->maxbdlsize);
	cfg->maxpktsize             = (ulong)iniGetBytes(ini, ROOT_SECTION, "PacketSize", 1, cfg->maxpktsize);
	cfg->check_path             = iniGetBool(ini, ROOT_SECTION, "CheckPathsForDupes", cfg->check_path);
	cfg->secure_echomail        = iniGetBool(ini, ROOT_SECTION, "SecureEchomail", cfg->secure_echomail);
	cfg->echomail_notify        = iniGetBool(ini, ROOT_SECTION, "EchomailNotify", cfg->echomail_notify);
	cfg->convert_tear           = iniGetBool(ini, ROOT_SECTION, "ConvertTearLines", cfg->convert_tear);
	cfg->trunc_bundles          = iniGetBool(ini, ROOT_SECTION, "TruncateBundles", cfg->trunc_bundles);
	cfg->zone_blind             = iniGetBool(ini, ROOT_SECTION, "ZoneBlind", cfg->zone_blind);
	cfg->zone_blind_threshold   = iniGetShortInt(ini, ROOT_SECTION, "ZoneBlindThreshold", cfg->zone_blind_threshold);
	cfg->add_from_echolists_only = iniGetBool(ini, ROOT_SECTION, "AreaAddFromEcholistsOnly", cfg->add_from_echolists_only);
	cfg->max_echomail_age       = (ulong)iniGetDuration(ini, ROOT_SECTION, "MaxEchomailAge", cfg->max_echomail_age);
	SAFECOPY(cfg->areamgr,        iniGetString(ini, ROOT_SECTION, "AreaManager", "SYSOP", value));
	cfg->auto_add_subs          = iniGetBool(ini, ROOT_SECTION, "AutoAddSubs", cfg->auto_add_subs);
	cfg->auto_add_to_areafile   = iniGetBool(ini, ROOT_SECTION, "AutoAddToAreaFile", cfg->auto_add_to_areafile);
	cfg->require_linked_node_cfg = iniGetBool(ini, ROOT_SECTION, "RequireLinkedNodeCfg", cfg->require_linked_node_cfg);

	/* NetMail options: */
	SAFECOPY(cfg->default_recipient, iniGetString(ini, ROOT_SECTION, "DefaultRecipient", "", value));
	cfg->sysop_alias_list           = iniGetStringList(ini, ROOT_SECTION, "SysopAliasList", ",", "SYSOP");
	cfg->fuzzy_zone                 = iniGetBool(ini, ROOT_SECTION, "FuzzyNetmailZones", cfg->fuzzy_zone);
	cfg->ignore_netmail_dest_addr   = iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailDestAddr", cfg->ignore_netmail_dest_addr);
	cfg->ignore_netmail_sent_attr   = iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailSentAttr", cfg->ignore_netmail_sent_attr);
	cfg->ignore_netmail_kill_attr   = iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailKillAttr", cfg->ignore_netmail_kill_attr);
	cfg->ignore_netmail_recv_attr   = iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailRecvAttr", cfg->ignore_netmail_recv_attr);
	cfg->ignore_netmail_local_attr  = iniGetBool(ini, ROOT_SECTION, "IgnoreNetmailLocalAttr", cfg->ignore_netmail_local_attr);
	cfg->ignore_packed_foreign_netmail = iniGetBool(ini, ROOT_SECTION, "IgnorePackedForeignNetmail", cfg->ignore_packed_foreign_netmail);
	cfg->kill_empty_netmail         = iniGetBool(ini, ROOT_SECTION, "KillEmptyNetmail", cfg->kill_empty_netmail);
	cfg->delete_netmail             = iniGetBool(ini, ROOT_SECTION, "DeleteNetmail", cfg->delete_netmail);
	cfg->max_netmail_age            = (ulong)iniGetDuration(ini, ROOT_SECTION, "MaxNetmailAge", cfg->max_netmail_age);

	/* BinkP options: */
	SAFECOPY(cfg->binkp_caps, iniGetString(ini, "BinkP", "Capabilities", "", value));
	SAFECOPY(cfg->binkp_sysop, iniGetString(ini, "BinkP", "Sysop", "", value));
	cfg->binkp_plainAuthOnly = iniGetBool(ini, "BinkP", "PlainAuthOnly", FALSE);
	cfg->binkp_plainTextOnly = iniGetBool(ini, "BinkP", "PlainTextOnly", FALSE);

	/******************/
	/* Archive Types: */
	/******************/
	str_list_t archivelist = iniGetSectionList(ini, "archive:");
	cfg->arcdefs = strListCount(archivelist);
	if ((cfg->arcdef = realloc_or_free(cfg->arcdef, sizeof(arcdef_t) * cfg->arcdefs)) == NULL) {
		strListFree(&archivelist);
		return false;
	}
	cfg->arcdefs = 0;
	char* archive;
	while ((archive = strListRemove(&archivelist, 0)) != NULL) {
		arcdef_t* arc = &cfg->arcdef[cfg->arcdefs++];
		memset(arc, 0, sizeof(arcdef_t));
		SAFECOPY(arc->name, truncsp(archive + 8));
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
	if (cfg->sort_nodelist)
		strListSortAlphaCase(nodelist);
	cfg->nodecfgs = strListCount(nodelist);
	if ((cfg->nodecfg = realloc_or_free(cfg->nodecfg, sizeof(nodecfg_t) * cfg->nodecfgs)) == NULL) {
		strListFree(&nodelist);
		return false;
	}
	cfg->nodecfgs = 0;
	char* node;
	while ((node = strListRemove(&nodelist, 0)) != NULL) {
		nodecfg_t* ncfg = &cfg->nodecfg[cfg->nodecfgs++];
		memset(ncfg, 0, sizeof(nodecfg_t));
		ncfg->addr = atofaddr(node + 5);
		char*      domain = strchr(node + 5, '@');
		if (domain != NULL)
			SAFECOPY(ncfg->domain, domain + 1);
		if (iniGetString(ini, node, "route", NULL, value) != NULL && value[0]) {
			fidoaddr_t addr = atofaddr(value);
			if (addr.zone != 0 && memcmp(&addr, &ncfg->addr, sizeof(addr)) != 0)
				ncfg->route = addr;
		}
		if (iniGetString(ini, node, "LocalAddress", NULL, value) != NULL && value[0]) {
			fidoaddr_t addr = atofaddr(value);
			if (addr.zone != 0 && memcmp(&addr, &ncfg->addr, sizeof(addr)) != 0)
				ncfg->local_addr = addr;
		}
		SAFECOPY(ncfg->password, iniGetString(ini, node, "AreaFixPwd", "", value));
		SAFECOPY(ncfg->pktpwd, iniGetString(ini, node, "PacketPwd", "", value));
		SAFECOPY(ncfg->sesspwd, iniGetString(ini, node, "SessionPwd", "", value));
		SAFECOPY(ncfg->ticpwd, iniGetString(ini, node, "TicFilePwd", "", value));
		SAFECOPY(ncfg->name, iniGetString(ini, node, "Name", "", value));
		SAFECOPY(ncfg->comment, iniGetString(ini, node, "Comment", "", value));
		if (!faddr_contains_wildcard(&ncfg->addr)) {
			SAFECOPY(ncfg->inbox, iniGetString(ini, node, "inbox", "", value));
			SAFECOPY(ncfg->outbox, iniGetString(ini, node, "outbox", "", value));
			char inbox[MAX_PATH + 1];
			char insec[MAX_PATH + 1];
			char inbound[MAX_PATH + 1];
			char outbox[MAX_PATH + 1];
			char outbound[MAX_PATH + 1];
			FULLPATH(inbox, ncfg->inbox, sizeof(inbox)), backslash(inbox);
			FULLPATH(outbox, ncfg->outbox, sizeof(outbox)), backslash(outbox);
			FULLPATH(insec, cfg->secure_inbound, sizeof(insec)), backslash(insec);
			FULLPATH(inbound, cfg->inbound, sizeof(inbound)), backslash(inbound);
			FULLPATH(outbound, cfg->outbound, sizeof(outbound)), backslash(outbound);
			if (stricmp(inbound, inbox) == 0 || stricmp(insec, inbox) == 0)
				ncfg->inbox[0] = 0;
			if (stricmp(outbound, outbox) == 0)
				ncfg->outbox[0] = 0;
		}
		ncfg->grphub            = iniGetStringList(ini, node, "GroupHub", ",", "");
		ncfg->keys              = iniGetStringList(ini, node, "keys", ",", "");
		ncfg->pkt_type          = iniGetEnum(ini, node, "PacketType", pktTypeStringList, ncfg->pkt_type);
		ncfg->areamgr           = iniGetBool(ini, node, "AreaFix", ncfg->password[0] ? true : false);
		ncfg->send_notify       = iniGetBool(ini, node, "notify", ncfg->send_notify);
		ncfg->passive           = iniGetBool(ini, node, "passive", ncfg->passive);
		ncfg->direct            = iniGetBool(ini, node, "direct", ncfg->direct);
		ncfg->status            = iniGetEnum(ini, node, "status", mailStatusStringList, ncfg->status);
		char* archive = iniGetString(ini, node, "archive", "", value);
		for (uint i = 0; i < cfg->arcdefs; i++) {
			if (stricmp(cfg->arcdef[i].name, archive) == 0) {
				ncfg->archive = &cfg->arcdef[i];
				break;
			}
		}
		/* BinkP settings */
		SAFECOPY(ncfg->binkp_src, iniGetString(ini, node, "BinkpSourceAddress", "", value));
		SAFECOPY(ncfg->binkp_host, iniGetString(ini, node, "BinkpHost", "", value));
		ncfg->binkp_port            = iniGetShortInt(ini, node, "BinkpPort", 24554);
		ncfg->binkp_poll            = iniGetBool(ini, node, "BinkpPoll", FALSE);
		ncfg->binkp_plainAuthOnly   = iniGetBool(ini, node, "BinkpPlainAuthOnly", FALSE);
		ncfg->binkp_allowPlainAuth  = iniGetBool(ini, node, "BinkpAllowPlainAuth", FALSE);
		ncfg->binkp_allowPlainText  = iniGetBool(ini, node, "BinkpAllowPlainText", TRUE);
		ncfg->binkp_tls             = iniGetBool(ini, node, "BinkpTLS", FALSE);
	}
	strListFree(&nodelist);

	/**************/
	/* EchoLists: */
	/**************/
	str_list_t echolists = iniGetSectionList(ini, "echolist:");
	cfg->listcfgs = strListCount(echolists);
	if ((cfg->listcfg = realloc_or_free(cfg->listcfg, sizeof(echolist_t) * cfg->listcfgs)) == NULL) {
		strListFree(&echolists);
		return false;
	}
	cfg->listcfgs = 0;
	char* echolist;
	while ((echolist = strListRemove(&echolists, 0)) != NULL) {
		echolist_t* elist = &cfg->listcfg[cfg->listcfgs++];
		memset(elist, 0, sizeof(echolist_t));
		SAFECOPY(elist->listpath, echolist + 9);
		elist->keys = iniGetStringList(ini, echolist, "keys", ",", "");
		if (iniGetString(ini, echolist, "hub", "", value) != NULL && value[0])
			elist->hub = atofaddr(value);
		elist->forward = iniGetBool(ini, echolist, "fwd", false);
		SAFECOPY(elist->password, iniGetString(ini, echolist, "pwd", "", value));
		SAFECOPY(elist->areamgr, iniGetString(ini, echolist, "AreaMgr", FIDO_AREAMGR_NAME, value));
	}
	strListFree(&echolists);

	/***********/
	/* Domains */
	/***********/
	str_list_t domains = iniGetSectionList(ini, "domain:");
	cfg->domain_count = strListCount(domains);
	if ((cfg->domain_list = realloc_or_free(cfg->domain_list, sizeof(struct fido_domain) * cfg->domain_count)) == NULL) {
		strListFree(&domains);
		return false;
	}
	cfg->domain_count = 0;
	char* domain;
	while ((domain = strListRemove(&domains, 0)) != NULL) {
		struct fido_domain* dom = &cfg->domain_list[cfg->domain_count++];
		memset(dom, 0, sizeof(*dom));
		dom->zone_list = iniGetIntList(ini, domain, "Zones", &dom->zone_count, ",", NULL);
		SAFECOPY(dom->name, domain + 7);
		SAFECOPY(dom->root, iniGetString(ini, domain, "OutboundRoot", cfg->outbound, value));
		SAFECOPY(dom->nodelist, iniGetString(ini, domain, "NodeList", "", value));
		SAFECOPY(dom->dns_suffix, iniGetString(ini, domain, "DNSSuffix", "", value));
	}
	strListFree(&domains);

	/**********/
	/* Robots */
	/**********/
	str_list_t robots = iniGetSectionList(ini, "robot:");
	cfg->robot_count = strListCount(robots);
	if ((cfg->robot_list = realloc_or_free(cfg->robot_list, sizeof(struct robot) * cfg->robot_count)) == NULL) {
		strListFree(&robots);
		return false;
	}
	cfg->robot_count = 0;
	char* robot;
	while ((robot = strListRemove(&robots, 0)) != NULL) {
		struct robot* bot = &cfg->robot_list[cfg->robot_count++];
		memset(bot, 0, sizeof(*bot));
		SAFECOPY(bot->name, robot + 6);
		SAFECOPY(bot->semfile, iniGetString(ini, robot, "SemFile", "", value));
		bot->attr = iniGetShortInt(ini, robot, "attr", 0);
		bot->uses_msg = iniGetBool(ini, robot, "UsesMsg", false);
	}
	strListFree(&robots);


	/* make sure we have some sane "maximum" size values here: */
	if (cfg->maxpktsize < 1024)
		cfg->maxpktsize = DFLT_PKT_SIZE;
	if (cfg->maxbdlsize < 1024)
		cfg->maxbdlsize = DFLT_BDL_SIZE;

	cfg->used_include = iniHasInclude(ini);
	strListFree(&ini);

	return true;
}

ini_style_t sbbsecho_ini_style = {  .value_separator = " = ", .section_separator = "" };

bool sbbsecho_write_ini(sbbsecho_cfg_t* cfg)
{
	char       section[128];
	FILE*      fp;
	str_list_t ini;

	if (cfg->cfgfile_backups)
		backup(cfg->cfgfile, cfg->cfgfile_backups, /* ren: */ false);

	if ((fp = iniOpenFile(cfg->cfgfile, /* for_modify: */ true)) == NULL)
		return false;
	ini = iniReadFile(fp);

	ini_style_t* style = &sbbsecho_ini_style;

	/************************/
	/* Global/root section: */
	/************************/
	iniSetString(&ini,      ROOT_SECTION, "Inbound", cfg->inbound, style);
	iniSetString(&ini,      ROOT_SECTION, "SecureInbound", cfg->secure_inbound, style);
	iniSetString(&ini,      ROOT_SECTION, "Outbound", cfg->outbound, style);
	iniSetString(&ini,      ROOT_SECTION, "AreaFile", cfg->areafile, style);
	iniSetInteger(&ini,     ROOT_SECTION, "AreaFileBackups", cfg->areafile_backups, style);
	iniSetInteger(&ini,     ROOT_SECTION, "CfgFileBackups", cfg->cfgfile_backups, style);
	iniSetInteger(&ini,     ROOT_SECTION, "MaxLogsKept", cfg->max_logs_kept, style);
	iniSetBytes(&ini,       ROOT_SECTION, "MaxLogSize", 1, cfg->max_log_size, style);
	iniSetBytes(&ini,       ROOT_SECTION, "MinFreeDiskSpace", 1, cfg->min_free_diskspace, style);
	iniSetString(&ini,      ROOT_SECTION, "BadAreaFile", cfg->badareafile, style);
	iniSetString(&ini,      ROOT_SECTION, "EchoStats", cfg->echostats, style);
	if (cfg->logfile[0])
		iniSetString(&ini,      ROOT_SECTION, "LogFile", cfg->logfile, style);
	if (cfg->logtime[0])
		iniSetString(&ini,      ROOT_SECTION, "LogTimeFormat", cfg->logtime, style);
	if (cfg->temp_dir[0])
		iniSetString(&ini,      ROOT_SECTION, "TempDirectory", cfg->temp_dir, style);
	iniSetString(&ini,      ROOT_SECTION, "OutgoingSemaphore", cfg->outgoing_sem, style);
	iniSetBytes(&ini,       ROOT_SECTION, "BundleSize", 1, cfg->maxbdlsize, style);
	iniSetBytes(&ini,       ROOT_SECTION, "PacketSize", 1, cfg->maxpktsize, style);
	iniSetStringList(&ini,  ROOT_SECTION, "SysopAliasList", ",", cfg->sysop_alias_list, style);
	iniSetBool(&ini,        ROOT_SECTION, "ZoneBlind", cfg->zone_blind, style);
	iniSetShortInt(&ini,    ROOT_SECTION, "ZoneBlindThreshold", cfg->zone_blind_threshold, style);
	iniSetLogLevel(&ini,    ROOT_SECTION, "LogLevel", cfg->log_level, style);
	iniSetBool(&ini,        ROOT_SECTION, "CheckPathsForDupes", cfg->check_path, style);
	iniSetBool(&ini,        ROOT_SECTION, "StrictPacketPasswords", cfg->strict_packet_passwords, style);
	iniSetBool(&ini,        ROOT_SECTION, "SecureEchomail", cfg->secure_echomail, style);
	iniSetBool(&ini,        ROOT_SECTION, "EchomailNotify", cfg->echomail_notify, style);
	iniSetBool(&ini,        ROOT_SECTION, "StripLineFeeds", cfg->strip_lf, style);
	iniSetBool(&ini,        ROOT_SECTION, "StripSoftCRs", cfg->strip_soft_cr, style);
	iniSetBool(&ini,        ROOT_SECTION, "UseOutboxes", cfg->use_outboxes, style);
	iniSetBool(&ini,        ROOT_SECTION, "AutoUTF8", cfg->auto_utf8, style);
	iniSetBool(&ini,        ROOT_SECTION, "SortNodeList", cfg->sort_nodelist, style);
	iniSetBool(&ini,        ROOT_SECTION, "ConvertTearLines", cfg->convert_tear, style);
	iniSetBool(&ini,        ROOT_SECTION, "FuzzyNetmailZones", cfg->fuzzy_zone, style);
	iniSetBool(&ini,        ROOT_SECTION, "BinkleyStyleOutbound", cfg->flo_mailer, style);
	iniSetBool(&ini,        ROOT_SECTION, "TruncateBundles", cfg->trunc_bundles, style);
	iniSetBool(&ini,        ROOT_SECTION, "AreaAddFromEcholistsOnly", cfg->add_from_echolists_only, style);
	iniSetBool(&ini,        ROOT_SECTION, "AutoAddSubs", cfg->auto_add_subs, style);
	iniSetBool(&ini,        ROOT_SECTION, "AutoAddToAreaFile", cfg->auto_add_to_areafile, style);
	iniSetBool(&ini,        ROOT_SECTION, "RequireLinkedNodeCfg", cfg->require_linked_node_cfg, style);
	iniSetDuration(&ini,    ROOT_SECTION, "BsyTimeout", cfg->bsy_timeout, style);
	iniSetDuration(&ini,    ROOT_SECTION, "BsoLockDelay", cfg->bso_lock_delay, style);
	iniSetLongInt(&ini,     ROOT_SECTION, "BsoLockAttempts", cfg->bso_lock_attempts, style);
	iniSetDuration(&ini,    ROOT_SECTION, "MaxEchomailAge", cfg->max_echomail_age, style);
	iniSetDuration(&ini,    ROOT_SECTION, "MaxNetmailAge", cfg->max_netmail_age, style);
	iniSetBool(&ini,        ROOT_SECTION, "RelayFilteredMsgs", cfg->relay_filtered_msgs, style);
	iniSetBool(&ini,        ROOT_SECTION, "KillEmptyNetmail",       cfg->kill_empty_netmail, style);
	iniSetBool(&ini,        ROOT_SECTION, "DeleteNetmail",          cfg->delete_netmail, style);
	iniSetBool(&ini,        ROOT_SECTION, "DeletePackets",          cfg->delete_packets, style);
	iniSetBool(&ini,        ROOT_SECTION, "DeleteBadPackets",       cfg->delete_bad_packets, style);
	iniSetBool(&ini,        ROOT_SECTION, "VerboseBadPacketNames",  cfg->verbose_bad_packet_names, style);

	iniSetBool(&ini,        ROOT_SECTION, "IgnoreNetmailDestAddr", cfg->ignore_netmail_dest_addr, style);
	iniSetBool(&ini,        ROOT_SECTION, "IgnoreNetmailSentAttr", cfg->ignore_netmail_sent_attr, style);
	iniSetBool(&ini,        ROOT_SECTION, "IgnoreNetmailKillAttr", cfg->ignore_netmail_kill_attr, style);
	iniSetBool(&ini,        ROOT_SECTION, "IgnoreNetmailRecvAttr", cfg->ignore_netmail_recv_attr, style);
	iniSetBool(&ini,        ROOT_SECTION, "IgnoreNetmailLocalAttr", cfg->ignore_netmail_local_attr, style);
	iniSetBool(&ini,        ROOT_SECTION, "IgnorePackedForeignNetmail", cfg->ignore_packed_foreign_netmail, style);
	iniSetString(&ini,      ROOT_SECTION, "DefaultRecipient", cfg->default_recipient, style);
	iniSetEnum(&ini,        ROOT_SECTION, "DefaultPacketType", pktTypeStringList, cfg->default_packet_type, style);

	/******************/
	/* BinkP Settings */
	/******************/
	iniSetString(&ini,      "BinkP",   "Capabilities", cfg->binkp_caps, style);
	iniSetString(&ini,      "BinkP",   "Sysop", cfg->binkp_sysop, style);
	iniSetBool(&ini,        "BinkP",   "PlainAuthOnly", cfg->binkp_plainAuthOnly, style);
	iniSetBool(&ini,        "BinkP",   "PlainTextOnly", cfg->binkp_plainTextOnly, style);

	/******************/
	/* Archive Types: */
	/******************/
	iniRemoveSections(&ini, "archive:");
	for (uint i = 0; i < cfg->arcdefs; i++) {
		arcdef_t* arc = &cfg->arcdef[i];
		if (arc->name[0] == 0)
			continue;
		SAFEPRINTF(section, "archive:%s", arc->name);
		iniSetString(&ini,  section,    "Sig", arc->hexid, style);
		iniSetInteger(&ini, section,    "SigOffset", arc->byteloc, style);
		iniSetString(&ini,  section,    "Pack", arc->pack, style);
		iniSetString(&ini,  section,    "Unpack", arc->unpack, style);
	}

	/****************/
	/* Links/Nodes: */
	/****************/
	iniRemoveSections(&ini, "node:");
	for (uint i = 0; i < cfg->nodecfgs; i++) {
		nodecfg_t* node = &cfg->nodecfg[i];
		SAFEPRINTF(section, "node:%s", faddrtoa(&node->addr));
		if (node->domain[0])
			sprintf(section + strlen(section), "@%s", node->domain);
		iniSetString(&ini, section,   "Name", node->name, style);
		iniSetString(&ini, section,   "Comment", node->comment, style);
		iniSetString(&ini, section,   "Archive", node->archive == SBBSECHO_ARCHIVE_NONE ? "None" : node->archive->name, style);
		iniSetEnum(&ini, section,   "PacketType", pktTypeStringList, node->pkt_type, style);
		iniSetString(&ini, section,   "PacketPwd", node->pktpwd, style);
		iniSetBool(&ini, section,   "AreaFix", node->areamgr, style);
		iniSetString(&ini, section,   "AreaFixPwd", node->password, style);
		iniSetString(&ini, section,   "SessionPwd", node->sesspwd, style);
		iniSetString(&ini, section,   "TicFilePwd", node->ticpwd, style);
		iniSetString(&ini, section,   "Inbox", node->inbox, style);
		iniSetString(&ini, section,   "Outbox", node->outbox, style);
		iniSetBool(&ini, section,   "Passive", node->passive, style);
		iniSetBool(&ini, section,   "Direct", node->direct, style);
		iniSetBool(&ini, section,   "Notify", node->send_notify, style);
		iniSetStringList(&ini, section,  "Keys", ",", node->keys, style);
		iniSetEnum(&ini, section,   "Status", mailStatusStringList, node->status, style);
		if (node->route.zone)
			iniSetString(&ini, section,  "Route", faddrtoa(&node->route), style);
		else
			iniRemoveKey(&ini, section,  "Route");
		if (node->local_addr.zone)
			iniSetString(&ini, section,  "LocalAddress", faddrtoa(&node->local_addr), style);
		else
			iniRemoveKey(&ini, section,  "LocalAddress");
		iniSetStringList(&ini, section, "GroupHub", ",", node->grphub, style);
		/* BinkP-related */
		iniSetString(&ini, section,   "BinkpHost", node->binkp_host, style);
		iniSetShortInt(&ini, section,   "BinkpPort", node->binkp_port, style);
		iniSetBool(&ini, section,   "BinkpPoll", node->binkp_poll, style);
		iniSetBool(&ini, section,   "BinkpPlainAuthOnly", node->binkp_plainAuthOnly, style);
		iniSetBool(&ini, section,   "BinkpAllowPlainAuth", node->binkp_allowPlainAuth, style);
		iniSetBool(&ini, section,   "BinkpAllowPlainText", node->binkp_allowPlainText, style);
		iniSetBool(&ini, section,   "BinkpTLS", node->binkp_tls, style);
		iniSetString(&ini, section,   "BinkpSourceAddress", node->binkp_src, style);
	}
	if (cfg->sort_nodelist)
		iniSortSections(&ini, "node:", /* sort keys: */ false);

	/**************/
	/* EchoLists: */
	/**************/
	iniRemoveSections(&ini, "echolist:");
	for (uint i = 0; i < cfg->listcfgs; i++) {
		echolist_t* elist = &cfg->listcfg[i];
		if (elist->listpath[0] == 0)
			continue;
		SAFEPRINTF(section, "echolist:%s", elist->listpath);
		iniSetString(&ini, section,   "Hub", faddrtoa(&elist->hub), style);
		iniSetString(&ini, section,   "AreaMgr", elist->areamgr, style);
		iniSetString(&ini, section,   "Pwd", elist->password, style);
		iniSetBool(&ini, section,   "Fwd", elist->forward, style);
		iniSetStringList(&ini, section,  "Keys", ",", elist->keys, style);
	}

	/* Domains */
	iniRemoveSections(&ini, "domain:");
	for (unsigned i = 0; i < cfg->domain_count; i++) {
		struct fido_domain* dom = &cfg->domain_list[i];
		if (dom->name[0] == 0)
			continue;
		SAFEPRINTF(section, "domain:%s", dom->name);
		iniSetIntList(&ini, section,    "Zones", ",",   dom->zone_list, dom->zone_count, style);
		iniSetString(&ini,  section,    "DNSSuffix",    dom->dns_suffix, style);
		if (strcmp(cfg->outbound, dom->root) != 0)
			iniSetString(&ini,  section,    "OutboundRoot", dom->root, style);
		iniSetString(&ini,  section,    "NodeList",     dom->nodelist, style);
	}

	/* Robots */
	iniRemoveSections(&ini, "robot:");
	for (unsigned i = 0; i < cfg->robot_count; i++) {
		struct robot* bot = &cfg->robot_list[i];
		if (bot->name[0] == 0)
			continue;
		SAFEPRINTF(section, "robot:%s", bot->name);
		iniSetString(&ini, section, "SemFile", bot->semfile, style);
		iniSetHexInt(&ini, section, "attr", bot->attr, style);
		iniSetBool(&ini, section, "UsesMSG", bot->uses_msg, style);
	}

	iniWriteFile(fp, ini);
	iniCloseFile(fp);

	iniFreeStringList(ini);
	return true;
}
