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

#include "scfg.h"
#include "ciolib.h" // CIO_KEY_*

void qhub_edit(int num);
char *daystr(char days);
void qhub_sub_edit(uint num);
BOOL import_qwk_conferences(uint num);

bool new_qhub(unsigned new_qhubnum)
{
	qhub_t* new_qhub = malloc(sizeof(qhub_t));
	if (new_qhub == NULL) {
		/* ToDo: report error */
		return false;
	}
	memset(new_qhub, 0, sizeof(*new_qhub));
	new_qhub->enabled = TRUE;

	qhub_t** new_qhub_list = realloc(cfg.qhub, sizeof(qhub_t*) * (cfg.total_qhubs + 1));
	if (new_qhub_list == NULL) {
		free(new_qhub);
		errormsg(WHERE, ERR_ALLOC, "qhub list", cfg.total_qhubs + 1);
		return false;
	}
	cfg.qhub = new_qhub_list;

	for (unsigned u = cfg.total_qhubs; u > new_qhubnum; u--)
		cfg.qhub[u] = cfg.qhub[u - 1];

	cfg.qhub[new_qhubnum] = new_qhub;
	cfg.total_qhubs++;
	return true;
}

bool new_qhub_sub(qhub_t* qhub, int subnum, sub_t* sub, unsigned confnum)
{
	if ((qhub->sub = realloc_or_free(qhub->sub, sizeof(*qhub->sub) * (qhub->subs + 1))) == NULL
	    || (qhub->conf = (ushort *)realloc_or_free(qhub->conf, sizeof(*qhub->conf) * (qhub->subs + 1))) == NULL
	    || (qhub->mode = (uchar *)realloc_or_free(qhub->mode, sizeof(*qhub->mode) * (qhub->subs + 1))) == NULL) {
		/* ToDo: report error */
		return false;
	}
	for (int u = qhub->subs; u > subnum; u--) {
		qhub->sub[u] = qhub->sub[u - 1];
		qhub->conf[u] = qhub->conf[u - 1];
		qhub->mode[u] = qhub->mode[u - 1];
	}
	qhub->sub[subnum] = sub;
	qhub->conf[subnum] = confnum;
	qhub->mode[subnum] = qhub->misc & QHUB_CTRL_A;
	qhub->subs++;
	return true;
}

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(char *str)
{
	char *  p;
	faddr_t addr;

	addr.zone = addr.net = addr.node = addr.point = 0;
	if ((p = strchr(str, ':')) != NULL) {
		addr.zone = atoi(str);
		addr.net = atoi(p + 1);
	}
	else {
		if (cfg.total_faddrs)
			addr.zone = cfg.faddr[0].zone;
		else
			addr.zone = 1;
		addr.net = atoi(str);
	}
	if (!addr.zone)              /* no such thing as zone 0 */
		addr.zone = 1;
	if ((p = strchr(str, '/')) != NULL)
		addr.node = atoi(p + 1);
	else {
		if (cfg.total_faddrs)
			addr.net = cfg.faddr[0].net;
		else
			addr.net = 1;
		addr.node = atoi(str);
	}
	if ((p = strchr(str, '.')) != NULL)
		addr.point = atoi(p + 1);
	return addr;
}

uint getgrp(char* title)
{
	static int grp_dflt, grp_bar;
	int        i;

	for (i = 0; i < cfg.total_grps && i < MAX_OPTS; i++)
		snprintf(opt[i], MAX_OPLN, "%-25s", cfg.grp[i]->lname);
	opt[i][0] = 0;
	return uifc.list(WIN_SAV | WIN_RHT | WIN_BOT, 0, 0, 0, &grp_dflt, &grp_bar, title, opt);
}

uint getsub(void)
{
	static int sub_dflt, sub_bar;
	char       str[81];
	int        i, j, k;
	uint       subnum[MAX_OPTS + 1] = {0};

	while (1) {
		i = getgrp("Select Message Group");
		if (i == -1)
			return -1;
		for (j = k = 0; j < cfg.total_subs && k < MAX_OPTS; j++)
			if (cfg.sub[j]->grp == i) {
				snprintf(opt[k], MAX_OPLN, "%-25s", cfg.sub[j]->lname);
				subnum[k++] = j;
			}
		opt[k][0] = 0;
		snprintf(str, sizeof str, "Select %s Sub-board", cfg.grp[i]->sname);
		j = uifc.list(WIN_RHT | WIN_BOT | WIN_SAV, 0, 0, 45, &sub_dflt, &sub_bar, str, opt);
		if (j < 0 || j >= cfg.total_subs || j >= MAX_OPTS)
			continue;
		sub_dflt++;
		sub_bar++;
		return subnum[j];
	}
}

/* These correlate with the LOG_* definitions in syslog.h/gen_defs.h */
static char* logLevelStringList[]
    = {"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Informational", "Debugging", NULL};

void mqtt_cfg()
{
	static int      dflt;
	struct mqtt_cfg saved_cfg = cfg.mqtt;
	char            str[128];
	static char*    mqttQOS[]
	    = { "0: At most once", "1: At least once", "2: Exactly once", NULL };
	static char*    mqttVersion[]
	    = { "3.1.0", "3.1.1", "5.0", NULL };
	static char*    mqttTlsMode[]
	    = { "Off", "Certificate", "Pre-Shared-Key", "Synchronet Broker", NULL };

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Enabled", cfg.mqtt.enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Broker Address", cfg.mqtt.broker_addr);
		snprintf(opt[i++], MAX_OPLN, "%-20s%u", "Broker Port", cfg.mqtt.broker_port);
		snprintf(opt[i++], MAX_OPLN, "%-20s%u seconds", "Keep-alive", cfg.mqtt.keepalive);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Publish Verbosity", cfg.mqtt.verbose ? "High" : "Low");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Publish QOS", mqttQOS[cfg.mqtt.publish_qos]);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Subscribe QOS", mqttQOS[cfg.mqtt.subscribe_qos]);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Log Level", logLevelStringList[cfg.mqtt.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "TLS (encryption)", mqttTlsMode[cfg.mqtt.tls.mode]);
		if (cfg.mqtt.tls.mode != MQTT_TLS_SBBS) {
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Username", cfg.mqtt.username);
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Password", cfg.mqtt.password);
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Protocol Version", mqttVersion[cfg.mqtt.protocol_version - 3]);
		}
		if (cfg.mqtt.tls.mode == MQTT_TLS_CERT) {
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "CA Cert", cfg.mqtt.tls.cafile);
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Client Cert", cfg.mqtt.tls.certfile);
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Key File", cfg.mqtt.tls.keyfile);
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Key File Password", cfg.mqtt.tls.keypass);
		} else if (cfg.mqtt.tls.mode == MQTT_TLS_PSK) {
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Key", cfg.mqtt.tls.psk);
			snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Identity", cfg.mqtt.tls.identity);
		}
		opt[i][0] = 0;
		uifc.helpbuf =
			"~ Message Queue Telemetry Transport (MQTT) ~\n"
			"\n"
			"Synchronet can publish status and log messages to an MQTT Broker\n"
			"for monitoring by MQTT clients. Synchronet will also subscribe to\n"
			"topics that may be used to control Synchronet servers and nodes from\n"
			"MQTT clients.\n"
			"\n"
			"For the MQTT Topic Naming and Payload Schemes used by Synchronet, see:\n"
			"`https://wiki.synchro.net/ref:mqtt`\n"
			"\n"
			"Note: An MQTT `Broker` is sometimes referred to as an MQTT `Server`.\n"
		;
		i = uifc.list(WIN_ACT | WIN_MID | WIN_SAV | WIN_ESC, 0, 0, 0, &dflt, 0
		              , "Message Queue Telemetry Transport", opt);
		if (i == -1) {
			uifc.changes = memcmp(&saved_cfg, &cfg.mqtt, sizeof(saved_cfg)) != 0;
			i = save_changes(WIN_MID | WIN_SAV);
			if (i == -1)
				continue;
			if (i == 0) {
				save_main_cfg(&cfg);
				refresh_cfg(&cfg);
			}
			return;
		}
		switch (i) {
			case 0:
				cfg.mqtt.enabled = !cfg.mqtt.enabled;
				break;
			case 1:
				uifc.helpbuf =
					"~ Broker Hostname or IP Address ~\n"
					"\n"
					"Enter the DNS name or IP address of the MQTT Broker.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Broker Hostname or IP Address"
				           , cfg.mqtt.broker_addr, sizeof(cfg.mqtt.broker_addr) - 1, K_EDIT);
				break;
			case 2:
				uifc.helpbuf =
					"~ Broker TCP Port Number ~\n"
					"\n"
					"Enter the TCP port number (0-65535) used by the MQTT Broker.\n"
					"\n"
					"Default: `1883` for unencrypted MQTT, `8883` for encrypted MQTT over TLS"
				;
				SAFEPRINTF(str, "%hu", cfg.mqtt.broker_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Broker TCP Port Number"
				               , str, 5, K_EDIT | K_NUMBER) > 0)
					cfg.mqtt.broker_port = atoi(str);
				break;
			case 3:
				SAFEPRINTF(str, "%u", cfg.mqtt.keepalive);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Seconds to keep inactive connection alive"
				               , str, 5, K_EDIT | K_NUMBER) > 0 && atoi(str) >= 5)
					cfg.mqtt.keepalive = atoi(str);
				break;
			case 4:
				cfg.mqtt.verbose = !cfg.mqtt.verbose;
				break;
			case 5:
				i = cfg.mqtt.publish_qos;
				if ((i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Quality of Service for Publishing", mqttQOS)) >= 0)
					cfg.mqtt.publish_qos = i;
				break;
			case 6:
				i = cfg.mqtt.subscribe_qos;
				if ((i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Quality of Service for Subscriptions", mqttQOS)) >= 0)
					cfg.mqtt.subscribe_qos = i;
				break;
			case 7:
				uifc.helpbuf =
					"~ MQTT Log Level ~\n"
					"\n"
					"Select the minimum severity of log messages to be published to the MQTT\n"
					"Broker.\n"
					"\n"
					"The default/normal setting is `Informational`.";
				i = cfg.mqtt.log_level;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Log Level", logLevelStringList);
				if (i >= 0 && i <= LOG_DEBUG)
					cfg.mqtt.log_level = i;
				break;
			case 8:
				uifc.helpbuf =
					"~ Encryption via TLS ~\n"
					"\n"
					"MQTT traffic may be encrypted via TLS using one of:\n"
					"\n"
					"  - `Certificate-based` authentication (mutual-auth optionally supported)\n"
					"  - `Pre-Shared-Key` authentication\n"
					"  - `Synchronet Broker` use internal Synchronet authentication\n"
					"\n"
					"When TLS is used, the default MQTT port is `8883`.\n"
				;
				i = cfg.mqtt.tls.mode;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Encryption via TLS", mqttTlsMode);
				if (i >= 0)
					cfg.mqtt.tls.mode = i;
				break;
			case 9:
				uifc.helpbuf =
					"~ User name ~\n"
					"\n"
					"Enter the user name to use for authenticating with the MQTT Broker.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "User name for authentication"
				           , cfg.mqtt.username, sizeof(cfg.mqtt.username) - 1, K_EDIT);
				break;
			case 10:
				uifc.helpbuf =
					"~ Password ~\n"
					"\n"
					"Enter the user password to use for authenticating with the MQTT Broker.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Password for authentication"
				           , cfg.mqtt.password, sizeof(cfg.mqtt.password) - 1, K_EDIT);
				break;
			case 11:
				i = cfg.mqtt.protocol_version - 3;
				if ((i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Protocol Version", mqttVersion)) >= 0)
					cfg.mqtt.protocol_version = 3 + i;
				break;
			case 12:
				if (cfg.mqtt.tls.mode == MQTT_TLS_CERT) {
					uifc.helpbuf =
						"~ CA Certificate File ~\n"
						"\n"
						"Path to a file containing the PEM encoded trusted Certificate Authority\n"
						"(CA) certificate."
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "CA Certificate File"
					           , cfg.mqtt.tls.cafile, sizeof(cfg.mqtt.tls.cafile) - 1, K_EDIT);
				} else {
					uifc.helpbuf =
						"~ Pre-Shared-Key ~\n"
						"\n"
						"The pre-shared-key in hexadecimal format with no leading '0x'.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Pre-Shared-Key (in hex)"
					           , cfg.mqtt.tls.psk, sizeof(cfg.mqtt.tls.psk) - 1, K_EDIT);
				}
				break;
			case 13:
				if (cfg.mqtt.tls.mode == MQTT_TLS_CERT) {
					uifc.helpbuf =
						"~ Client Certificate File ~\n"
						"\n"
						"Path to file containing the PEM encoded certificate for this client.\n"
						"\n"
						"A client certificate and private key file are only needed for brokers\n"
						"that perform client certificate authentication (mutual authentication)."
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Client Certificate File"
					           , cfg.mqtt.tls.certfile, sizeof(cfg.mqtt.tls.certfile) - 1, K_EDIT);
				} else {
					uifc.helpbuf =
						"~ Client Identity ~\n"
						"\n"
						"The identity (e.g. name) of this client.\n"
						"\n"
						"May be used as the username depending on the broker settings.\n"
					;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Client Identity"
					           , cfg.mqtt.tls.identity, sizeof(cfg.mqtt.tls.identity) - 1, K_EDIT);
				}
				break;
			case 14:
				uifc.helpbuf =
					"~ Private Key File ~\n"
					"\n"
					"Path to a file containing the PEM encoded private key for this client.\n"
					"\n"
					"A client certificate and private key file are only needed for brokers\n"
					"that perform client certificate authentication (mutual authentication)."
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Private Key File"
				           , cfg.mqtt.tls.keyfile, sizeof(cfg.mqtt.tls.keyfile) - 1, K_EDIT);
				break;
			case 15:
				uifc.helpbuf =
					"~ Private Key File Password ~\n"
					"\n"
					"If the `Private Key File` is encrypted, the password to decrypt it\n"
					"must be entered here.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Private Key File Password"
				           , cfg.mqtt.tls.keypass, sizeof(cfg.mqtt.tls.keypass) - 1, K_EDIT);
				break;

		}
	}
}

void net_cfg()
{
	static int net_dflt, qnet_dflt, fnet_dflt, inet_dflt
	, qhub_dflt;
	char       str[81], done;
	int        i, j, k, l;
	int        mode;

	while (1) {
		*cfg.filename = '\0';
		i = 0;
		strcpy(opt[i++], "Internet E-mail");
		strcpy(opt[i++], "QWK Packet Networks");
		strcpy(opt[i++], "FidoNet EchoMail and NetMail");
		strcpy(opt[i++], "MQTT");
		opt[i][0] = 0;
		uifc.helpbuf =
			"`Network Configuration:`\n"
			"\n"
			"Select the type of network technology that you want to configure.\n"
		;
		i = uifc.list(WIN_ORG | WIN_ACT | WIN_CHE, 0, 0, 0, &net_dflt, 0, "Network Configuration", opt);
		if (i < 0) // ESC
			break;
		uifc.changes = FALSE;
		if (!load_main_cfg(&cfg, error, sizeof(error))) {
			uifc.msgf("ERROR: %s", error);
			break;
		}
		if (i == 3) { // MQTT
			mqtt_cfg();
			free_main_cfg(&cfg);
			continue;
		}
		if (!load_msgs_cfg(&cfg, error, sizeof(error))) {
			uifc.msgf("ERROR: %s", error);
			break;
		}
		if (i == 1) {  /* QWK net stuff */
			done = 0;
			while (!done) {
				i = 0;
				strcpy(opt[i++], "Network Hubs...");
				strcpy(opt[i++], "Default Tagline");
				opt[i][0] = 0;
				uifc.helpbuf =
					"`QWK Packet Networks:`\n"
					"\n"
					"From this menu you can configure the default tagline to use for\n"
					"outgoing messages on QWK networked sub-boards, or you can select\n"
					"`Network Hubs...` to add, delete, or configure QWK hubs that your system\n"
					"calls to exchange packets with.\n"
				;
				i = uifc.list(WIN_ACT | WIN_RHT | WIN_BOT | WIN_CHE, 0, 0, 0, &qnet_dflt, 0
				              , "QWK Packet Networks", opt);
				switch (i) {
					case -1:    /* ESC */
						i = save_changes(WIN_MID | WIN_SAV);
						if (i == -1)
							continue;
						done = 1;
						if (i == 0) {
							save_msgs_cfg(&cfg);
							refresh_cfg(&cfg);
						}
						break;
					case 1:
						uifc.helpbuf =
							"`QWK Network Default Tagline:`\n"
							"\n"
							"This is the default tagline to use for outgoing messages on QWK\n"
							"networked sub-boards. This default can be overridden on a per sub-board\n"
							"basis with the sub-board configuration `Network Options...`.\n"
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, ""
						           , cfg.qnet_tagline, sizeof(cfg.qnet_tagline) - 1, K_MSG | K_EDIT);
						break;
					case 0:
						while (1) {
							for (i = 0; i < cfg.total_qhubs && i < MAX_OPTS; i++)
								snprintf(opt[i], MAX_OPLN, "%-8.8s  %s"
								         , cfg.qhub[i]->id
								         , cfg.qhub[i]->enabled ? cfg.qhub[i]->call : "<DISABLED>");
							opt[i][0] = 0;
							i = WIN_ACT | WIN_RHT | WIN_SAV;
							if (cfg.total_qhubs < MAX_OPTS)
								i |= WIN_INS | WIN_INSACT | WIN_XTR;
							if (cfg.total_qhubs)
								i |= WIN_DEL;
							uifc.helpbuf =
								"`QWK Network Hubs:`\n"
								"\n"
								"This is a list of QWK network hubs that your system calls to exchange\n"
								"message packets with.\n"
								"\n"
								"To add a hub, select the desired location with the arrow keys and hit\n"
								"~ INS ~.\n"
								"\n"
								"To delete a hub, select it and hit ~ DEL ~.\n"
								"\n"
								"To configure a hub, select it and hit ~ ENTER ~.\n"
							;
							i = uifc.list(i, 0, 0, 0, &qhub_dflt, 0
							              , "QWK Network Hubs", opt);
							if (i == -1)
								break;
							int msk = i & MSK_ON;
							i &= MSK_OFF;
							if (msk == MSK_INS) {
								uifc.helpbuf =
									"`QWK Network Hub System ID:`\n"
									"\n"
									"This is the QWK System ID of this hub. It is used for incoming and\n"
									"outgoing network message packets and must be accurate.\n"
								;
								if (uifc.input(WIN_MID | WIN_SAV, 0, 0
								               , "System ID", str, LEN_QWKID, K_UPPER) < 1)
									continue;
								if (!new_qhub(i))
									continue;
								SAFECOPY(cfg.qhub[i]->id, str);
								SAFECOPY(cfg.qhub[i]->fmt, "ZIP");
								SAFECOPY(cfg.qhub[i]->call, "*qnet-ftp %s hub.address YOURPASS");
								cfg.qhub[i]->node = NODE_ANY;
								cfg.qhub[i]->days = 0x7f; /* all days */
								uifc.changes = TRUE;
								continue;
							}
							if (msk == MSK_DEL) {
								free(cfg.qhub[i]->mode);
								free(cfg.qhub[i]->conf);
								free(cfg.qhub[i]->sub);
								free(cfg.qhub[i]);
								cfg.total_qhubs--;
								while (i < cfg.total_qhubs) {
									cfg.qhub[i] = cfg.qhub[i + 1];
									i++;
								}
								uifc.changes = TRUE;
								continue;
							}
							qhub_edit(i);
						}
						break;
				}
			}
		}

		else if (i == 2) {     /* FidoNet Stuff */
			static faddr_t savfaddr;
			done = 0;
			while (!done) {
				i = 0;
				tmp[0] = 0;
				for (j = 0; j < cfg.total_faddrs && strlen(tmp) < 24; j++)
					sprintf(tmp + strlen(tmp), "%s%s", j ? ", " : "", smb_faddrtoa(&cfg.faddr[j], NULL));
				if (j < cfg.total_faddrs)
					strcat(tmp, ", ...");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "System Addresses", tmp);
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "Default Origin Line", cfg.origline);
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "NetMail Semaphore", cfg.netmail_sem);
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "EchoMail Semaphore", cfg.echomail_sem);
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "NetMail Directory", cfg.netmail_dir);
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "Allow Sending of NetMail"
				         , cfg.netmail_misc & NMAIL_ALLOW ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "Allow File Attachments"
				         , cfg.netmail_misc & NMAIL_FILE ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "Send NetMail Using Alias"
				         , cfg.netmail_misc & NMAIL_ALIAS ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "NetMail Defaults to Crash"
				         , cfg.netmail_misc & NMAIL_CRASH ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "NetMail Defaults to Direct"
				         , cfg.netmail_misc & NMAIL_DIRECT ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "NetMail Defaults to Hold"
				         , cfg.netmail_misc & NMAIL_HOLD ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
				         , "Kill NetMail After Sent"
				         , cfg.netmail_misc & NMAIL_KILL ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-33.33s%" PRIu32
				         , "Cost to Send NetMail", cfg.netmail_cost);
				if (cfg.total_faddrs > 1)
					snprintf(opt[i++], MAX_OPLN, "%-33.33s%s"
					         , "Choose NetMail Source Address"
					         , cfg.netmail_misc & NMAIL_CHSRCADDR ? "Yes":"No");
				opt[i][0] = 0;
				uifc.helpbuf =
					"`FidoNet EchoMail and NetMail:`\n"
					"\n"
					"This menu contains configuration options that pertain specifically to\n"
					"networking E-mail (NetMail) and sub-boards (EchoMail) through networks\n"
					"using FidoNet technology.\n"
				;
				i = uifc.list(WIN_ACT | WIN_MID | WIN_CHE, 0, 0, 68, &fnet_dflt, 0
				              , "FidoNet EchoMail and NetMail", opt);
				switch (i) {
					case -1:    /* ESC */
						i = save_changes(WIN_MID | WIN_SAV);
						if (i == -1)
							continue;
						done = 1;
						if (i == 0) {
							save_msgs_cfg(&cfg);
							refresh_cfg(&cfg);
						}
						break;
					case 0:
						uifc.helpbuf =
							"`System FidoNet Addresses:`\n"
							"\n"
							"These are the FidoNet-style addresses of your system, used to receive\n"
							"FidoNet-style NetMail and EchoMail over FidoNet Technology Networks.\n"
							"\n"
							"The `Main` address is also used as the default address for Fido-Networked\n"
							"sub-boards (EchoMail areas).\n"
							"\n"
							"The supported address format (so-called 3D or 4D): `Zone`:`Net`/`Node`[.`Point`]\n"
						;
						k = l = 0;
						while (1) {
							for (i = 0; i < cfg.total_faddrs && i < MAX_OPTS; i++) {
								if (i == 0)
									strcpy(str, "Main");
								else
									sprintf(str, "AKA %u", i);
								snprintf(opt[i], MAX_OPLN, "%-8.8s %16s"
								         , str, smb_faddrtoa(&cfg.faddr[i], tmp));
							}
							opt[i][0] = 0;
							mode = WIN_RHT | WIN_SAV | WIN_ACT | WIN_INSACT;
							if (cfg.total_faddrs < MAX_OPTS)
								mode |= WIN_INS | WIN_XTR;
							if (cfg.total_faddrs)
								mode |= WIN_DEL | WIN_COPY | WIN_CUT;
							if (savfaddr.zone)
								mode |= WIN_PASTE | WIN_PASTEXTR;
							i = uifc.list(mode, 0, 0, 0, &k, &l
							              , "System Addresses", opt);
							if (i == -1)
								break;
							int msk = i & MSK_ON;
							i &= MSK_OFF;
							if (msk == MSK_INS || msk == MSK_PASTE) {
								faddr_t newfaddr;
								if (msk == MSK_INS) {
									if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Address (e.g. 1:2/3 or 1:2/3.4)"
									               , str, 25, K_UPPER) < 1)
										continue;
									newfaddr = atofaddr(str);
								} else
									newfaddr = savfaddr;

								if ((cfg.faddr = (faddr_t *)realloc_or_free(cfg.faddr
								                                            , sizeof(faddr_t) * (cfg.total_faddrs + 1))) == NULL) {
									errormsg(WHERE, ERR_ALLOC, nulstr
									         , sizeof(faddr_t) * cfg.total_faddrs + 1);
									cfg.total_faddrs = 0;
									bail(1);
									continue;
								}

								for (j = cfg.total_faddrs; j > i; j--)
									cfg.faddr[j] = cfg.faddr[j - 1];

								cfg.faddr[i] = newfaddr;
								cfg.total_faddrs++;
								uifc.changes = TRUE;
								continue;
							}
							if (msk == MSK_COPY) {
								savfaddr = cfg.faddr[i];
								continue;
							}
							if (msk == MSK_DEL || msk == MSK_CUT) {
								if (msk == MSK_CUT)
									savfaddr = cfg.faddr[i];
								cfg.total_faddrs--;
								while (i < cfg.total_faddrs) {
									cfg.faddr[i] = cfg.faddr[i + 1];
									i++;
								}
								uifc.changes = TRUE;
								continue;
							}
							smb_faddrtoa(&cfg.faddr[i], str);
							if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Address"
							               , str, 25, K_EDIT | K_UPPER) >= 1)
								cfg.faddr[i] = atofaddr(str);
						}
						break;
					case 1:
						uifc.helpbuf =
							"`Default Origin Line:`\n"
							"\n"
							"This is the default origin line used for sub-boards networked via\n"
							"EchoMail. This origin line can be overridden on a per sub-board basis\n"
							"with the sub-board configuration `Network Options...`.\n"
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, "* Origin"
						           , cfg.origline, sizeof(cfg.origline) - 1, K_EDIT);
						break;
					case 2:
						uifc.helpbuf =
							"`NetMail Semaphore File:`\n"
							"\n"
							"This is a filename that will be used as a semaphore (signal) to your\n"
							"FidoNet front-end that new NetMail has been created and the messages\n"
							"should be re-scanned.\n"
							"\n"
							"`Command line specifiers may be included in the semaphore filename.`\n"
							SCFG_CMDLINE_SPEC_HELP
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, "NetMail Semaphore"
						           , cfg.netmail_sem, sizeof(cfg.netmail_sem) - 1, K_EDIT);
						break;
					case 3:
						uifc.helpbuf =
							"`EchoMail Semaphore File:`\n"
							"\n"
							"This is a filename that will be used as a semaphore (signal) to your\n"
							"FidoNet front-end that new EchoMail has been created and the messages\n"
							"should be re-scanned.\n"
							"\n"
							"`Command line specifiers may be included in the semaphore filename.`\n"
							SCFG_CMDLINE_SPEC_HELP
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, "EchoMail Semaphore"
						           , cfg.echomail_sem, sizeof(cfg.echomail_sem) - 1, K_EDIT);
						break;
					case 4:
						uifc.helpbuf =
							"`NetMail Directory:`\n"
							"\n"
							"This is the directory where FidoNet NetMail will be imported from\n"
							"and exported to (in FTS-1, *.MSG format).\n"
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, "NetMail"
						           , cfg.netmail_dir, sizeof(cfg.netmail_dir) - 1, K_EDIT);
						break;
					case 5:
						i = (cfg.netmail_misc & NMAIL_ALLOW) ? 0 : 1;
						uifc.helpbuf =
							"`Allow Users to Send NetMail:`\n"
							"\n"
							"If you are on a FidoNet style network and want your users to be allowed\n"
							"to send FidoNet NetMail, set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Allow Users to Send NetMail", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_ALLOW)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_ALLOW;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_ALLOW) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_ALLOW;
						}
						break;
					case 6:
						i = (cfg.netmail_misc & NMAIL_FILE) ? 0 : 1;
						uifc.helpbuf =
							"`Allow Users to Send NetMail File Attachments:`\n"
							"\n"
							"If you are on a FidoNet style network and want your users to be allowed\n"
							"to send NetMail file attachments, set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Allow Users to Send NetMail File Attachments", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_FILE)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_FILE;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_FILE) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_FILE;
						}
						break;
					case 7:
						i = (cfg.netmail_misc & NMAIL_ALIAS) ? 0 : 1;
						uifc.helpbuf =
							"`Use Aliases in NetMail:`\n"
							"\n"
							"If you allow aliases on your system and wish users to have their NetMail\n"
							"contain their alias as the `From User`, set this option to `Yes`. If you\n"
							"want all NetMail to be sent using users' real names, set this option to\n"
							"`No`.\n"
							"\n"
							"Users with the '`O`' restriction flag will always send netmail using\n"
							"their alias (their real name is a duplicate of another user account)."
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Use Aliases in NetMail", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_ALIAS)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_ALIAS;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_ALIAS) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_ALIAS;
						}
						break;
					case 8:
						i = (cfg.netmail_misc & NMAIL_CRASH) ? 0 : 1;
						uifc.helpbuf =
							"`NetMail Defaults to Crash Status:`\n"
							"\n"
							"If you want all NetMail to default to crash (send immediately) status,\n"
							"set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "NetMail Defaults to Crash Status", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_CRASH)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_CRASH;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_CRASH) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_CRASH;
						}
						break;
					case 9:
						i = (cfg.netmail_misc & NMAIL_DIRECT) ? 0 : 1;
						uifc.helpbuf =
							"`NetMail Defaults to Direct Status:`\n"
							"\n"
							"If you want all NetMail to default to direct (send directly) status,\n"
							"set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "NetMail Defaults to Direct Status", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_DIRECT)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_DIRECT;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_DIRECT) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_DIRECT;
						}
						break;
					case 10:
						i = (cfg.netmail_misc & NMAIL_HOLD) ? 0 : 1;
						uifc.helpbuf =
							"`NetMail Defaults to Hold Status:`\n"
							"\n"
							"If you want all NetMail to default to hold status, set this option to\n"
							"`Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "NetMail Defaults to Hold Status", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_HOLD)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_HOLD;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_HOLD) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_HOLD;
						}
						break;
					case 11:
						i = (cfg.netmail_misc & NMAIL_KILL) ? 0 : 1;
						uifc.helpbuf =
							"`Kill NetMail After it is Sent:`\n"
							"\n"
							"If you want NetMail messages to be deleted after they are successfully\n"
							"sent, set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Kill NetMail After it is Sent", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_KILL)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_KILL;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_KILL) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_KILL;
						}
						break;
					case 12:
						ultoa(cfg.netmail_cost, str, 10);
						uifc.helpbuf =
							"`Cost in Credits to Send NetMail:`\n"
							"\n"
							"This is the number of credits it will cost your users to send NetMail.\n"
							"If you want the sending of NetMail to be free, set this value to `0`.\n"
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0
						           , "Cost in Credits to Send NetMail"
						           , str, 10, K_EDIT | K_NUMBER);
						cfg.netmail_cost = atol(str);
						break;
					case 13:
						i = (cfg.netmail_misc & NMAIL_CHSRCADDR) ? 0 : 1;
						uifc.helpbuf =
							"`Choose NetMail Source Address:`\n"
							"\n"
							"When the system has multiple FidoNet-style addresses, you can allow\n"
							"users to choose the source address when sending NetMail messages by\n"
							"setting this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Allow Senders of NetMail to Choose the Source Address", uifcYesNoOpts);
						if (!i && !(cfg.netmail_misc & NMAIL_CHSRCADDR)) {
							uifc.changes = TRUE;
							cfg.netmail_misc |= NMAIL_CHSRCADDR;
						}
						else if (i == 1 && cfg.netmail_misc & NMAIL_CHSRCADDR) {
							uifc.changes = TRUE;
							cfg.netmail_misc &= ~NMAIL_CHSRCADDR;
						}
						break;
				}
			}
		}
		else if (i == 0) {     /* Internet E-mail */
			done = 0;
			while (!done) {
				i = 0;
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "System Address", cfg.sys_inetaddr);
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "Inbound E-mail Semaphore", cfg.smtpmail_sem);
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "Outbound E-mail Semaphore", cfg.inetmail_sem);
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "Allow Sending of E-mail"
				         , cfg.inetmail_misc & NMAIL_ALLOW ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "Allow File Attachments"
				         , cfg.inetmail_misc & NMAIL_FILE ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "Send E-mail Using Alias"
				         , cfg.inetmail_misc & NMAIL_ALIAS ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%s"
				         , "Kill E-mail After Sent"
				         , cfg.inetmail_misc & NMAIL_KILL ? "Yes":"No");
				snprintf(opt[i++], MAX_OPLN, "%-27.27s%" PRIu32
				         , "Cost to Send E-mail", cfg.inetmail_cost);
				opt[i][0] = 0;
				uifc.helpbuf =
					"`Internet E-mail:`\n"
					"\n"
					"This menu contains configuration options that pertain specifically to\n"
					"Internet E-mail.\n"
				;
				i = uifc.list(WIN_ACT | WIN_MID | WIN_CHE, 0, 0, 60, &inet_dflt, 0
				              , "Internet E-mail", opt);
				switch (i) {
					case -1:    /* ESC */
						i = save_changes(WIN_MID | WIN_SAV);
						if (i == -1)
							continue;
						done = 1;
						if (i == 0) {
							save_msgs_cfg(&cfg);
							refresh_cfg(&cfg);
						}
						break;
					case 0:
						edit_sys_inetaddr(false, false);
						break;
					case 1:
						uifc.helpbuf =
							"`Inbound Internet E-mail Semaphore File:`\n"
							"\n"
							"This is a filename that will be used as a semaphore (signal) to any\n"
							"external Internet e-mail processors that new mail has been received\n"
							"and the message base should be re-scanned.\n"
							"\n"
							"`Command line specifiers may be included in the semaphore filename.`\n"
							SCFG_CMDLINE_SPEC_HELP
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, "Inbound Semaphore"
						           , cfg.smtpmail_sem, sizeof(cfg.smtpmail_sem) - 1, K_EDIT);
						break;
					case 2:
						uifc.helpbuf =
							"`Outbound Internet E-mail Semaphore File:`\n"
							"\n"
							"This is a filename that will be used as a semaphore (signal) to any\n"
							"external Internet gateways (if supported) that new mail has been created\n"
							"and the message base should be re-scanned.\n"
							"\n"
							"`Command line specifiers may be included in the semaphore filename.`\n"
							SCFG_CMDLINE_SPEC_HELP
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0, "Outbound Semaphore"
						           , cfg.inetmail_sem, sizeof(cfg.inetmail_sem) - 1, K_EDIT);
						break;
					case 3:
						i = (cfg.inetmail_misc & NMAIL_ALLOW) ? 0 : 1;
						uifc.helpbuf =
							"`Allow Users to Send Internet E-mail:`\n"
							"\n"
							"If you want your users to be allowed to send Internet E-mail, set this\n"
							"option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Allow Users to Send E-mail", uifcYesNoOpts);
						if (!i && !(cfg.inetmail_misc & NMAIL_ALLOW)) {
							uifc.changes = TRUE;
							cfg.inetmail_misc |= NMAIL_ALLOW;
						}
						else if (i == 1 && cfg.inetmail_misc & NMAIL_ALLOW) {
							uifc.changes = TRUE;
							cfg.inetmail_misc &= ~NMAIL_ALLOW;
						}
						break;
					case 4:
						i = (cfg.inetmail_misc & NMAIL_FILE) ? 0 : 1;
						uifc.helpbuf =
							"`Allow Users to Send Internet E-mail File Attachments:`\n"
							"\n"
							"If you want your users to be allowed to send Internet E-mail with file\n"
							"attachments, set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Allow Users to Send E-mail with File Attachments", uifcYesNoOpts);
						if (!i && !(cfg.inetmail_misc & NMAIL_FILE)) {
							uifc.changes = TRUE;
							cfg.inetmail_misc |= NMAIL_FILE;
						}
						else if (i == 1 && cfg.inetmail_misc & NMAIL_FILE) {
							uifc.changes = TRUE;
							cfg.inetmail_misc &= ~NMAIL_FILE;
						}
						break;
					case 5:
						i = (cfg.inetmail_misc & NMAIL_ALIAS) ? 0 : 1;
						uifc.helpbuf =
							"`Use Aliases in Internet E-mail:`\n"
							"\n"
							"If you allow aliases on your system and wish users to have their\n"
							"Internet E-mail contain their alias as the `From User`, set this option to\n"
							"`Yes`. If you want all E-mail to be sent using users' real names, set this\n"
							"option to `No`.\n"
							"\n"
							"Users with the '`O`' restriction flag will always send netmail using\n"
							"their alias (their real name is a duplicate of another user account)."
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Use Aliases in Internet E-mail", uifcYesNoOpts);
						if (!i && !(cfg.inetmail_misc & NMAIL_ALIAS)) {
							uifc.changes = TRUE;
							cfg.inetmail_misc |= NMAIL_ALIAS;
						}
						else if (i == 1 && cfg.inetmail_misc & NMAIL_ALIAS) {
							uifc.changes = TRUE;
							cfg.inetmail_misc &= ~NMAIL_ALIAS;
						}
						break;
					case 6:
						i = (cfg.inetmail_misc & NMAIL_KILL) ? 0 : 1;
						uifc.helpbuf =
							"`Kill Internet E-mail After it is Sent:`\n"
							"\n"
							"If you want Internet E-mail messages to be deleted after they are\n"
							"successfully sent, set this option to `Yes`.\n"
						;
						i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
						              , "Kill Internet E-mail After it is Sent", uifcYesNoOpts);
						if (!i && !(cfg.inetmail_misc & NMAIL_KILL)) {
							uifc.changes = TRUE;
							cfg.inetmail_misc |= NMAIL_KILL;
						}
						else if (i == 1 && cfg.inetmail_misc & NMAIL_KILL) {
							uifc.changes = TRUE;
							cfg.inetmail_misc &= ~NMAIL_KILL;
						}
						break;
					case 7:
						ultoa(cfg.inetmail_cost, str, 10);
						uifc.helpbuf =
							"`Cost in Credits to Send Internet E-mail:`\n"
							"\n"
							"This is the number of credits it will cost your users to send Internet\n"
							"E-mail. If you want the sending of Internet E-mail to be free, set this\n"
							"value to `0`.\n"
						;
						uifc.input(WIN_MID | WIN_SAV, 0, 0
						           , "Cost in Credits to Send Internet E-mail"
						           , str, 10, K_EDIT | K_NUMBER);
						cfg.inetmail_cost = atol(str);
						break;
				}
			}
		}
		free_msgs_cfg(&cfg);
		free_main_cfg(&cfg);
	}
}

void qhub_adv_edit(qhub_t* qhub)
{
	char       str[128];
	static int dflt;
	int        i;

	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Pack Command Line", qhub->pack);
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Unpack Command Line", qhub->unpack);
		opt[i][0] = 0;
		sprintf(str, "%s Advanced Options", qhub->id);
		uifc.helpbuf = "`QWK Network Hub Advanced Options`\n"
		               "\n"
		               "The `Pack` and `Unpack Command Lines` are used for creating and extracting\n"
		               "REP (reply) and QWK message packets using an external archive utility\n"
		               "(these command-lines are optional).\n"
		;
		uifc_winmode_t wmode = WIN_ACT | WIN_MID | WIN_SAV;
		switch (uifc.list(wmode, 0, 0, 0, &dflt, 0, str, opt)) {
			case -1:
				return;
				break;
			case 0:
				uifc.helpbuf =
					"`REP Packet Creation Command:`\n"
					"\n"
					"This is the command line to use to create (compress) REP packets for\n"
					"this QWK network hub.\n"
					SCFG_CMDLINE_PREFIX_HELP
					SCFG_CMDLINE_SPEC_HELP
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, ""
				           , qhub->pack, sizeof(qhub->pack) - 1, K_EDIT);
				break;
			case 1:
				uifc.helpbuf =
					"`QWK Packet Extraction Command:`\n"
					"\n"
					"This is the command line to use to extract (decompress) QWK packets from\n"
					"this QWK network hub.\n"
					SCFG_CMDLINE_PREFIX_HELP
					SCFG_CMDLINE_SPEC_HELP
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, ""
				           , qhub->unpack, sizeof(qhub->unpack) - 1, K_EDIT);
				break;
		}
	}
}

void qhub_edit(int num)
{
	static int qhub_dflt;
	char *     p, done = 0, str[256];
	int        i, j;

	while (!done) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Hub System ID", cfg.qhub[num]->id);
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Enabled", cfg.qhub[num]->enabled ? "Yes":"No");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Archive Format", cfg.qhub[num]->fmt);
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Call-out Command Line", cfg.qhub[num]->call);
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Native Call-out Command", cfg.qhub[num]->misc & QHUB_NATIVE ? "Yes":"No");
		if (cfg.qhub[num]->node == NODE_ANY)
			SAFECOPY(str, "Any");
		else
			SAFEPRINTF(str, "%u", cfg.qhub[num]->node);
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Call-out Node", str);
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Call-out Days", daystr(cfg.qhub[num]->days));
		if (cfg.qhub[num]->freq) {
			sprintf(str, "%u times a day", 1440 / cfg.qhub[num]->freq);
			snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Call-out Frequency", str);
		}
		else {
			sprintf(str, "%2.2u:%2.2u", cfg.qhub[num]->time / 60, cfg.qhub[num]->time % 60);
			snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Call-out Time", str);
		}
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Include Kludge Lines", cfg.qhub[num]->misc & QHUB_NOKLUDGES ? "No":"Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Include VOTING.DAT File", cfg.qhub[num]->misc & QHUB_NOVOTING ? "No":"Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Include HEADERS.DAT File", cfg.qhub[num]->misc & QHUB_NOHEADERS ? "No":"Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Include UTF-8 Characters", cfg.qhub[num]->misc & QHUB_UTF8 ? "Yes":"No");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Include MIME-encoded Text", cfg.qhub[num]->misc & QHUB_MIME ? "Yes":"No");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Word-wrap Exported Messages", cfg.qhub[num]->misc & QHUB_WORDWRAP ? "Yes":"No");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Extended (QWKE) Packets", cfg.qhub[num]->misc & QHUB_EXT ? "Yes":"No");
		snprintf(opt[i++], MAX_OPLN, "%-30.30s%s", "Exported Ctrl-A Codes"
		         , cfg.qhub[num]->misc & QHUB_EXPCTLA ? "Expand" : cfg.qhub[num]->misc & QHUB_RETCTLA ? "Leave in" : "Strip");
		strcpy(opt[i++], "Advanced Options...");
		strcpy(opt[i++], "Import Conferences...");
		strcpy(opt[i++], "Networked Sub-boards...");
		opt[i][0] = 0;
		sprintf(str, "%s QWK Network Hub", cfg.qhub[num]->id);
		uifc.helpbuf =
			"`QWK Network Hub Configuration:`\n"
			"\n"
			"This menu allows you to configure options specific to this QWKnet hub.\n"
			"\n"
			"The `Hub System ID` must match the QWK System ID of this network hub.\n"
			"\n"
			"The `Archive Format` should be set to an archive/compression format\n"
			"that the hub will expect your REP packets to be submitted with\n"
			"(typically, ZIP).\n"
			"\n"
			"The `Call-out Command Line` is executed when your system attempts a packet\n"
			"exchange with the QWKnet hub (e.g. executes a script).\n"
			"\n"
			"Toggle `Native Call-out Command` to `Yes` if your Call-out Command-line\n"
			"is invoking a native (not a 16-bit DOS) program or script.\n"
			"\n"
			"`Kludge Lines` (e.g. @TZ, @VIA, @MSGID, @REPLY) provide information not\n"
			"available in standard QWK message headers, but are superfluous when the\n"
			"HEADERS.DAT file is supported and used.\n"
			"\n"
			"The `VOTING.DAT` file is the distributed QWKnet voting system supported\n"
			"in Synchronet v3.17 and later\n"
			"\n"
			"The `HEADERS.DAT` file provides all the same information that can be\n"
			"found in Kludge Lines and also addresses the 25-character QWK field\n"
			"length limits. HEADERS.DAT is supported in Synchronet v3.15 and later.\n"
			"\n"
			"Synchronet v3.18 and later supports `UTF-8` encoded messages within QWK\n"
			"packets. If the hub is using Synchronet v3.18 or later, set this option\n"
			"to `Yes`. This option also changes the QWK new-line sequence to the ASCII\n"
			"LF (10) character instead of the traditional QWK newline byte (227).\n"
			"\n"
			"`Extended (QWKE) Packets` are not normally used in QWK Networking.\n"
			"Setting this to `Yes` enables some QWKE-specific Kludge Lines that are\n"
			"superfluous when the HEADERS.DAT file is supported and used.\n"
			"\n"
			"`Exported Ctrl-A Codes` determines how Synchronet attribute/color\n"
			"codes in messages are exported into the QWK network packets. This\n"
			"may be set to `Leave in` (retain), `Expand` (to ANSI), or `Strip` (remove).\n"
			"This setting is used for QWKnet NetMail messages and may over-ride the\n"
			"equivalent setting for each sub-board.\n"
			"\n"
			"`Import Conferences...` allows you to import a QWK `control.dat` file which\n"
			"can both create new sub-boards in a target message group of your choice\n"
			"and set the `Networked Sub-boards` with QWK conference numbers correlating\n"
			"with the hub system.\n"
			"\n"
			"`Networked Sub-boards...` allows you to manually add, remove, or modify\n"
			"local sub-board associations with conferences on this QWK network hub.\n"
		;
		uifc_winmode_t wmode = WIN_ACT | WIN_MID | WIN_SAV | WIN_EXTKEYS;
		if (num > 0)
			wmode |= WIN_LEFTKEY;
		if (num + 1 < cfg.total_qhubs)
			wmode |= WIN_RIGHTKEY;
		switch (uifc.list(wmode, 0, 0, 0, &qhub_dflt, 0, str, opt)) {
			case -1:
				done = 1;
				break;
			case -CIO_KEY_LEFT - 2:
				if (num > 0)
					num--;
				break;
			case -CIO_KEY_RIGHT - 2:
				if (num + 1 < cfg.total_qhubs)
					num++;
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`QWK Network Hub System ID:`\n"
					"\n"
					"This is the QWK System ID of this hub. It is used for incoming and\n"
					"outgoing network packets and must be accurate.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "QWK Network Hub System ID"
				           , cfg.qhub[num]->id, LEN_QWKID, K_UPPER | K_EDIT);
				break;
			case __COUNTER__:
				cfg.qhub[num]->enabled = !cfg.qhub[num]->enabled;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`REP Packet Archive Format:`\n"
					"\n"
					"This is the archive format used for REP packets created for this QWK\n"
					"network hub (typically, this would be `ZIP`).\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "REP Packet Archive Format"
				           , cfg.qhub[num]->fmt, sizeof(cfg.qhub[num]->fmt) - 1, K_EDIT | K_UPPER);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`QWK Network Hub Call-out Command Line:`\n"
					"\n"
					"This is the command line to use to initiate a call-out to this QWK\n"
					"network hub.\n"
					SCFG_CMDLINE_PREFIX_HELP
					SCFG_CMDLINE_SPEC_HELP
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 0, ""
				           , cfg.qhub[num]->call, sizeof(cfg.qhub[num]->call) - 1, K_EDIT);
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_NATIVE;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				if (cfg.qhub[num]->node == NODE_ANY)
					SAFECOPY(str, "Any");
				else
					SAFEPRINTF(str, "%u", cfg.qhub[num]->node);
				uifc.helpbuf =
					"`Node to Perform Call-out:`\n"
					"\n"
					"This is the number of the node to perform the call-out for this QWK\n"
					"network hub (or `Any`).\n"
				;
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0
				               , "Node to Perform Call-out", str, 3, K_EDIT) > 0) {
					if (IS_DIGIT(*str))
						cfg.qhub[num]->node = atoi(str);
					else
						cfg.qhub[num]->node = NODE_ANY;
				}
				break;
			case __COUNTER__:
				j = 0;
				while (1) {
					for (i = 0; i < 7; i++)
						snprintf(opt[i], MAX_OPLN, "%s        %s"
						         , wday[i], (cfg.qhub[num]->days & (1 << i)) ? "Yes":"No");
					opt[i][0] = 0;
					uifc.helpbuf =
						"`Days to Perform Call-out:`\n"
						"\n"
						"These are the days that a call-out will be performed for this QWK\n"
						"network hub.\n"
					;
					i = uifc.list(WIN_MID, 0, 0, 0, &j, 0
					              , "Days to Perform Call-out", opt);
					if (i == -1)
						break;
					cfg.qhub[num]->days ^= (1 << i);
					uifc.changes = TRUE;
				}
				break;
			case __COUNTER__:
				i = (cfg.qhub[num]->freq != 0);
				uifc.helpbuf =
					"`Perform Call-out at a Specific Time:`\n"
					"\n"
					"If you want the system call this QWK network hub at a specific time,\n"
					"set this option to `Yes`. If you want the system to call this network\n"
					"hub more than once a day at predetermined intervals, set this option to\n"
					"`No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Perform Call-out at a Specific Time", uifcYesNoOpts);
				if (i == 0) {
					sprintf(str, "%2.2u:%2.2u", cfg.qhub[num]->time / 60
					        , cfg.qhub[num]->time % 60);
					uifc.helpbuf =
						"`Time to Perform Call-out:`\n"
						"\n"
						"This is the time (in 24 hour HH:MM format) to perform the call-out to\n"
						"this QWK network hub.\n"
					;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0
					               , "Time to Perform Call-out (HH:MM)"
					               , str, 5, K_UPPER | K_EDIT) > 0) {
						cfg.qhub[num]->freq = 0;
						if ((p = strchr(str, ':')) == NULL) {
							uifc.msg("Incorrect time format");
							break;
						}
						cfg.qhub[num]->time = atoi(str) * 60;
						cfg.qhub[num]->time += atoi(p + 1);
					}
				}
				else if (i == 1) {
					sprintf(str, "%u", cfg.qhub[num]->freq
					        && cfg.qhub[num]->freq <= 1440 ? 1440 / cfg.qhub[num]->freq : 0);
					uifc.helpbuf =
						"`Number of Call-outs Per Day:`\n"
						"\n"
						"This is the maximum number of times the system will perform a call-out\n"
						"per day to this QWK network hub. This value is actually converted by\n"
						"Synchronet into minutes between call-outs and when the BBS is idle\n"
						"and this number of minutes since the last call-out is reached, it will\n"
						"perform a call-out.\n"
					;
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0
					               , "Number of Call-outs Per Day"
					               , str, 4, K_NUMBER | K_EDIT) > 0) {
						cfg.qhub[num]->time = 0;
						i = atoi(str);
						if (i && i <= 1440)
							cfg.qhub[num]->freq = 1440 / i;
						else
							cfg.qhub[num]->freq = 0;
					}
				}
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_NOKLUDGES;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_NOVOTING;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_NOHEADERS;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_UTF8;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_MIME;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_WORDWRAP;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				cfg.qhub[num]->misc ^= QHUB_EXT;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				i = cfg.qhub[num]->misc & QHUB_CTRL_A;
				i++;
				if (i == QHUB_CTRL_A)
					i = 0;
				cfg.qhub[num]->misc &= ~QHUB_CTRL_A;
				cfg.qhub[num]->misc |= i;
				uifc.changes = TRUE;
				break;
			case __COUNTER__:
				qhub_adv_edit(cfg.qhub[num]);
				break;
			case __COUNTER__:
				import_qwk_conferences(num);
				break;
			case __COUNTER__:
				qhub_sub_edit(num);
				break;
		}
	}
}

void qhub_sub_edit(uint num)
{
	char  str[256];
	int   j, k, l, m, n, bar = 0;

	char* qwk_conf_num_help =
		"`Conference Number on Hub:`\n"
		"\n"
		"This is the number of the conference on the QWK network hub.\n"
		"\n"
		"It is important to understand that this is `NOT` necessarily the\n"
		"conference number of this sub-board on your system. It is the number\n"
		"of the conference this sub-board is networked with on this `QWK\n"
		"network hub`.\n"
		"\n"
		"The most reliable source of conference numbers would be taken from\n"
		"a `CONTROL.DAT` file extracted from a `.QWK` packet downloaded from\n"
		"the QWK network hub."
	;
	char* qwk_ctrl_a_help =
		"`Ctrl-A Codes:`\n"
		"\n"
		"You are being prompted for the method of handling Ctrl-A attribute codes\n"
		"generated by Synchronet. If this QWK network hub is a Synchronet BBS,\n"
		"set this option to `Leave in`. If the QWK network hub is not a Synchronet\n"
		"BBS, but allows ANSI escape sequences in messages, set this option to\n"
		"`Expand to ANSI`. If the QWK network hub is not a Synchronet BBS and does\n"
		"not support ANSI escape sequences in messages (or you're not sure), set\n"
		"this option to `Strip out`.\n"
	;
	unsigned last_conf_num = 0;
	k = 0;
	while (1) {
		unsigned opts = 0;
		for (j = 0; j < cfg.qhub[num]->subs; j++) {
			if (cfg.qhub[num]->sub[j] == NULL)
				continue;
			snprintf(opt[opts++], MAX_OPLN, "%-5u %-*.*s %-*.*s"
			         , cfg.qhub[num]->conf[j]
			         , LEN_GSNAME, LEN_GSNAME
			         , cfg.grp[cfg.qhub[num]->sub[j]->grp]->sname
			         , LEN_SSNAME, LEN_SSNAME
			         , cfg.qhub[num]->sub[j]->sname);
		}
		opt[opts][0] = 0;
		int mode = WIN_BOT | WIN_SAV | WIN_ACT;
		if (cfg.qhub[num]->subs < MAX_OPTS)
			mode |= WIN_INS | WIN_INSACT | WIN_XTR;
		if (cfg.qhub[num]->subs)
			mode |= WIN_DEL;
		uifc.helpbuf =
			"`QWK Networked Sub-boards:`\n"
			"\n"
			"This is a list of the sub-boards that are networked with this QWK\n"
			"network hub.\n"
			"\n"
			"To add a sub-board, select the desired location and hit ~ INS ~.\n"
			"\n"
			"To remove a sub-board, select it and hit ~ DEL ~.\n"
			"\n"
			"To configure a sub-board for this QWK network hub, select it and hit\n"
			"~ ENTER ~.\n"
		;
		SAFEPRINTF(str, "%s Networked Sub-boards", cfg.qhub[num]->id);
		j = uifc.list(mode, 0, 0, 0, &k, &bar, str, opt);
		if (j == -1)
			break;
		if ((j & MSK_ON) == MSK_INS) {
			j &= MSK_OFF;
			if ((l = getsub()) == -1)
				continue;
			uifc.helpbuf = qwk_conf_num_help;
			if (last_conf_num)
				SAFEPRINTF(str, "%u", last_conf_num + 1);
			else
				str[0] = 0;
			if (uifc.input(WIN_MID | WIN_SAV, 0, 0
			               , "Conference Number on Hub"
			               , str, 5, K_EDIT | K_NUMBER) < 1)
				continue;
			last_conf_num = atoi(str);
			strcpy(opt[0], "Strip out");
			strcpy(opt[1], "Leave in");
			strcpy(opt[2], "Expand to ANSI");
			opt[3][0] = 0;
			m = 0;
			uifc.helpbuf = qwk_ctrl_a_help;
			if ((m = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &m, 0
			                   , "Ctrl-A Codes", opt)) == -1)
				continue;
			if (!new_qhub_sub(cfg.qhub[num], j, cfg.sub[l], last_conf_num))
				continue;
			if (!m)
				cfg.qhub[num]->mode[j] = QHUB_STRIP;
			else if (m == 1)
				cfg.qhub[num]->mode[j] = QHUB_RETCTLA;
			else
				cfg.qhub[num]->mode[j] = QHUB_EXPCTLA;
			uifc.changes = TRUE;
			k++;
			bar++;
			continue;
		}
		if ((j & MSK_ON) == MSK_DEL) {
			j &= MSK_OFF;
			cfg.qhub[num]->subs--;
			while (j < cfg.qhub[num]->subs) {
				cfg.qhub[num]->sub[j] = cfg.qhub[num]->sub[j + 1];
				cfg.qhub[num]->mode[j] = cfg.qhub[num]->mode[j + 1];
				cfg.qhub[num]->conf[j] = cfg.qhub[num]->conf[j + 1];
				j++;
			}
			uifc.changes = TRUE;
			continue;
		}
		l = 0;
		while (1) {
			n = 0;
			snprintf(opt[n++], MAX_OPLN, "%-22.22s%.*s %.*s"
			         , "Sub-board"
			         , LEN_GSNAME
			         , cfg.grp[cfg.qhub[num]->sub[j]->grp]->sname
			         , LEN_SSNAME
			         , cfg.qhub[num]->sub[j]->sname);
			snprintf(opt[n++], MAX_OPLN, "%-22.22s%u"
			         , "Conference Number", cfg.qhub[num]->conf[j]);
			snprintf(opt[n++], MAX_OPLN, "%-22.22s%s"
			         , "Ctrl-A Codes", cfg.qhub[num]->mode[j] == QHUB_STRIP ?
			         "Strip out" : cfg.qhub[num]->mode[j] == QHUB_RETCTLA ?
			         "Leave in" : "Expand to ANSI");
			opt[n][0] = 0;
			uifc.helpbuf =
				"`QWK Networked Sub-board:`\n"
				"\n"
				"You are configuring the options for this sub-board for this QWK network\n"
				"hub.\n"
			;
			uifc_winmode_t wmode = WIN_MID | WIN_SAV | WIN_ACT | WIN_EXTKEYS;
			if (j > 0)
				wmode |= WIN_LEFTKEY;
			if (j + 1 < cfg.qhub[num]->subs)
				wmode |= WIN_RIGHTKEY;
			l = uifc.list(wmode, 0, 0,
			              22 + LEN_GSNAME + LEN_SSNAME, &l, 0
			              , "Networked Sub-board", opt);
			if (l == -1)
				break;
			if (l == -CIO_KEY_LEFT - 2) {
				if (j > 0)
					j--;
				continue;
			}
			if (l == -CIO_KEY_RIGHT - 2) {
				if (j + 1 < cfg.qhub[num]->subs)
					j++;
				continue;
			}
			if (!l) {
				m = getsub();
				if (m != -1) {
					cfg.qhub[num]->sub[j] = cfg.sub[m];
					uifc.changes = TRUE;
				}
			}
			else if (l == 1) {
				uifc.helpbuf = qwk_conf_num_help;
				sprintf(str, "%u", cfg.qhub[num]->conf[j]);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0
				               , "Conference Number on Hub"
				               , str, 5, K_NUMBER | K_EDIT) > 0)
					cfg.qhub[num]->conf[j] = atoi(str);
			}
			else if (l == 2) {
				strcpy(opt[0], "Strip out");
				strcpy(opt[1], "Leave in");
				strcpy(opt[2], "Expand to ANSI");
				opt[3][0] = 0;
				m = 0;
				uifc.helpbuf = qwk_ctrl_a_help;
				m = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &m, 0
				              , "Ctrl-A Codes", opt);
				uifc.changes = TRUE;
				if (!m)
					cfg.qhub[num]->mode[j] = QHUB_STRIP;
				else if (m == 1)
					cfg.qhub[num]->mode[j] = QHUB_RETCTLA;
				else if (m == 2)
					cfg.qhub[num]->mode[j] = QHUB_EXPCTLA;
			}
		}
	}
}

BOOL import_qwk_conferences(uint qhubnum)
{
	char str[256];

	int  grpnum = getgrp("Target Message Group");
	if (grpnum < 0)
		return FALSE;

	/* QWK Conference number range */
	int min_confnum, max_confnum;
	strcpy(str, "1000");
	uifc.helpbuf = "`Minimum / Maximum QWK Conference Number`:\n"
	               "\n"
	               "This setting allows you to control the range of QWK conference numbers\n"
	               "that will be imported from the `control.dat` file.";
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Minimum QWK Conference Number"
	               , str, 5, K_EDIT | K_NUMBER) < 1)
		return FALSE;
	min_confnum = atoi(str);
	sprintf(str, "%u", min_confnum + 999);
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum QWK Conference Number"
	               , str, 5, K_EDIT | K_NUMBER) < 1)
		return FALSE;
	max_confnum = atoi(str);

	char filename[MAX_PATH + 1] = "control.dat";
	uifc.helpbuf = "Enter the path to the QWK control.dat file to import";
	if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filename"
	               , filename, sizeof(filename) - 1, K_EDIT) <= 0)
		return FALSE;
	(void)fexistcase(filename);
	FILE *fp;
	if ((fp = fopen(filename, "rt")) == NULL) {
		uifc.msg("File Open Failure");
		return FALSE;
	}
	uifc.pop("Importing Areas...");
	long added = 0;
	long ported = import_msg_areas(IMPORT_LIST_TYPE_QWK_CONTROL_DAT, fp, grpnum, min_confnum, max_confnum, cfg.qhub[qhubnum], /* pkt_orig */ NULL, /* faddr: */ NULL, /* misc: */ 0, &added);
	fclose(fp);
	uifc.pop(NULL);
	if (ported < 0)
		sprintf(str, "!ERROR %ld imported message areas", ported);
	else {
		sprintf(str, "%ld Message Areas Imported Successfully (%ld added)", ported, added);
		if (ported > 0)
			uifc.changes = TRUE;
	}
	uifc.msg(str);
	return TRUE;
}

char *daystr(char days)
{
	static char str[256];
	int         i;

	days &= 0x7f;

	if (days == 0)
		return "None";

	if (days == 0x7f)
		return "All";

	str[0] = 0;
	for (i = 0; i < 7; i++) {
		if (days & (1 << i)) {
			SAFECAT(str, wday[i]);
			SAFECAT(str, " ");
		}
	}
	return str;
}
