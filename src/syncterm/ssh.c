/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: ssh.c,v 1.31 2020/05/28 22:58:26 deuce Exp $ */

#include <assert.h>
#include <stdlib.h>

#include "base64.h"
#include "bbslist.h"
#include "ciolib.h"
#include "conn.h"
#include "eventwrap.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "sftp.h"
#include "sockwrap.h"
#include "st_crypt.h"
#include "syncterm.h"
#include "threadwrap.h"
#include "uifcinit.h"
#include "window.h"
#include "xpendian.h"

SOCKET          ssh_sock;
CRYPT_SESSION   ssh_session;
int             ssh_channel;
int             ssh_active = true;
pthread_mutex_t ssh_mutex;
int             sftp_channel = -1;
bool            sftp_active;
sftpc_state_t   sftp_state;

void
cryptlib_error_message(int status, const char *msg)
{
	char  str[64];
	char  str2[64];
	char *errmsg;
	int   err_len;

	sprintf(str, "Error %d %s\r\n\r\n", status, msg);
	pthread_mutex_lock(&ssh_mutex);
	cl.GetAttributeString(ssh_session, CRYPT_ATTRIBUTE_ERRORMESSAGE, NULL, &err_len);
	errmsg = (char *)malloc(err_len + strlen(str) + 5);
	strcpy(errmsg, str);
	cl.GetAttributeString(ssh_session, CRYPT_ATTRIBUTE_ERRORMESSAGE, errmsg + strlen(str), &err_len);
	pthread_mutex_unlock(&ssh_mutex);
	errmsg[strlen(str) + err_len] = 0;
	strcat(errmsg, "\r\n\r\n");
	sprintf(str2, "Error %d %s", status, msg);
	uifcmsg(str2, errmsg);
	free(errmsg);
}

static void
close_sftp_channel(void)
{
	if (sftp_state == NULL)
		return;
	pthread_mutex_lock(&ssh_mutex);
	if (sftp_channel != -1) {
		if (cryptStatusOK(cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel)))
			cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		sftp_channel = -1;
	}
	pthread_mutex_unlock(&ssh_mutex);
	sftpc_finish(sftp_state);
	sftp_state = NULL;
}

static void
close_ssh_channel(void)
{
	pthread_mutex_lock(&ssh_mutex);
	if (ssh_channel != -1) {
		if (cryptStatusOK(cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel)))
			cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		ssh_channel = -1;
	}
	pthread_mutex_unlock(&ssh_mutex);
}

void
ssh_input_thread(void *args)
{
	int    popstatus, gchstatus, status;
	int    rd;
	size_t buffered;
	size_t buffer;
	int    chan;

	SetThreadName("SSH Input");
	while (ssh_active && !conn_api.terminate) {
		if (!socket_readable(ssh_sock, 100))
			continue;

		pthread_mutex_lock(&ssh_mutex);
		conn_api.input_thread_running = 1;
		cl.FlushData(ssh_session);
		popstatus = cl.PopData(ssh_session, conn_api.rd_buf, conn_api.rd_buf_size, &rd);
		gchstatus = cl.GetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &chan);
		pthread_mutex_unlock(&ssh_mutex);

                // Handle case where there was socket activity without readable data (ie: rekey)
		if (popstatus == CRYPT_ERROR_TIMEOUT)
			continue;

		// A final read on a channel just occured... figure out which is missing...
		if (gchstatus == CRYPT_ERROR_NOTFOUND) {
			if (ssh_channel != -1) {
				status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel);
				if (status == CRYPT_ERROR_NOTFOUND) {
					chan = ssh_channel;
				}
			}
			if (chan == -1) {
				if (sftp_channel != -1) {
					status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel);
					if (status == CRYPT_ERROR_NOTFOUND) {
						chan = sftp_channel;
					}
				}
			}
		}

		if (cryptStatusError(popstatus)) {
			if ((popstatus == CRYPT_ERROR_COMPLETE) || (popstatus == CRYPT_ERROR_READ)) { /* connection closed */
				if (chan == ssh_channel)
					break;
			}
			else {
				cryptlib_error_message(popstatus, "recieving data");
				break;
			}
		}
		else {
			if (chan == sftp_channel) {
				if (!sftpc_recv(sftp_state, conn_api.rd_buf, rd)) {
					close_sftp_channel();
					continue;
				}
				if (gchstatus == CRYPT_ERROR_NOTFOUND) {
					sftp_channel = -1;
				}
			}
			else if (chan == ssh_channel) {
				buffered = 0;
				while (buffered < rd) {
					pthread_mutex_lock(&(conn_inbuf.mutex));
					buffer = conn_buf_wait_free(&conn_inbuf, rd - buffered, 100);
					buffered += conn_buf_put(&conn_inbuf, conn_api.rd_buf + buffered, buffer);
					pthread_mutex_unlock(&(conn_inbuf.mutex));
				}
				if (gchstatus == CRYPT_ERROR_NOTFOUND) {
					ssh_channel = -1;
				}
			}
			else {
				assert(true);
				cryptlib_error_message(gchstatus, "unknown channel");
			}
		}
	}
	conn_api.input_thread_running = 2;
}

void
ssh_output_thread(void *args)
{
	int    wr;
	int    ret;
	size_t sent;
	int    status;

	SetThreadName("SSH Output");
	conn_api.output_thread_running = 1;
	while (ssh_active && !conn_api.terminate && ssh_channel != -1) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr = conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if (wr) {
			wr = conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent = 0;
			while (ssh_active && sent < wr) {
				ret = 0;
				pthread_mutex_lock(&ssh_mutex);
				if (ssh_channel == -1) {
					pthread_mutex_unlock(&ssh_mutex);
					break;
				}
				status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, ssh_channel);
				if (cryptStatusOK(status))
					status = cl.PushData(ssh_session, conn_api.wr_buf + sent, wr - sent, &ret);
				pthread_mutex_unlock(&ssh_mutex);
				if (cryptStatusError(status)) {
					if ((status == CRYPT_ERROR_COMPLETE) || (status == CRYPT_ERROR_NOTFOUND)) { /* connection closed */
						break;
					}
					cryptlib_error_message(status, "sending data");
					break;
				}
				sent += ret;
			}
			if (sent) {
				pthread_mutex_lock(&ssh_mutex);
				cl.FlushData(ssh_session);
				pthread_mutex_unlock(&ssh_mutex);
			}
		}
		else {
			pthread_mutex_unlock(&(conn_outbuf.mutex));
		}
	}
	conn_api.output_thread_running = 2;
}

#if NOTYET
static bool
sftp_send(uint8_t *buf, size_t sz, void *cb_data)
{
	size_t sent = 0;
	while (ssh_active && sent < sz) {
		int status;
		int ret;
		pthread_mutex_lock(&ssh_mutex);
		status = cl.FlushData(ssh_session);
		status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel);
		if (cryptStatusOK(status)) {
			status = cl.PushData(ssh_session, buf + sent, sz - sent, &ret);
			if (cryptStatusOK(status))
				status = cl.FlushData(ssh_session);
		}
		pthread_mutex_unlock(&ssh_mutex);
		if (cryptStatusError(status))
			cryptlib_error_message(status, "sending sftp data");
		if (cryptStatusError(status)) {
			if (status == CRYPT_ERROR_COMPLETE) { /* connection closed */
				break;
			}
			cryptlib_error_message(status, "sending sftp data");
			break;
		}
		sent += ret;
	}
	return sent == sz;
}

static char *
get_public_key(CRYPT_CONTEXT ctx)
{
	int sz;
	int status;
	char *raw;
	char *ret;
	size_t rsz;

	status = cl.GetAttributeString(ctx, CRYPT_CTXINFO_SSH_PUBLIC_KEY, NULL, &sz);
	if (cryptStatusOK(status)) {
		fprintf(stderr, "Size: %d\n", sz);
		raw = malloc(sz);
		if (raw != NULL) {
			status = cl.GetAttributeString(ctx, CRYPT_CTXINFO_SSH_PUBLIC_KEY, raw, &sz);
			if (cryptStatusOK(status)) {
				rsz = sz * 4 / 3 + 3;
				ret = malloc(rsz);
				if (ret != NULL) {
					b64_encode(ret, rsz, raw, sz);
					free(raw);
					return ret;
				}
			}
			free(raw);
		}
	}
	fprintf(stderr, "Error %d\n", status);
	return NULL;
}

static bool
add_public_key(struct bbslist *bbs, char *priv)
{
	int status;
	bool added = false;

	// TODO: Without this sleep, all is woe.
	//SLEEP(10);
	while (!conn_api.input_thread_running)
		SLEEP(1);
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Creating SFTP Channel");
	}
	pthread_mutex_lock(&ssh_mutex);
	status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		cryptlib_error_message(status, "setting new channel");
	}
	if (cryptStatusOK(status)) {
		status = cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TYPE, "subsystem", 9);
		if (cryptStatusError(status))
			cryptlib_error_message(status, "setting channel type");
		if (cryptStatusOK(status)) {
			status = cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ARG1, "sftp", 4);
			if (cryptStatusError(status))
				cryptlib_error_message(status, "setting subsystem");
			if (cryptStatusOK(status)) {
				status = cl.GetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &sftp_channel);
				if (cryptStatusError(status))
					cryptlib_error_message(status, "getting new channel");
				if (cryptStatusOK(status)) {
					// TODO: Do something...
				}
			}
		}
	}
	if (sftp_channel != -1) {
		pthread_mutex_lock(&ssh_mutex);
		cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel);
		status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 1);
		pthread_mutex_unlock(&ssh_mutex);
		if (cryptStatusError(status)) {
			cryptlib_error_message(status, "activating sftp session");
		}
		if (cryptStatusOK(status)) {
			sftp_state = sftpc_begin(sftp_send, NULL);
			if (sftp_state != NULL) {
				if (sftpc_init(sftp_state)) {
					sftp_str_t ret = NULL;
					sftp_filehandle_t f = NULL;

					if (sftpc_open(sftp_state, ".ssh/authorized_keys", SSH_FXF_READ, NULL, &f)) {
						if (sftpc_read(sftp_state, f, 0, 1024, &ret)) {
							fprintf(stderr, "First lines... %s\n", ret->c_str);
							free_sftp_str(ret);
							added = true;
						}
						sftpc_close(sftp_state, &f);
					}
				}
			}
		}
		pthread_mutex_lock(&ssh_mutex);
		cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sftp_channel);
		status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0);
		if (cryptStatusOK(status))
			sftp_channel = -1;
		pthread_mutex_unlock(&ssh_mutex);
	}
	return added;
}
#else
static char *
get_public_key(CRYPT_CONTEXT ctx)
{
	return NULL;
}

static bool
add_public_key(struct bbslist *bbs, char *priv)
{
	return true;
}
#endif

static void
error_popup(struct bbslist *bbs, const char *blurb, int status)
{
	if (!bbs->hidepopups)
		cryptlib_error_message(status, blurb);
	conn_api.terminate = 1;
	if (!bbs->hidepopups)
		uifc.pop(NULL);
}

#define KEY_PASSWORD "TODO:ThisIsDumb"
#define KEY_LABEL "ssh_key"
int
ssh_connect(struct bbslist *bbs)
{
	int           off = 1;
	int           status;
	char          password[MAX_PASSWD_LEN + 1];
	char          username[MAX_USER_LEN + 1];
	int           rows, cols;
	const char   *term;
	int           slen;
	uint8_t       server_fp[sizeof(bbs->ssh_fingerprint)];
	char          path[MAX_PATH+1];
	CRYPT_KEYSET  ssh_keyset;
	CRYPT_CONTEXT ssh_context;
	char         *pubkey = NULL;

	ssh_channel = -1;
	sftp_channel = -1;
	if (!bbs->hidepopups)
		init_uifc(true, true);
	pthread_mutex_init(&ssh_mutex, NULL);

	if (!crypt_loaded) {
		if (!bbs->hidepopups) {
			uifcmsg("Cannot load cryptlib - SSH inoperative", "`Cannot load cryptlib`\n\n"
			    "Cannot load the file "
#ifdef _WIN32
			    "cl32.dll"
#else
			    "libcl.so"
#endif
			    "\nThis file is required for SSH functionality.\n\n"
			    "The newest version is always available from:\n"
			    "http://www.cs.auckland.ac.nz/~pgut001/cryptlib/");
			return conn_api.terminate = -1;
		}
	}

	get_syncterm_filename(path, sizeof(path), SYNCTERM_PATH_KEYS, false);
	if(cryptStatusOK(cl.KeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, path, CRYPT_KEYOPT_READONLY))) {
		status = cl.GetPrivateKey(ssh_keyset, &ssh_context, CRYPT_KEYID_NAME, KEY_LABEL, KEY_PASSWORD);
		if(cryptStatusError(status)) {
			error_popup(bbs, "creating context", status);
		}
		if (cryptStatusError(cl.KeysetClose(ssh_keyset))) {
			error_popup(bbs, "closing keyset", status);
		}
	}
	else {
		do {
			/* Couldn't do that... create a new context and use the key from there... */
			status = cl.CreateContext(&ssh_context, CRYPT_UNUSED, CRYPT_ALGO_RSA);
			if (cryptStatusError(status)) {
				error_popup(bbs, "creating context", status);
				break;
			}
			status = cl.SetAttributeString(ssh_context, CRYPT_CTXINFO_LABEL, KEY_LABEL, strlen(KEY_LABEL));
			if (cryptStatusError(status)) {
				error_popup(bbs, "setting label", status);
				break;
			}
			status = cl.GenerateKey(ssh_context);
			if (cryptStatusError(status)) {
				error_popup(bbs, "generating key", status);
				break;
			}

			/* Ok, now try saving this one... use the syspass to encrypt it. */
			status = cl.KeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, path, CRYPT_KEYOPT_CREATE);
			if (cryptStatusError(status)) {
				error_popup(bbs, "creating keyset", status);
				break;
			}
			status = cl.AddPrivateKey(ssh_keyset, ssh_context, KEY_PASSWORD);
			if (cryptStatusError(status)) {
				cl.KeysetClose(ssh_keyset);
				error_popup(bbs, "adding private key", status);
				break;
			}
			status = cl.KeysetClose(ssh_keyset);
			if (cryptStatusError(status)) {
				error_popup(bbs, "closing keyset", status);
				break;
			}
		} while(0);
	}
	if (cryptStatusError(status)) {
		cl.DestroyContext(ssh_context);
		ssh_context = -1;
	}

	ssh_sock = conn_socket_connect(bbs);
	if (ssh_sock == INVALID_SOCKET)
		return -1;

	ssh_active = true;

	if (!bbs->hidepopups)
		uifc.pop("Creating Session");
	status = cl.CreateSession(&ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH);
	if (cryptStatusError(status)) {
		error_popup(bbs, "creating session", status);
		return -1;
	}

        /* we need to disable Nagle on the socket. */
	if (setsockopt(ssh_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)))
		fprintf(stderr, "%s:%d: Error %d calling setsockopt()\n", __FILE__, __LINE__, errno);

	SAFECOPY(password, bbs->password);
	SAFECOPY(username, bbs->user);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	if (!username[0]) {
		if (bbs->hidepopups)
			init_uifc(false, false);
		uifcinput("UserID", MAX_USER_LEN, username, 0, "No stored UserID.");
		if (bbs->hidepopups)
			uifcbail();
	}

	if (!bbs->hidepopups)
		uifc.pop("Setting Username");

        /* Add username/password */
	status = cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_USERNAME, username, strlen(username));
	if (cryptStatusError(status)) {
		error_popup(bbs, "setting username", status);
		return -1;
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);
	if (bbs->conn_type == CONN_TYPE_SSHNA) {
		status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_OPTIONS, CRYPT_SSHOPTION_NONE_AUTH);
		if (cryptStatusError(status)) {
			error_popup(bbs, "disabling password auth", status);
			return -1;
		}
	}
	else {
		if (!password[0]) {
			if (bbs->hidepopups)
				init_uifc(false, false);
			uifcinput("Password", MAX_PASSWD_LEN, password, K_PASSWORD, "Incorrect password.  Try again.");
			if (bbs->hidepopups)
				uifcbail();
		}

		if (!bbs->hidepopups)
			uifc.pop("Setting Password");
		status = cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_PASSWORD, password, strlen(password));
		if (cryptStatusError(status)) {
			error_popup(bbs, "setting password", status);
			return -1;
		}

		if (ssh_context != -1) {
			if (!bbs->hidepopups)
				uifc.pop("Setting Private Key");
			pubkey = get_public_key(ssh_context);
			status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_PRIVATEKEY, ssh_context);
			cl.DestroyContext(ssh_context);
			if (cryptStatusError(status)) {
				free(pubkey);
				error_popup(bbs, "setting private key", status);
				return -1;
			}
		}
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Username");
	}

        /* Pass socket to cryptlib */
	status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, ssh_sock);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "passing socket", status);
		return -1;
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Type");
	}
	term = get_emulation_str(get_emulation(bbs));
	status = cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TERMINAL, term, strlen(term));

	get_term_win_size(&cols, &rows, NULL, NULL, &bbs->nostatus);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Width");
	}
	status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_WIDTH, cols);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Height");
	}
	status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_HEIGHT, rows);

	cl.SetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 1);

        /* Activate the session */
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Activating Session");
	}
	status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "activating session", status);
		return -1;
	}
	cl.FlushData(ssh_session);

	ssh_active = true;
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Clearing Ownership");
	}
	status = cl.SetAttribute(ssh_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "clearing session ownership", status);
		return -1;
	}
	if (!bbs->hidepopups) {
                /* Clear ownership */
		uifc.pop(NULL);
		uifc.pop("Getting SSH Channel");
	}
	status = cl.GetAttribute(ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &ssh_channel);
	if (cryptStatusError(status)) {
		free(pubkey);
		error_popup(bbs, "getting ssh channel", status);
		return -1;
	}

	memset(server_fp, 0, sizeof(server_fp));
	status = cl.GetAttributeString(ssh_session, CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1, server_fp, &slen);
	if (cryptStatusOK(status)) {
		if (memcmp(bbs->ssh_fingerprint, server_fp, sizeof(server_fp))) {
			static const char * const opts[4] = {"Disconnect", "Update", "Ignore", ""};
			FILE *listfile;
			str_list_t inifile;
			char fpstr[41];
			int i;

			slen = 0;
			if (bbs->has_fingerprint) {
				char ofpstr[41];

				for (i = 0; i < sizeof(server_fp); i++) {
					sprintf(&fpstr[i * 2], "%02x", server_fp[i]);
				}
				for (i = 0; i < sizeof(server_fp); i++) {
					sprintf(&ofpstr[i * 2], "%02x", bbs->ssh_fingerprint[i]);
				}
				asprintf(&uifc.helpbuf, "`Fingerprint Changed`\n\n"
				    "The server fingerprint has changed from the last known good connection.\n"
				    "This may indicate someone is evesdropping on your connection.\n"
				    "It is also possible that a host key has just been changed.\n"
				    "\n"
				    "Last known fingerprint: %s\n"
				    "Fingerprint sent now:   %s\n", ofpstr, fpstr);
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &slen, NULL, "Fingerprint Changed", (char **)opts);
				free(uifc.helpbuf);
			}
			else
				i = 1;
			switch(i) {
				case 1:
					if ((listfile = fopen(settings.list_path, "r")) != NULL) {
						inifile = iniReadFile(listfile);
						fclose(listfile);
						iniSetString(&inifile, bbs->name, "SSHFingerprint", fpstr, &ini_style);
						if ((listfile = fopen(settings.list_path, "w")) != NULL) {
							iniWriteFile(listfile, inifile);
							fclose(listfile);
						}
						strListFree(&inifile);
					}
					break;
				case 2:
					break;
				default:
					free(pubkey);
					if (!bbs->hidepopups)
						uifc.pop(NULL);
					return -1;
			}
		}
		bbs->has_fingerprint = true;
	} else {
		fprintf(stderr, "Failed to get fingerprint. :(\n");
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	create_conn_buf(&conn_inbuf, BUFFER_SIZE);
	create_conn_buf(&conn_outbuf, BUFFER_SIZE);
	conn_api.rd_buf = (unsigned char *)malloc(BUFFER_SIZE);
	conn_api.rd_buf_size = BUFFER_SIZE;
	conn_api.wr_buf = (unsigned char *)malloc(BUFFER_SIZE);
	conn_api.wr_buf_size = BUFFER_SIZE;

	_beginthread(ssh_output_thread, 0, NULL);
	_beginthread(ssh_input_thread, 0, NULL);

	add_public_key(bbs, pubkey);
	free(pubkey);

	if (!bbs->hidepopups)
		uifc.pop(NULL); // TODO: Why is this called twice?

	return 0;
}

int
ssh_close(void)
{
	char garbage[1024];

	conn_api.terminate = 1;
	close_sftp_channel();
	close_ssh_channel();
	ssh_active = false;
	while (conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	cl.DestroySession(ssh_session);
	closesocket(ssh_sock);
	ssh_sock = INVALID_SOCKET;
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	pthread_mutex_destroy(&ssh_mutex);
	return 0;
}
