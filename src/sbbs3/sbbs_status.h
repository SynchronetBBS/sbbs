#ifndef _SBBS_STATUS_
#define _SBBS_STATUS_

#include "client.h"
#include "gen_defs.h"

enum sbbs_status_service {
	SERVICE_FTP,
	SERVICE_MAIL,
	SERVICE_TERM,
	SERVICE_WEB,
	SERVICE_SERVICES,
	SERVICE_EVENT,
	SERVICE_COUNT
};

enum sbbs_status_type {
	STATUS_LPUTS,
	STATUS_ERRORMSG,
	STATUS_STATUS,
	STATUS_STARTED,
	STATUS_RECYCLE,
	STATUS_TERMINATED,
	STATUS_CLIENTS,
	STATUS_THREAD_UP,
	STATUS_SOCKET_OPEN,
	STATUS_CLIENT_ON,
	STATUS_STATS
};

struct sbbs_status_msg_hdr {
	uint32_t	service;
	uint32_t	type;
	size_t		len;
};

struct sbbs_status_msg {
	struct sbbs_status_msg_hdr hdr;
	union {
		struct {
			int	level;
			char	msg[];
		} lputs;
		struct {
			int	level;
			char	msg[];
		} errormsg;
		struct {
			uint8_t	unused;
			char	status[];
		} status;
		struct {
			uint8_t	unused;
		} started;
		struct {
			uint8_t unused;
		} recycle;
		struct {
			int	code;
		} terminated;
		struct {
			int	active;
		} clients;
		struct {
			BOOL	up;
			BOOL	setuid;
		} thread_up;
		struct {
			BOOL	open;
		} socket_open;
		struct {
			BOOL		on;
			SOCKET		sock;
			client_t	client;
			BOOL		update;
			char		strdata[];
		} client_on;
		struct {
			uint64_t	error_count;
			uint64_t	served;
			BOOL		started;
			int		clients;
			int		threads;
			int		sockets;
			char		status[];
		} stats;
	} msg;
};

void status_lputs(enum sbbs_status_service svc, int level, const char *str);
void status_errormsg(enum sbbs_status_service svc, int level, const char *str);
void status_status(enum sbbs_status_service svc, const char *str);
void status_started(enum sbbs_status_service svc);
void status_recycle(enum sbbs_status_service svc);
void status_terminated(enum sbbs_status_service svc, int code);
void status_clients(enum sbbs_status_service svc, int active);
void status_thread_up(enum sbbs_status_service svc, BOOL up, BOOL setuid);
void status_socket_open(enum sbbs_status_service svc, BOOL open);
void status_client_on(enum sbbs_status_service svc, BOOL on, SOCKET sock, client_t *client, BOOL update);
void status_thread_terminate(void);
void status_thread(void *arg);

#define makestubs(lower)                                                \
void status_##lower##_lputs(void *cbdata, int level, const char *str);   \
void status_##lower##_errormsg(void *cbdata, int level, const char *str); \
void status_##lower##_status(void *cbdata, const char *str);               \
void status_##lower##_started(void *cbdata);                                \
void status_##lower##_recycle(void *cbdata);                                 \
void status_##lower##_terminated(void *cbdata, int code);                     \
void status_##lower##_clients(void *cbdata, int active);                       \
void status_##lower##_thread_up(void *cbdata, BOOL up, BOOL setuid);            \
void status_##lower##_socket_open(void *cbdata, BOOL open);                      \
void status_##lower##_client_on(void *cbdata, BOOL on, SOCKET sock, client_t *client, BOOL update);

makestubs(ftp);
makestubs(mail);
makestubs(term);
makestubs(web);
makestubs(services);
makestubs(event);
#undef makestubs

#endif
