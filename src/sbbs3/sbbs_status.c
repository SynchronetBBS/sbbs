#include <sys/stat.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "sbbs_status.h"

#include "genwrap.h"
#include "sockwrap.h"
#include "link_list.h"

static uint64_t svc_error_count[SERVICE_COUNT] = {0};
static char *status[SERVICE_COUNT] = {0};
static unsigned started = 0;
static int clients[SERVICE_COUNT] = {0};
static int threads[SERVICE_COUNT] = {0};
static int sockets[SERVICE_COUNT] = {0};
static link_list_t svc_client_list[SERVICE_COUNT] = {{0}};
static uint64_t served[SERVICE_COUNT] = {0};
static link_list_t status_sock;

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t status_mutex[SERVICE_COUNT];
static pthread_mutex_t status_thread_mutex;

static void init_lists(void)
{
	int i;

	listInit(&status_sock, LINK_LIST_MUTEX);
	pthread_mutex_init(&status_thread_mutex, NULL);
	for (i=0; i<SERVICE_COUNT; i++) {
		pthread_mutex_init(&status_mutex[i], NULL);
		listInit(&svc_client_list[i], LINK_LIST_MUTEX);
	}
}

static void sendsmsg(struct sbbs_status_msg *msg)
{
	int ret;
	list_node_t* node;
	list_node_t* next;
	SOCKET *sock;

	listLock(&status_sock);
	for(node=status_sock.first; node!=NULL; node=next) {
		next = node->next;
		sock = node->data;
		ret = send(*sock, msg, msg->hdr.len, MSG_EOR);
		if (ret == SOCKET_ERROR) {
			if (ERROR_VALUE != EAGAIN) {
				closesocket(*sock);
				listRemoveNode(&status_sock, node, TRUE);
			}
		}
	}
	listUnlock(&status_sock);
}

void status_lputs(enum sbbs_status_service svc, int level, const char *str)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.lputs.msg)+strlen(str)+1;
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_LPUTS;
	msg->hdr.len = sz;
	msg->msg.lputs.level = level;
	strcpy(msg->msg.lputs.msg, str);
	sendsmsg(msg);
	free(msg);
}

void status_errormsg(enum sbbs_status_service svc, int level, const char *str)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	svc_error_count[svc]++;
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.errormsg.msg)+strlen(str)+1;
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_ERRORMSG;
	msg->hdr.len = sz;
	msg->msg.errormsg.level = level;
	strcpy(msg->msg.errormsg.msg, str);
	sendsmsg(msg);
	free(msg);
}

void status_status(enum sbbs_status_service svc, const char *str)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	if (status[svc])
		free(status[svc]);
	status[svc] = strdup(str);
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.status.status)+strlen(str)+1;
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_STATUS;
	msg->hdr.len = sz;
	strcpy(msg->msg.status.status, str);
	sendsmsg(msg);
	free(msg);
}

void status_started(enum sbbs_status_service svc)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	started |= 1<<svc;
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.started.unused);
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_STARTED;
	msg->hdr.len = sz;
	sendsmsg(msg);
	free(msg);
}

void status_recycle(enum sbbs_status_service svc)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.recycle.unused);
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_RECYCLE;
	msg->hdr.len = sz;
	sendsmsg(msg);
	free(msg);
}

void status_terminated(enum sbbs_status_service svc, int code)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	started &= ~(1<<svc);
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.terminated) + sizeof(msg->msg.terminated);
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_TERMINATED;
	msg->hdr.len = sz;
	msg->msg.terminated.code = code;
	sendsmsg(msg);
	free(msg);
}

void status_clients(enum sbbs_status_service svc, int active)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	clients[svc] = active;
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.clients) + sizeof(msg->msg.clients);
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_CLIENTS;
	msg->hdr.len = sz;
	msg->msg.clients.active = active;
	sendsmsg(msg);
	free(msg);
}

void status_thread_up(enum sbbs_status_service svc, BOOL up, BOOL setuid)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	threads[svc] += up ? 1 : -1;
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.thread_up) + sizeof(msg->msg.thread_up);
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_THREAD_UP;
	msg->hdr.len = sz;
	msg->msg.thread_up.up = up;
	msg->msg.thread_up.setuid = setuid;
	sendsmsg(msg);
	free(msg);
}

void status_socket_open(enum sbbs_status_service svc, BOOL open)
{
	struct sbbs_status_msg *msg;
	size_t sz;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	sockets[svc] += open ? 1 : -1;
	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	sz = offsetof(struct sbbs_status_msg, msg.socket_open) + sizeof(msg->msg.socket_open);
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_SOCKET_OPEN;
	msg->hdr.len = sz;
	msg->msg.socket_open.open = open;
	sendsmsg(msg);
	free(msg);
}

void status_client_on(enum sbbs_status_service svc, BOOL on, SOCKET sock, client_t *client, BOOL update)
{
	client_t tmpc = {0};
	struct sbbs_status_msg *msg;
	size_t sz;
	char *p;

	pthread_once(&init_once, init_lists);
	pthread_mutex_lock(&status_mutex[svc]);
	if(on) {
		if(update) {
			list_node_t*	node;

			listLock(&svc_client_list[svc]);
			if((node=listFindTaggedNode(&svc_client_list[svc], sock)) != NULL)
				memcpy(node->data, client, sizeof(client_t));
			listUnlock(&svc_client_list[svc]);
		} else {
			served[svc]++;
			listAddNodeData(&svc_client_list[svc], client, sizeof(client_t), sock, LAST_NODE);
		}
	} else
		listRemoveTaggedNode(&svc_client_list[svc], sock, /* free_data: */TRUE);

	pthread_mutex_unlock(&status_mutex[svc]);
	if (status_sock.count == 0)
		return;
	if (client)
		tmpc = *client;
	client = &tmpc;
	if (client->protocol == NULL)
		client->protocol = "<null>";
	if (client->user == NULL)
		client->user = "<null>";
	sz = offsetof(struct sbbs_status_msg, msg.client_on) + sizeof(msg->msg.client_on) + strlen(client->protocol) + 1 + strlen(client->user) + 1;
	msg = malloc(sz);
	if (msg == NULL)
		return;
	msg->hdr.service = svc;
	msg->hdr.type = STATUS_CLIENT_ON;
	msg->hdr.len = sz;
	msg->msg.client_on.on = on;
	msg->msg.client_on.sock = sock;
	msg->msg.client_on.client = *client;
	msg->msg.client_on.update = update;
	strcpy(msg->msg.client_on.strdata, client->protocol);
	p = strchr(msg->msg.client_on.strdata, 0);
	p++;
	strcpy(p, client->user);
	sendsmsg(msg);
	free(msg);
}

bool status_thread_terminated = false;

void status_thread_terminate(void)
{
	pthread_mutex_lock(&status_thread_mutex);
	status_thread_terminated = true;
	pthread_mutex_unlock(&status_thread_mutex);
}

void status_thread(void *arg)
{
	SOCKET sock;
	struct sockaddr_un addr;
	char *fname = strdup(arg);
	socklen_t addrlen;
	fd_set fd;
	struct timeval tv;
	SOCKET *csock;
	char auth[1024];
	ssize_t len;
	struct sbbs_status_msg *msg;
	size_t mlen;
	int i;
	int nb;
	list_node_t* node;
	struct stat st;

	pthread_once(&init_once, init_lists);
	SetThreadName("status");
	//thread_up(TRUE);
	if (stat(fname, &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFSOCK)
			unlink(fname);
	}
	sock = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sock == INVALID_SOCKET)
		return;
	memset(&addr, 0, sizeof(addr));
	SAFECOPY(addr.sun_path, fname);
	addr.sun_family = AF_UNIX;
#ifdef SUN_LEN
	addrlen = SUN_LEN(&addr);
#else
	addrlen = offsetof(struct sockaddr_un, un_addr.sun_path) + strlen(addr.sun_path) + 1;
#endif
	if (bind(sock, (void *)&addr, addrlen) != 0) {
		closesocket(sock);
		free(fname);
		return;
	}
	if (listen(sock, 2) != 0) {
		closesocket(sock);
		free(fname);
		return;
	}

	pthread_mutex_lock(&status_thread_mutex);
	while (!status_thread_terminated) {
		pthread_mutex_unlock(&status_thread_mutex);
		FD_ZERO(&fd);
		FD_SET(sock, &fd);
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 1;
		if (select(sock+1, &fd, NULL, NULL, &tv) == 1) {
			csock = malloc(sizeof(SOCKET));
			if (csock == NULL)
				break;
			*csock = accept(sock, (void *)&addr, &addrlen);
			if (*csock != INVALID_SOCKET) {
				nb = 1;
				ioctlsocket(*csock, FIONBIO, &nb);
				FD_ZERO(&fd);
				FD_SET(*csock, &fd);
				memset(&tv, 0, sizeof(tv));
				tv.tv_sec = 5;
				if (select((*csock)+1, &fd, NULL, NULL, &tv) == 1) {
					len = recv(*csock, auth, sizeof(auth), 0);
					if (len <= 0) {
						closesocket(*csock);
						free(csock);
						pthread_mutex_lock(&status_thread_mutex);
						continue;
					}
					// TODO: Check auth... "User\0Pass\0SysPass"
					listLock(&status_sock);
					listPushNode(&status_sock, csock);
					for (i=0; i<SERVICE_COUNT; i++) {
						/* SERVICE_EVENT doesn't have status, just logs */
						if (i == SERVICE_EVENT)
							continue;
						pthread_mutex_lock(&status_mutex[i]);
						mlen = offsetof(struct sbbs_status_msg, msg.stats.status) + (status[i] == NULL ? 0 : strlen(status[i])) + 1;
						msg = malloc(mlen);
						if (msg != NULL) {
							msg->hdr.service = i;
							msg->hdr.type = STATUS_STATS;
							msg->hdr.len = mlen;
							msg->msg.stats.error_count = svc_error_count[i];
							msg->msg.stats.served = served[i];
							msg->msg.stats.started = started & (1<<i) ? TRUE : FALSE;
							msg->msg.stats.clients = clients[i];
							msg->msg.stats.threads = threads[i];
							msg->msg.stats.sockets = sockets[i];
							if (status[i] == NULL)
								msg->msg.stats.status[0] = 0;
							else
								strcpy(msg->msg.stats.status, status[i]);
							
							send(*csock, msg, msg->hdr.len, 0);
							free(msg);
						}
						// Now send all the client info...
						mlen = offsetof(struct sbbs_status_msg, msg.client_on) + sizeof(msg->msg.client_on);
						msg = malloc(mlen);
						if (msg != NULL) {
							msg->hdr.service = i;
							msg->hdr.type = STATUS_CLIENT_ON;
							msg->hdr.len = mlen;
							msg->msg.client_on.on = TRUE;
							msg->msg.client_on.update = FALSE;
							listLock(&svc_client_list[i]);
							for (node = svc_client_list[i].first; node != NULL; node = node->next) {
								msg->msg.client_on.sock = node->tag;
								msg->msg.client_on.client = *(client_t*)(node->data);
								send(*csock, msg, msg->hdr.len, 0);
							}
							listUnlock(&svc_client_list[i]);
						}
						pthread_mutex_unlock(&status_mutex[i]);
					}
					// Send current status
					listUnlock(&status_sock);
				}
				else {
					closesocket(*csock);
					free(csock);
					pthread_mutex_lock(&status_thread_mutex);
					continue;
				}
			}
			else {
				free(csock);
				pthread_mutex_lock(&status_thread_mutex);
				continue;
			}
		}
		pthread_mutex_lock(&status_thread_mutex);
	}
	unlink(fname);
	free(fname);
}

#define makestubs(lower, UPPER)                                                                                          \
void status_##lower##_lputs(void *cbdata, int level, const char *str) { status_lputs(SERVICE_##UPPER, level, str); }      \
void status_##lower##_errormsg(void *cbdata, int level, const char *str) { status_errormsg(SERVICE_##UPPER, level, str); } \
void status_##lower##_status(void *cbdata, const char *str) { status_status(SERVICE_##UPPER, str); }                        \
void status_##lower##_started(void *cbdata) { status_started(SERVICE_##UPPER); }                                             \
void status_##lower##_recycle(void *cbdata) { status_recycle(SERVICE_##UPPER); }                                              \
void status_##lower##_terminated(void *cbdata, int code) { status_terminated(SERVICE_##UPPER, code); }                         \
void status_##lower##_clients(void *cbdata, int active) { status_clients(SERVICE_##UPPER, active); }                            \
void status_##lower##_thread_up(void *cbdata, BOOL up, BOOL setuid) { status_thread_up(SERVICE_##UPPER, up, setuid); }           \
void status_##lower##_socket_open(void *cbdata, BOOL open) { status_socket_open(SERVICE_##UPPER, open); }                         \
void status_##lower##_client_on(void *cbdata, BOOL on, SOCKET sock, client_t *client, BOOL update) { status_client_on(SERVICE_##UPPER, on, sock, client, update); }

makestubs(ftp, TERM);
makestubs(mail, MAIL);
makestubs(term, TERM);
makestubs(web, WEB);
makestubs(services, SERVICES);
makestubs(event, EVENT);
