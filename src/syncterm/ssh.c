/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: ssh.c,v 1.31 2020/05/28 22:58:26 deuce Exp $ */

#include <stdlib.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "sockwrap.h"
#include "threadwrap.h"

#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"
#include "ciolib.h"

#include "st_crypt.h"

#include "syncterm.h"
#include "window.h"

SOCKET	ssh_sock;
CRYPT_SESSION	ssh_session;
int				ssh_active=FALSE;
pthread_mutex_t	ssh_mutex;

void cryptlib_error_message(int status, const char * msg)
{
	char	str[64];
	char	str2[64];
	char	*errmsg;
	int		err_len;

	sprintf(str,"Error %d %s\r\n\r\n",status, msg);
	pthread_mutex_lock(&ssh_mutex);
	cl.GetAttributeString(ssh_session, CRYPT_ATTRIBUTE_ERRORMESSAGE, NULL, &err_len);
	errmsg=(char *)malloc(err_len+strlen(str)+5);
	strcpy(errmsg, str);
	cl.GetAttributeString(ssh_session, CRYPT_ATTRIBUTE_ERRORMESSAGE, errmsg+strlen(str), &err_len);
	pthread_mutex_unlock(&ssh_mutex);
	errmsg[strlen(str)+err_len]=0;
	strcat(errmsg,"\r\n\r\n");
	sprintf(str2,"Error %s", msg);
	uifcmsg(str2, errmsg);
	free(errmsg);
}

void ssh_input_thread(void *args)
{
	fd_set	rds;
	int status;
	int rd;
	size_t	buffered;
	size_t	buffer;
	struct timeval tv;

	SetThreadName("SSH Input");
	conn_api.input_thread_running=1;
	while(ssh_active && !conn_api.terminate) {
		FD_ZERO(&rds);
		FD_SET(ssh_sock, &rds);
		tv.tv_sec = 0;
		tv.tv_usec = 100;

		rd=select(ssh_sock+1, &rds, NULL, NULL, &tv);
		if(rd==-1) {
			if(errno==EBADF)
				break;
			rd=0;
		}
		if(rd == 0)
			continue;

		pthread_mutex_lock(&ssh_mutex);
		cl.FlushData(ssh_session);
		status=cl.PopData(ssh_session, conn_api.rd_buf, conn_api.rd_buf_size, &rd);
		pthread_mutex_unlock(&ssh_mutex);

		// Handle case where there was socket activity without readable data (ie: rekey)
		if (status == CRYPT_ERROR_TIMEOUT)
			continue;

		if(cryptStatusError(status)) {
			if(status==CRYPT_ERROR_COMPLETE || status == CRYPT_ERROR_READ) {	/* connection closed */
				ssh_active=FALSE;
				break;
			}
			cryptlib_error_message(status, "recieving data");
			ssh_active=FALSE;
			break;
		}
		else {
			buffered=0;
			while(buffered < rd) {
				pthread_mutex_lock(&(conn_inbuf.mutex));
				buffer=conn_buf_wait_free(&conn_inbuf, rd-buffered, 100);
				buffered+=conn_buf_put(&conn_inbuf, conn_api.rd_buf+buffered, buffer);
				pthread_mutex_unlock(&(conn_inbuf.mutex));
			}
		}
	}
	conn_api.input_thread_running=2;
}

void ssh_output_thread(void *args)
{
	int		wr;
	int		ret;
	size_t	sent;
	int		status;

	SetThreadName("SSH Output");
	conn_api.output_thread_running=1;
	while(ssh_active && !conn_api.terminate) {
		pthread_mutex_lock(&(conn_outbuf.mutex));
		wr=conn_buf_wait_bytes(&conn_outbuf, 1, 100);
		if(wr) {
			wr=conn_buf_get(&conn_outbuf, conn_api.wr_buf, conn_api.wr_buf_size);
			pthread_mutex_unlock(&(conn_outbuf.mutex));
			sent=0;
			while(sent < wr) {
				pthread_mutex_lock(&ssh_mutex);
				status=cl.PushData(ssh_session, conn_api.wr_buf+sent, wr-sent, &ret);
				pthread_mutex_unlock(&ssh_mutex);
				if(cryptStatusError(status)) {
					if(status==CRYPT_ERROR_COMPLETE) {	/* connection closed */
						ssh_active=FALSE;
						break;
					}
					cryptlib_error_message(status, "sending data");
					ssh_active=FALSE;
					break;
				}
				sent += ret;
			}
			if(sent) {
				pthread_mutex_lock(&ssh_mutex);
				cl.FlushData(ssh_session);
				pthread_mutex_unlock(&ssh_mutex);
			}
		}
		else
			pthread_mutex_unlock(&(conn_outbuf.mutex));
	}
	conn_api.output_thread_running=2;
}

int ssh_connect(struct bbslist *bbs)
{
	int off=1;
	int status;
	char password[MAX_PASSWD_LEN+1];
	char username[MAX_USER_LEN+1];
	int	rows,cols;
	const char *term;

	if (!bbs->hidepopups)
		init_uifc(TRUE, TRUE);
	pthread_mutex_init(&ssh_mutex, NULL);

	if(!crypt_loaded) {
		if (!bbs->hidepopups) {
			uifcmsg("Cannot load cryptlib - SSH inoperative",	"`Cannot load cryptlib`\n\n"
						"Cannot load the file "
#ifdef _WIN32
						"cl32.dll"
#else
						"libcl.so"
#endif
						"\nThis file is required for SSH functionality.\n\n"
						"The newest version is always available from:\n"
						"http://www.cs.auckland.ac.nz/~pgut001/cryptlib/"
						);
			return(conn_api.terminate=-1);
		}
	}

	ssh_sock=conn_socket_connect(bbs);
	if(ssh_sock==INVALID_SOCKET)
		return(-1);

	ssh_active=FALSE;

	if (!bbs->hidepopups)
		uifc.pop("Creating Session");
	status=cl.CreateSession(&ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH);
	if(cryptStatusError(status)) {
		char	str[1024];
		sprintf(str,"Error %d creating session",status);
		if (!bbs->hidepopups)
			uifcmsg("Error creating session",str);
		conn_api.terminate=1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return(-1);
	}

	/* we need to disable Nagle on the socket. */
	setsockopt(ssh_sock, IPPROTO_TCP, TCP_NODELAY, ( char * )&off, sizeof ( off ) );

	SAFECOPY(password,bbs->password);
	SAFECOPY(username,bbs->user);

	if (!bbs->hidepopups)
		uifc.pop(NULL);

	if(!username[0]) {
		if (bbs->hidepopups)
			init_uifc(FALSE, FALSE);
		uifcinput("UserID",MAX_USER_LEN,username,0,"No stored UserID.");
		if (bbs->hidepopups)
			uifcbail();
	}

	if (!bbs->hidepopups)
		uifc.pop("Setting Username");
	/* Add username/password */
	status=cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_USERNAME, username, strlen(username));
	if(cryptStatusError(status)) {
		char	str[1024];
		sprintf(str,"Error %d setting username",status);
		if (!bbs->hidepopups)
			uifcmsg("Error setting username",str);
		conn_api.terminate=1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return(-1);
	}

	if (!bbs->hidepopups)
		uifc.pop(NULL);
	if (bbs->conn_type == CONN_TYPE_SSHNA) {
		status = cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_OPTIONS, CRYPT_SSHOPTION_NONE_AUTH);
		if(cryptStatusError(status)) {
			char	str[1024];
			sprintf(str,"Error %d disabling password auth",status);
			if (!bbs->hidepopups)
				uifcmsg("Error disabling password auth",str);
			conn_api.terminate=1;
			if (!bbs->hidepopups)
				uifc.pop(NULL);
			return(-1);
		}
	}
	else {
		if(!password[0]) {
			if (bbs->hidepopups)
				init_uifc(FALSE, FALSE);
			uifcinput("Password",MAX_PASSWD_LEN,password,K_PASSWORD,"Incorrect password.  Try again.");
			if (bbs->hidepopups)
				uifcbail();
		}

		if (!bbs->hidepopups)
			uifc.pop("Setting Password");
		status=cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_PASSWORD, password, strlen(password));
		if(cryptStatusError(status)) {
			char	str[1024];
			sprintf(str,"Error %d setting password",status);
			if (!bbs->hidepopups)
				uifcmsg("Error setting password",str);
			conn_api.terminate=1;
			if (!bbs->hidepopups)
				uifc.pop(NULL);
			return(-1);
		}
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Username");
	}
	/* Pass socket to cryptlib */
	status=cl.SetAttribute(ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, ssh_sock);
	if(cryptStatusError(status)) {
		char	str[1024];
		sprintf(str,"Error %d passing socket",status);
		if (!bbs->hidepopups)
			uifcmsg("Error passing socket",str);
		conn_api.terminate=1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return(-1);
	}

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Type");
	}
	term = get_emulation_str(get_emulation(bbs));
	status=cl.SetAttributeString(ssh_session, CRYPT_SESSINFO_SSH_TERMINAL, term, strlen(term));

	get_term_win_size(&cols, &rows, &bbs->nostatus);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Width");
	}
	status=cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_WIDTH, cols);

	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Setting Terminal Height");
	}
	status=cl.SetAttribute(ssh_session, CRYPT_SESSINFO_SSH_HEIGHT, rows);

	cl.SetAttribute(ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 1);

	/* Activate the session */
	if (!bbs->hidepopups) {
		uifc.pop(NULL);
		uifc.pop("Activating Session");
	}
	status=cl.SetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 1);
	if(cryptStatusError(status)) {
		if (!bbs->hidepopups)
			cryptlib_error_message(status, "activating session");
		conn_api.terminate=1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return(-1);
	}

	ssh_active=TRUE;
	if (!bbs->hidepopups) {
		/* Clear ownership */
		uifc.pop(NULL);	// TODO: Why is this called twice?
		uifc.pop(NULL);
		uifc.pop("Clearing Ownership");
	}
	status=cl.SetAttribute(ssh_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED);
	if(cryptStatusError(status)) {
		if (!bbs->hidepopups)
			cryptlib_error_message(status, "clearing session ownership");
		conn_api.terminate=1;
		if (!bbs->hidepopups)
			uifc.pop(NULL);
		return(-1);
	}
	if (!bbs->hidepopups)
		uifc.pop(NULL);

	create_conn_buf(&conn_inbuf, BUFFER_SIZE);
	create_conn_buf(&conn_outbuf, BUFFER_SIZE);
	conn_api.rd_buf=(unsigned char *)malloc(BUFFER_SIZE);
	conn_api.rd_buf_size=BUFFER_SIZE;
	conn_api.wr_buf=(unsigned char *)malloc(BUFFER_SIZE);
	conn_api.wr_buf_size=BUFFER_SIZE;

	_beginthread(ssh_output_thread, 0, NULL);
	_beginthread(ssh_input_thread, 0, NULL);

	if (!bbs->hidepopups)
		uifc.pop(NULL);	// TODO: Why is this called twice?

	return(0);
}

int ssh_close(void)
{
	char garbage[1024];

	conn_api.terminate=1;
	ssh_active=FALSE;
	cl.SetAttribute(ssh_session, CRYPT_SESSINFO_ACTIVE, 0);
	while(conn_api.input_thread_running == 1 || conn_api.output_thread_running == 1) {
		conn_recv_upto(garbage, sizeof(garbage), 0);
		SLEEP(1);
	}
	cl.DestroySession(ssh_session);
	closesocket(ssh_sock);
	ssh_sock=INVALID_SOCKET;
	destroy_conn_buf(&conn_inbuf);
	destroy_conn_buf(&conn_outbuf);
	FREE_AND_NULL(conn_api.rd_buf);
	FREE_AND_NULL(conn_api.wr_buf);
	pthread_mutex_destroy(&ssh_mutex);
	return(0);
}
