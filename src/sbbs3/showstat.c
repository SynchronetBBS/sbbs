#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "sbbs_status.h"
#include "dirwrap.h"
#include "genwrap.h"

void usage(void) {
	printf("Usage: showstat <path>\n");
}

static struct termios old;
void noecho(void)
{
	struct termios new = old;
	new.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &new);
}

void restore(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

int main(int argc, char **argv)
{
	SOCKET sock;
	struct sockaddr_un addr;
	socklen_t addrlen;
	char buf[4096];
	struct sbbs_status_msg *msg;
	time_t t;
	char *p;

	if (argc != 2) {
		usage();
		return 1;
	}
	if (!fexist(argv[1])) {
		usage();
		return 1;
	}

	tcgetattr(STDIN_FILENO, &old);
	sock = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sock == INVALID_SOCKET)
		return 1;
	memset(&addr, 0, sizeof(addr));
	SAFECOPY(addr.sun_path, argv[1]);
	addr.sun_family = AF_UNIX;
#ifdef SUN_LEN
	addrlen = SUN_LEN(&addr);
#else
	addrlen = offsetof(struct sockaddr_un, un_addr.sun_path) + strlen(addr.sun_path) + 1;
#endif
	printf("UN: ");
	fgets(buf, sizeof(buf), stdin);
	truncnl(buf);
	noecho();
	p = strchr(buf, 0);
	p++;
	printf("PW: ");
	fgets(p, sizeof(buf)-(p-buf), stdin);
	truncnl(p);
	p = strchr(p, 0);
	p++;
	printf("\nSY: ");
	fgets(p, sizeof(buf)-(p-buf), stdin);
	truncnl(p);
	p = strchr(p, 0);
	p++;
	restore();
	puts("");

	if (connect(sock, (void *)&addr, addrlen) != 0) {
		fprintf(stderr, "Unable to connect!\n");
		return 1;
	}

	send(sock, buf, p-buf, 0);
	while (sock != -1) {
		if (recv(sock, buf, sizeof(buf), 0) <= 0) {
			close(sock);
			sock = -1;
			continue;
		}
		msg = (void *)buf;
		switch(msg->hdr.service) {
			case SERVICE_EVENT:
				printf("EVENT: ");
				break;
			case SERVICE_FTP:
				printf("FTP: ");
				break;
			case SERVICE_MAIL:
				printf("MAIL: ");
				break;
			case SERVICE_SERVICES:
				printf("SERVICES: ");
				break;
			case SERVICE_TERM:
				printf("TERM: ");
				break;
			case SERVICE_WEB:
				printf("WEB: ");
				break;
			case SERVICE_STATUS:
				printf("STATUS: ");
				break;
			default:
				printf("UNKNOWN SERVICE %" PRIu32 " ", msg->hdr.service);
				break;
		}
		switch(msg->hdr.type) {
			case STATUS_CLIENTS:
				printf("%d clients\n", msg->msg.clients.active);
				break;
			case STATUS_CLIENT_ON:
				t = msg->msg.client_on.client.time; /* sigh */
				msg->msg.client_on.client.protocol = msg->msg.client_on.strdata;
				msg->msg.client_on.client.user = strchr(msg->msg.client_on.strdata, 0)+1;
				if (msg->msg.client_on.on) {
					printf("Client %s%s: sock: %d\n addr: %s\n host: %s\n port: %" PRIu16 "\n %s at %s via %s\n",
						msg->msg.client_on.on ? "on" : "off",
						msg->msg.client_on.update ? " update" : "",
						msg->msg.client_on.sock,
						msg->msg.client_on.client.addr,
						msg->msg.client_on.client.host,
						msg->msg.client_on.client.port,
						msg->msg.client_on.client.user,
						ctime(&t),
						msg->msg.client_on.client.protocol);
				}
				else {
					printf("Client off: sock: %d\n", msg->msg.client_on.sock);
				}
				break;
			case STATUS_ERRORMSG:
				printf("%d - %s\n", msg->msg.errormsg.level, msg->msg.errormsg.msg);
				break;
			case STATUS_LPUTS:
				printf("%d - %s\n", msg->msg.lputs.level, msg->msg.lputs.msg);
				break;
			case STATUS_RECYCLE:
				printf("recycled\n");
				break;
			case STATUS_SOCKET_OPEN:
				printf("socket %s\n", msg->msg.socket_open.open ? "open" : "close");
				break;
			case STATUS_STARTED:
				printf("started\n");
				break;
			case STATUS_STATS:
				printf("errs: %" PRIu64 ", srv: %" PRIu64 " %s, clients: %d, threads: %d, sockets: %d\n--- %s\n",
					msg->msg.stats.error_count,
					msg->msg.stats.served,
					msg->msg.stats.started ? "running" : "stopped",
					msg->msg.stats.clients,
					msg->msg.stats.threads,
					msg->msg.stats.sockets,
					msg->msg.stats.status);
				break;
			case STATUS_STATUS:
				printf("Status '%s'\n", msg->msg.status.status);
				break;
			case STATUS_TERMINATED:
				printf("Terminated %d\n", msg->msg.terminated.code);
				break;
			case STATUS_THREAD_UP:
				printf("thread %s (%d)\n", msg->msg.thread_up.up ? "up" : "down", msg->msg.thread_up.setuid);
				break;
			default:
				printf("!Unhandled type: %" PRIu32 "\n", msg->hdr.type);
				break;
		}
	}
	return 0;
}
