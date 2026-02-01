/* Synchronet answer "caller" function */

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

#include <stddef.h> // size_t for base64.h
#include "base64.h"
#include "sbbs.h"
#include "telnet.h"
#include "ssl.h"

extern "C" void client_on(SOCKET sock, client_t* client, BOOL update);

bool
sbbs_t::set_authresponse(bool activate_ssh)
{
	int status;

	lprintf(LOG_DEBUG, "%04d SSH Setting attribute: SESSINFO_AUTHRESPONSE", client_socket.load());
	status = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_AUTHRESPONSE, activate_ssh);
	if (cryptStatusError(status)) {
		log_crypt_error_status_sock(status, "setting auth response");
		return false;
	}
	return true;
}

static bool
check_pubkey(scfg_t *cfg, ushort unum, char *pkey, size_t pksz)
{
	// 2048 is enough bytes for anyone!
	// I would absolutely prefer a getline() here. :(
	char  path[MAX_PATH + 1];
	char  keyline[2048 + 1];
	FILE *sshkeys;
	char *brkb;
	char *tok;

	// Obviously not valid...
	if (pksz < 16)
		return false;
	SAFEPRINTF2(path, "%suser/%04d.sshkeys", cfg->data_dir, unum);
	sshkeys = fopen(path, "rb");
	if (sshkeys != NULL) {
		while (fgets(keyline, sizeof(keyline), sshkeys) != NULL) {
			size_t len = strlen(keyline);

			if (keyline[len - 1] != '\n') {
				// Ignore the rest of the line...
				while (keyline[len - 1] != '\n') {
					lprintf(LOG_ERR, "keyline not large enough for key in %s (or missing newline)\n", path);
					if (fgets(keyline, sizeof(keyline), sshkeys) == NULL)
						break;
					len = strlen(keyline);
				}
			}
			else {
				tok = strtok_r(keyline, " \t", &brkb);
				if (tok) {
					tok = strtok_r(NULL, " \t", &brkb);
					if (tok) {
						char pk[2048];
						int  pklen;
						pklen = b64_decode(pk, sizeof(pk), tok, 0);
						if (pklen > 0) {
							if ((pksz - 4) == (unsigned)pklen) {
								if (memcmp(&pkey[4], pk, pklen) == 0) {
									fclose(sshkeys);
									return true;
								}
							}
						}
					}
				}
			}
		}
		fclose(sshkeys);
	}
	return false;
}

bool sbbs_t::answer()
{
	char      str[MAX_PATH + 1], str2[MAX_PATH + 1], c;
	char      tmp[MAX_PATH];
	char *    ctmp;
	char *    pubkey{nullptr};
	size_t    pubkeysz;
	char      path[MAX_PATH + 1];
	int       i, l, in;
	struct tm tm;
	max_socket_inactivity = startup->max_dumbterm_inactivity;
	useron.number = 0;
	answertime = logontime = starttime = now = time(NULL);
	/* Caller ID string is client IP address, by default (may be overridden later) */
	SAFECOPY(cid, client_ipaddr);

	memset(&tm, 0, sizeof(tm));
	localtime_r(&now, &tm);

	llprintf("@", "%-6s  %s %s %02d %u            Node %3u"
	              , tm_as_hhmm(&cfg, &tm, str2)
	              , wday[tm.tm_wday]
	              , mon[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900, cfg.node_num);

	llprintf("@+", "%s  %s [%s]", connection, client_name, client_ipaddr);

	if (client_ident[0]) {
		llprintf("@*", "Identity: %s", client_ident);
	}

	if (sys_status & SS_RLOGIN) {
		if (incom(1000) == 0) {
			for (i = 0; i < (int)sizeof(str) - 1; i++) {
				in = incom(1000);
				if (in == 0 || in == NOINP)
					break;
				str[i] = in;
			}
			str[i] = 0;
			for (i = 0; i < (int)sizeof(str2) - 1; i++) {
				in = incom(1000);
				if (in == 0 || in == NOINP)
					break;
				str2[i] = in;
			}
			str2[i] = 0;
			for (i = 0; i < (int)sizeof(terminal) - 1; i++) {
				in = incom(1000);
				if (in == 0 || in == NOINP)
					break;
				terminal[i] = in;
			}
			terminal[i] = 0;
			lprintf(LOG_DEBUG, "RLogin: '%.*s' / '%.*s' / '%s'"
			        , LEN_ALIAS * 2, str
			        , LEN_ALIAS * 2, str2
			        , terminal);
			SAFECOPY(rlogin_term, terminal);
			SAFECOPY(rlogin_name, parse_login(str2));
			SAFECOPY(rlogin_pass, str);
			/* Truncate terminal speed (e.g. "/57600") from terminal-type string
			   (but keep full terminal type/speed string in rlogin_term): */
			truncstr(terminal, "/");
			useron.number = 0;
			if (rlogin_name[0])
				useron.number = find_login_id(&cfg, rlogin_name);
			if (useron.number) {
				getuserdat(&cfg, &useron);
				SAFEPRINTF(path, "%srlogin.cfg", cfg.ctrl_dir);
				if (!findstr(client.addr, path)) {
					SAFECOPY(tmp, rlogin_pass);
					for (i = 0; i < 3 && online; i++) {
						if (stricmp(tmp, useron.pass)) {
							if (cfg.sys_misc & SM_ECHO_PW)
								llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt: '%s'"
								              , useron.number, useron.alias, tmp);
							else
								llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt"
								              , useron.number, useron.alias);
							badlogin(useron.alias, tmp);
							rioctl(IOFI);       /* flush input buffer */
							bputs(text[InvalidLogon]);
							bputs(text[PasswordPrompt]);
							console |= CON_PASSWORD;
							getstr(tmp, LEN_PASS * 2, K_UPPER | K_LOWPRIO | K_TAB);
							console &= ~CON_PASSWORD;
						}
						else {
							if (user_is_sysop(&useron) && (cfg.sys_misc & SM_SYSPASSLOGIN) && (cfg.sys_misc & SM_R_SYSOP)) {
								rioctl(IOFI);       /* flush input buffer */
								if (!chksyspass())
									bputs(text[InvalidLogon]);
								else {
									i = 0;
									break;
								}
							}
							else {
								i = 0;
								break;
							}
						}
					}
					if (i) {
						if (stricmp(tmp, useron.pass)) {
							if (cfg.sys_misc & SM_ECHO_PW)
								llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt: '%s'"
								              , useron.number, useron.alias, tmp);
							else
								llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt"
								              , useron.number, useron.alias);
							badlogin(useron.alias, tmp);
							bputs(text[InvalidLogon]);
						}
						lprintf(LOG_DEBUG, "!CLIENT IP (%s) NOT LISTED in %s", client.addr, path);
						useron.number = 0;
						hangup();
					}
				}
			}
			else {
				if (cfg.sys_misc & SM_ECHO_PW)
					lprintf(LOG_NOTICE, "RLogin !UNKNOWN USER: '%s' (password: %s)", rlogin_name, rlogin_pass);
				else
					lprintf(LOG_NOTICE, "RLogin !UNKNOWN USER: '%s'", rlogin_name);
				badlogin(rlogin_name, rlogin_pass);
			}
		}
		if (rlogin_name[0] == 0) {
			lprintf(LOG_NOTICE, "!RLogin: No user name received");
			sys_status &= ~SS_RLOGIN;
		}
	}

	if (online && !(telnet_mode & TELNET_MODE_OFF)) {
		/* Disable Telnet Terminal Echo */
		request_telnet_opt(TELNET_WILL, TELNET_ECHO);
		/* Will suppress Go Ahead */
		request_telnet_opt(TELNET_WILL, TELNET_SUP_GA);
		/* Retrieve terminal type and speed from telnet client --RS */
		request_telnet_opt(TELNET_DO, TELNET_TERM_TYPE);
		request_telnet_opt(TELNET_DO, TELNET_TERM_SPEED);
		request_telnet_opt(TELNET_DO, TELNET_SEND_LOCATION);
		request_telnet_opt(TELNET_DO, TELNET_NEGOTIATE_WINDOW_SIZE);
		request_telnet_opt(TELNET_DO, TELNET_NEW_ENVIRON);
	}
#ifdef USE_CRYPTLIB
	if (sys_status & SS_SSH) {
		int  ssh_failed = 0;
		bool activate_ssh = false;

		tmp[0] = 0;
		pthread_mutex_lock(&ssh_mutex);

		if (startup->options & BBS_OPT_SSH_ANYAUTH) {
			lprintf(LOG_DEBUG, "%04d SSH Setting attribute: SESSINFO_ACTIVE", client_socket.load());
			if (cryptStatusError(i = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1))) {
				log_crypt_error_status_sock(i, "setting session active");
				activate_ssh = false;
				// TODO: Add private key here...
				if (i == CRYPT_ENVELOPE_RESOURCE) {
					activate_ssh = set_authresponse(true);
					lprintf(LOG_DEBUG, "%04d SSH Setting attribute: SESSINFO_ACTIVE", client_socket.load());
					i = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1);
					if (cryptStatusError(i)) {
						log_crypt_error_status_sock(i, "setting session active");
						activate_ssh = false;
					}
					else {
						SetEvent(ssh_active);
						lprintf(LOG_DEBUG, "%04d SSH SSH_ANYAUTH allowed presented credential", client_socket.load());
					}
				}
			}
			else {
				activate_ssh = true;
				SetEvent(ssh_active);
				lprintf(LOG_DEBUG, "%04d SSH SSH_ANYAUTH allowed with no credential", client_socket.load());
			}
		}
		else {
			for (ssh_failed = 0; ssh_failed < 3; ssh_failed++) {
				lprintf(LOG_DEBUG, "%04d SSH Setting attribute: SESSINFO_ACTIVE", client_socket.load());
				if (cryptStatusError(i = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1))) {
					log_crypt_error_status_sock(i, "setting session active");
					activate_ssh = false;
					// TODO: Add private key here...
					if (i != CRYPT_ENVELOPE_RESOURCE) {
						break;
					}
				}
				else {
					SetEvent(ssh_active);
					break;
				}
				free_crypt_attrstr(pubkey);
				pubkey = nullptr;
				pubkeysz = 0;
				ctmp = get_crypt_attribute(ssh_session, CRYPT_SESSINFO_USERNAME);
				if (ctmp) {
					SAFECOPY(rlogin_name, parse_login(ctmp));
					free_crypt_attrstr(ctmp);
					ctmp = get_crypt_attribute(ssh_session, CRYPT_SESSINFO_PASSWORD);
					if (ctmp) {
						SAFECOPY(tmp, ctmp);
						free_crypt_attrstr(ctmp);
					}
					else {
						free_crypt_attrstr(pubkey);
						pubkey = get_binary_crypt_attribute(ssh_session, CRYPT_SESSINFO_PUBLICKEY, &pubkeysz);
					}
					lprintf(LOG_DEBUG, "%04d SSH login: '%s'", client_socket.load(), rlogin_name);
				}
				else {
					rlogin_name[0] = 0;
					continue;
				}
				useron.number = find_login_id(&cfg, rlogin_name);
				if (useron.number) {
					if (getuserdat(&cfg, &useron) == 0) {
						if (pubkey) {
							if (check_pubkey(&cfg, useron.number, pubkey, pubkeysz)) {
								SAFECOPY(rlogin_pass, tmp);
								activate_ssh = set_authresponse(true);
								lprintf(LOG_DEBUG, "%04d SSH Public key authentication successful", client_socket.load());
								ssh_failed--;
							}
							else {
								lprintf(LOG_DEBUG, "%04d SSH Public key authentication failed", client_socket.load());
							}
						}
						else {
							if (stricmp(tmp, useron.pass) == 0) {
								SAFECOPY(rlogin_pass, tmp);
								activate_ssh = set_authresponse(true);
								lprintf(LOG_DEBUG, "%04d SSH password authentication successful", client_socket.load());
								ssh_failed--;
							}
							else if (ssh_failed) {
								if (cfg.sys_misc & SM_ECHO_PW)
									llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt: '%s'"
									              , useron.number, useron.alias, tmp);
								else
									llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  FAILED Password attempt"
									              , useron.number, useron.alias);
								badlogin(useron.alias, tmp);
								useron.number = 0;
							}
						}
					}
					else {
						lprintf(LOG_NOTICE, "%04d SSH failed to read user data for %s", client_socket.load(), rlogin_name);
					}
				}
				else {
					if (cfg.sys_misc & SM_ECHO_PW)
						lprintf(LOG_NOTICE, "%04d SSH !UNKNOWN USER: '%s' (password: %s)", client_socket.load(), rlogin_name, truncsp(tmp));
					else
						lprintf(LOG_NOTICE, "%04d SSH !UNKNOWN USER: '%s'", client_socket.load(), rlogin_name);
					badlogin(rlogin_name, tmp);
					// Enable SSH so we can create a new user...
					activate_ssh = set_authresponse(true);
				}
				if (pubkey) {
					free_crypt_attrstr(pubkey);
					pubkey = nullptr;
				}
				if (!activate_ssh)
					set_authresponse(false);
			}
		}
		if (activate_ssh) {
			int  cid;
			char tname[1024];
			int  tnamelen;

			ssh_failed = 0;
			// Check the channel ID and name...
			if (cryptStatusOK(i = cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &cid))) {
				unsigned waits = 0;
				term_output_disabled = true;
				do {
					tnamelen = 0;
					i = cryptSetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, cid);
					log_crypt_error_status_sock(i, "setting channel id");
					if (cryptStatusOK(i)) {
						i = cryptGetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TYPE, tname, &tnamelen);
						log_crypt_error_status_sock(i, "getting channel type");
					}
					if (cryptStatusError(i)) {
						activate_ssh = false;
						break;
					}
					if (tnamelen == 7 && strnicmp(tname, "session", 7) == 0) {
						pthread_mutex_unlock(&ssh_mutex);
						/* TODO: If there's some sort of answer timeout setting,
						 *       it would make sense to use it here... other places
						 *       appear to use arbitrary inter-character timeouts
						 *       I'll just use a five second interpacket gap for now.
						 */
						if (waits == 0)
							lprintf(LOG_DEBUG, "%04d SSH [%s] waiting for channel type.", client_socket.load(), client_ipaddr);
						waits++;
						SLEEP(10);
						waits++;
						if (waits > 500) {
							lprintf(LOG_INFO, "%04d SSH [%s] TIMEOUT waiting for channel type.", client_socket.load(), client_ipaddr);
							activate_ssh = false;
							pthread_mutex_lock(&ssh_mutex);
							break;
						}
						pthread_mutex_lock(&ssh_mutex);
						continue;
					}
					else if (tnamelen == 5 && strnicmp(tname, "shell", 5) == 0) {
						term_output_disabled = false;
						session_channel = cid;
					}
					else if (tnamelen == 9 && strncmp(tname, "subsystem", 9) == 0) {
						i = cryptGetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ARG1, tname, &tnamelen);
						log_crypt_error_status_sock(i, "getting subsystem argument");
						if (cryptStatusError(i)) {
							activate_ssh = false;
							break;
						}
						if (((startup->options & (BBS_OPT_ALLOW_SFTP | BBS_OPT_SSH_ANYAUTH)) == BBS_OPT_ALLOW_SFTP) && tnamelen == 4 && strncmp(tname, "sftp", 4) == 0) {
							if (useron.number) {
								activate_ssh = init_sftp(cid);
								term->cols = 0;
								term->rows = 0;
								SAFECOPY(terminal, "sftp");
								mouse_mode = MOUSE_MODE_OFF;
								autoterm = 0;
								sys_status |= SS_USERON;
								SAFECOPY(client.protocol, "SFTP");
								SAFECOPY(client.user, useron.alias);
								client.usernum = useron.number;
								client_on(client_socket, &client, /* update: */ TRUE);
								SAFECOPY(connection, client.protocol);
								if (getnodedat(cfg.node_num, &thisnode, true)) {
									if ((useron.exempt & FLAG('Q') && useron.misc & QUIET))
										thisnode.status = NODE_QUIET;
									else
										thisnode.status = NODE_INUSE;
									thisnode.action = NODE_XFER;
									thisnode.connection = NODE_CONNECTION_SFTP;
									thisnode.useron = useron.number;
									putnodedat(cfg.node_num, &thisnode);
								}
								SAFECOPY(useron.connection, connection);
								SAFECOPY(useron.ipaddr, client_ipaddr);
								SAFECOPY(useron.host, client_name);
								useron.logons++;
								putuserdat(&useron);
								llprintf("++", "(%04u)  %-25s  %s Logon"
								         , useron.number, useron.alias, client.protocol);
								max_socket_inactivity = startup->max_sftp_inactivity;
							}
							else {
								lprintf(LOG_NOTICE, "%04d Trying to create new user over sftp, disconnecting.", client_socket.load());
								badlogin(rlogin_name, rlogin_pass, "SSH", &client_addr, /* delay: */ false);
								activate_ssh = false;
							}
						}
						else {
							lprintf(LOG_NOTICE, "%04d SSH [%s] active channel subsystem '%.*s' is not 'sftp' (or SFTP not allowed), disconnecting.", client_socket.load(), client_ipaddr, tnamelen, tname);
							badlogin(rlogin_name, rlogin_pass, "SSH", &client_addr, /* delay: */ false);
							// Fail because there's no session.
							activate_ssh = false;
						}
					}
					else {
						lprintf(LOG_NOTICE, "%04d SSH [%s] active channel '%.*s' is not 'session' or 'subsystem', disconnecting.", client_socket.load(), client_ipaddr, tnamelen, tname);
						badlogin(rlogin_name, rlogin_pass, "SSH", &client_addr, /* delay: */ false);
						// Fail because there's no session.
						activate_ssh = false;
					}
					break;
				} while (1);
			}
			else {
				log_crypt_error_status_sock(i, "getting channel id");
				if (i == CRYPT_ERROR_PERMISSION)
					lprintf(LOG_CRIT, "!Your cryptlib build is obsolete, please update");
				activate_ssh = false;
			}
		}
		if (activate_ssh) {
			if (cryptStatusError(i = cryptSetAttribute(ssh_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED))) {
				log_crypt_error_status_sock(i, "clearing owner");
				activate_ssh = false;
			}
		}
		if (!activate_ssh) {
			int status;
			lprintf(LOG_NOTICE, "%04d SSH [%s] session establishment failed", client_socket.load(), client_ipaddr);
			if (cryptStatusError(status = cryptDestroySession(ssh_session))) {
				lprintf(LOG_ERR, "%04d SSH ERROR %d destroying Cryptlib Session %d from %s line %d"
				        , client_socket.load(), status, ssh_session, __FILE__, __LINE__);
			}
			ssh_mode = false;
			pthread_mutex_unlock(&ssh_mutex);
			useron.number = 0;
			return false;
		}

		if (cryptStatusOK(cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_WIDTH, &l)) && l > 0) {
			term->cols = l;
			lprintf(LOG_DEBUG, "%04d SSH [%s] height %d", client_socket.load(), client.addr, term->cols);
		}
		if (cryptStatusOK(cryptGetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_HEIGHT, &l)) && l > 0) {
			term->rows = l;
			lprintf(LOG_DEBUG, "%04d SSH [%s] height %d", client_socket.load(), client.addr, term->rows);
		}
		l = 0;
		if (cryptStatusOK(cryptGetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TERMINAL, terminal, &l)) && l > 0) {
			if (l < (int)sizeof(terminal))
				terminal[l] = 0;
			else
				terminal[sizeof(terminal) - 1] = 0;
			lprintf(LOG_DEBUG, "%04d SSH [%s] term: %s", client_socket.load(), client.addr, terminal);
		}
		pthread_mutex_unlock(&ssh_mutex);

		/*
		 * Just wait here until there's a session... this seems fine.
		 */

		if (!term_output_disabled) {
			if (user_is_sysop(&useron) && (cfg.sys_misc & SM_SYSPASSLOGIN) && (cfg.sys_misc & SM_R_SYSOP)) {
				rioctl(IOFI);       /* flush input buffer */
				if (!chksyspass()) {
					bputs(text[InvalidLogon]);
					hangup();
					useron.number = 0;
					return false;
				}
			}
		}
	}
#endif

	/* Detect terminal type */
	if (!term_output_disabled) {
		// Grab telnet terminal if negotiated already
		if (!(telnet_mode & TELNET_MODE_OFF)) {
			if (autoterm == 0) {
				unsigned loops = 0;
				// Wait up to 2s more for telnet term type
				// TODO: Any way to detect if the remote send a zero-length type?
				lprintf(LOG_DEBUG, "Waiting for telnet terminal negotiation");
				while (telnet_remote_option[TELNET_TERM_TYPE] == TELNET_WILL
				    || telnet_remote_option[TELNET_TERM_TYPE] == 0) {
					/* Stop the input thread from writing to the telnet_* vars */
					pthread_mutex_lock(&input_thread_mutex);
					if (telnet_remote_option[TELNET_NEGOTIATE_WINDOW_SIZE] == TELNET_WILL) {
						if (telnet_cols >= TERM_COLS_MIN && telnet_cols <= TERM_COLS_MAX)
							term->cols = telnet_cols;
						if (telnet_rows >= TERM_ROWS_MIN && telnet_rows <= TERM_ROWS_MAX)
							term->rows = telnet_rows;
					}
					if (telnet_terminal[0]) {
						SAFECOPY(terminal, telnet_terminal);
						pthread_mutex_unlock(&input_thread_mutex);
						break;
					}
					pthread_mutex_unlock(&input_thread_mutex);
					if (++loops >= 30)
						break;
					if (telnet_remote_option[TELNET_TERM_TYPE] == 0 && loops >= 10)
						break;
					mswait(100);    // Allow some time for Telnet negotiation
				}
				lprintf(LOG_DEBUG, "Telnet terminal negotiation wait complete (%u loops)", loops);
			}
		}
		rioctl(IOFI);       /* flush input buffer */
		safe_snprintf(str, sizeof(str), "%s  %s", VERSION_NOTICE, COPYRIGHT_NOTICE);
		if (strcmp(terminal, "PETSCII") == 0) {
			autoterm |= PETSCII;
			update_terminal(this);
		}
		if (autoterm & PETSCII) {
			SAFECOPY(terminal, "PETSCII");
			term->lncntr = 0;
			cls();
			term->center(str, P_80COLS);
		} else {    /* ANSI+ terminal detection */
			/*
			 * TODO: Once this merges, it would be good to split the "ANSI detection"
			 *       out from feature detection (rows/cols/UTF8/CTerm/RIP/etc) so the
			 *       ANSI_Terminal class can be responsible for collecting the
			 *       details of the ANSI implementation.
			 */
			term_out( "\r\n"      /* locate cursor at column 1 */
			        "\x1b[s"    /* save cursor position (necessary for HyperTerm auto-ANSI) */
			        "\x1b[0c"   /* Request CTerm version */
			        "\x1b[255B" /* locate cursor as far down as possible */
			        "\x1b[255C" /* locate cursor as far right as possible */
			        "\x1b[30;40m" // black on black
			        "\b_"       /* need a printable char at this location to actually move cursor */
			        "\x1b[6n"   /* Get cursor position */
			        "\x1b[u"    /* restore cursor position */
			        "\x1b[!_"   /* RIP? */
			        "\r"        /* Move cursor left */
			        "\xef\xbb\xbf"  // UTF-8 Zero-width non-breaking space
			        "\x1b[6n"   /* Get cursor position (again) */
			        "\x1b[0m"   /* "Normal" colors */
			        "\x1b[2J"   /* clear screen */
			        "\x1b[H"    /* home cursor */
			        "\xC"       /* clear screen (in case not ANSI) */
			        "\r"        /* Move cursor left (in case previous char printed) */
			        );
			i = l = 0;
			term->row = 0;
			term->lncntr = 0;
			term->center(str, P_80COLS);

			while (i++ < 50 && l < (int)sizeof(str) - 1) {     /* wait up to 5 seconds for response */
				c = incom(100) & 0x7f;
				if (c == 0)
					continue;
				i = 0;
				if (l == 0 && c != ESC)  // response must begin with escape char
					continue;
				str[l++] = c;
				if (c == 'R') {   /* break immediately if ANSI response */
					mswait(500);
					break;
				}
			}

			while ((c = (incom(100) & 0x7f)) != 0 && l < (int)sizeof(str) - 1)
				str[l++] = c;
			str[l] = 0;

			if (l) {
				truncsp(str);
				c_escape_str(str, tmp, sizeof(tmp) - 1, true);
				lprintf(LOG_DEBUG, "received terminal auto-detection response: '%s'", tmp);

				if (strstr(str, "RIPSCRIP")) {
					if (terminal[0] == 0)
						SAFECOPY(terminal, "RIP");
					logline("@R", strstr(str, "RIPSCRIP"));
					autoterm |= (RIP | COLOR | ANSI);
				}

				char*    tokenizer = NULL;
				char*    p = strtok_r(str, "\x1b", &tokenizer);
				unsigned cursor_pos_report = 0;
				while (p != NULL) {
					int x, y;

					if (terminal[0] == 0)
						SAFECOPY(terminal, "ANSI");
					autoterm |= (ANSI | COLOR);
					if (sscanf(p, "[%u;%uR", &y, &x) == 2) {
						cursor_pos_report++;
						lprintf(LOG_DEBUG, "received ANSI cursor position report [%u]: %ux%u"
						        , cursor_pos_report, x, y);
						if (cursor_pos_report == 1) {
							/* Sanity check the coordinates in the response: */
							if (x >= TERM_COLS_MIN && x <= TERM_COLS_MAX)
								term->cols = x;
							if (y >= TERM_ROWS_MIN && y <= TERM_ROWS_MAX)
								term->rows = y;
						} else {    // second report
							if (x < 3) { // ZWNBSP didn't move cursor (more than one column)
								autoterm |= UTF8;
								unicode_zerowidth = x - 1;
							}
						}
					} else if (sscanf(p, "[=67;84;101;114;109;%u;%u", &x, &y) == 2 && *lastchar(p) == 'c') {
						lprintf(LOG_INFO, "received CTerm version report: %u.%u", x, y);
						term->cterm_version = (x * 1000) + y;
					}
					p = strtok_r(NULL, "\x1b", &tokenizer);
				}
			}

			rioctl(IOFI); /* flush left-over or late response chars */

			if (!autoterm) {
				autoterm |= NO_EXASCII;
				if (str[0]) {
					c_escape_str(str, tmp, sizeof(tmp) - 1, true);
					lprintf(LOG_NOTICE, "terminal auto-detection failed, response: '%s'", tmp);
				}
			}
			if (terminal[0])
				lprintf(LOG_DEBUG, "auto-detected terminal type: %ux%u %s", term->cols, term->rows, terminal);
			else
				SAFECOPY(terminal, "DUMB");
		}

		/* AutoLogon via IP or Caller ID here */
		if (!useron.number && !(sys_status & SS_RLOGIN)
		    && (startup->options & BBS_OPT_AUTO_LOGON) && client_ipaddr[0]) {
			useron.number = finduserstr(0, USER_IPADDR, client_ipaddr);
			if (useron.number) {
				getuserdat(&cfg, &useron);
				if (!(useron.misc & AUTOLOGON) || !(useron.exempt & FLAG('V')))
					useron.number = 0;
			}
		}

		if (!online) {
			useron.number = 0;
			return false;
		}

		if (!(telnet_mode & TELNET_MODE_OFF)) {
			/* Stop the input thread from writing to the telnet_* vars */
			pthread_mutex_lock(&input_thread_mutex);

			if (telnet_cmds_received) {
				if (stricmp(telnet_terminal, "sexpots") == 0) { /* dial-up connection (via SexPOTS) */
					llprintf("@S", "%s connection detected at %u bps", terminal, cur_rate);
					node_connection = (ushort)cur_rate;
					SAFEPRINTF(connection, "%u", cur_rate);
					SAFECOPY(cid, "Unknown");
					SAFECOPY(client_name, "Unknown");
					if (telnet_location[0]) {            /* Caller-ID info provided */
						llprintf("@*", "CID: %s", telnet_location);
						SAFECOPY(cid, telnet_location);
						truncstr(cid, " ");              /* Only include phone number in CID */
						char* p = telnet_location;
						FIND_WHITESPACE(p);
						SKIP_WHITESPACE(p);
						if (*p) {
							SAFECOPY(client_name, p);    /* CID name, if provided (maybe 'P' or 'O' if private or out-of-area) */
						}
					}
					SAFECOPY(client.addr, cid);
					SAFECOPY(client.host, client_name);
					client_on(client_socket, &client, TRUE /* update */);
				} else {
					if (telnet_location[0]) {            /* Telnet Location info provided */
						lprintf(LOG_INFO, "Telnet Location: %s", telnet_location);
						if (trashcan(telnet_location, "ip-silent")) {
							pthread_mutex_unlock(&input_thread_mutex);
							hangup();
							return false;
						}
						if (trashcan(telnet_location, "ip")) {
							pthread_mutex_unlock(&input_thread_mutex);
							lprintf(LOG_NOTICE, "%04d %s !TELNET LOCATION BLOCKED in ip.can: %s"
							        , client_socket.load(), client.protocol, telnet_location);
							hangup();
							return false;
						}
						SAFECOPY(cid, telnet_location);
					}
				}
				if (telnet_speed) {
					lprintf(LOG_INFO, "Telnet Speed: %u bps", telnet_speed.load());
					cur_rate = telnet_speed;
					cur_cps = telnet_speed / 10;
				}
				if (telnet_terminal[0])
					SAFECOPY(terminal, telnet_terminal);
				if (telnet_cols >= TERM_COLS_MIN && telnet_cols <= TERM_COLS_MAX)
					term->cols = telnet_cols;
				if (telnet_rows >= TERM_ROWS_MIN && telnet_rows <= TERM_ROWS_MAX)
					term->rows = telnet_rows;
			} else {
				lprintf(LOG_NOTICE, "no Telnet commands received, reverting to Raw TCP mode");
				telnet_mode |= TELNET_MODE_OFF;
				SAFECOPY(client.protocol, "Raw");
				client_on(client_socket, &client, /* update: */ TRUE);
				SAFECOPY(connection, client.protocol);
				node_connection = NODE_CONNECTION_RAW;
			}
			pthread_mutex_unlock(&input_thread_mutex);
		}
		lprintf(LOG_INFO, "terminal type: %ux%u %s %s", term->cols, term->rows, term_charset(autoterm), terminal);
		SAFECOPY(client_ipaddr, cid);   /* Over-ride IP address with Caller-ID info */
		SAFECOPY(useron.host, client_name);
	}

	update_nodeterm();

	if (!term_output_disabled) {
		if (!useron.number
		    && rlogin_name[0] != 0
		    && !(cfg.sys_misc & SM_CLOSED)
		    && !find_login_id(&cfg, rlogin_name)
		    && !::trashcan(&cfg, rlogin_name, "name")) {
			lprintf(LOG_INFO, "%s !UNKNOWN specified username: '%s', starting new user sign-up", client.protocol, rlogin_name);
			bprintf("%s: %s\r\n", text[UNKNOWN_USER], rlogin_name);
			if (!newuser())
				llprintf(LOG_NOTICE, "N-", "%s !New user registration canceled", client.protocol);
		}

		if (autoterm != NO_EXASCII)
			max_socket_inactivity = startup->max_login_inactivity;

		if (!useron.number) {    /* manual/regular login */

			/* Display ANSWER screen */
			rioctl(IOSM | PAUSE);
			sys_status |= SS_PAUSEON;
			menu("../answer");  // Should use P_NOABORT ?
			sys_status &= ~SS_PAUSEON;
			if (online && (i = exec_mod("login", cfg.login_mod)) != 0)
				lprintf(LOG_ERR, "Error %d executing login module", i);
		} else  /* auto login here */
			logon();
	}

	if (!useron.number)
		hangup();

	if (!online)
		return false;

	if (!(sys_status & SS_USERON)) {
		errormsg(WHERE, ERR_CHK, "User not logged on", sys_status);
		hangup();
		return false;
	}

	if (!term_output_disabled)
		max_socket_inactivity = startup->max_session_inactivity;
	return true;
}
