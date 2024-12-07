#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "ssh.h"

int sock = -1;
size_t txbufsz;
size_t txbuflen;
uint8_t *txbuf;
atomic_bool *tx_terminate;
cnd_t tx_cnd;
mtx_t tx_mtx;
struct deuce_ssh_session sess;

int
tx_thread(void *arg)
{
	while ((tx_terminate == NULL || *tx_terminate == false) && (sock != -1)) {
		mtx_lock(&tx_mtx);
		if (txbufsz == 0)
			cnd_wait(&tx_cnd, &tx_mtx);
		mtx_unlock(&tx_mtx);
		struct pollfd fd = {
			.fd = sock,
			.events = POLLOUT | POLLRDHUP,
			.revents = 0,
		};
		if (poll(&fd, 1, 5000) == 1) {
			if (fd.revents & POLLOUT) {
				mtx_lock(&tx_mtx);
				ssize_t sent = send(sock, txbuf, txbuflen, MSG_NOSIGNAL);
				if (sent > 0) {
					memmove(txbuf, &txbuf[sent], txbuflen - sent);
					txbuflen -= sent;
				}
				else if (sent == -1) {
					switch(errno) {
						case EAGAIN:
						case ENOBUFS:
							break;
						default:
							shutdown(sock, SHUT_WR);
							*tx_terminate = true;
							break;
					}
				}
				mtx_unlock(&tx_mtx);
			}
			if (fd.revents & (POLLHUP | POLLRDHUP)) {
				shutdown(sock, SHUT_WR);
				*tx_terminate = true;
			}
		}
	}
	return 0;
}

static int
tx(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata)
{
	mtx_lock(&tx_mtx);
	if (txbuflen + bufsz > txbufsz) {
		uint8_t *nb = realloc(txbuf, txbuflen + bufsz);
		if (nb == NULL) {
			mtx_unlock(&tx_mtx);
			return DEUCE_SSH_ERROR_ALLOC;
		}
		else {
			tx_terminate = terminate;
			txbuf = nb;
			memcpy(&txbuf[txbuflen], buf, bufsz);
			txbufsz = txbuflen + bufsz;
			if (txbuflen == 0)
				cnd_signal(&tx_cnd);
			txbuflen += bufsz;
		}
	}
	mtx_unlock(&tx_mtx);
	return 0;
}

static int
rx(uint8_t *buf, size_t bufsz, atomic_bool *terminate, void *cbdata)
{
	size_t rxlen = 0;

	while ((*terminate == false) && (sock != -1) && rxlen < bufsz) {
		struct pollfd fd = {
			.fd = sock,
			.events = POLLIN | POLLRDHUP,
			.revents = 0,
		};
		if (poll(&fd, 1, 5000) == 1) {
			if (fd.revents & POLLIN) {
				ssize_t received = recv(sock, &buf[rxlen], bufsz - rxlen, MSG_DONTWAIT);
				if (received > 0) {
					rxlen += received;
				}
				else if (received == -1) {
					switch(errno) {
						case EAGAIN:
						case EINTR:
							break;
						default:
							shutdown(sock, SHUT_RD);
							*terminate = true;
							break;
					}
				}
			}
			if (fd.revents & (POLLHUP | POLLRDHUP)) {
				shutdown(sock, SHUT_RD);
				*terminate = true;
			}
		}
	}
	return 0;
}

static int
rxline(uint8_t *buf, size_t bufsz, size_t *bytes_received, atomic_bool *terminate, void *cbdata)
{
	size_t pos = 0;
	bool lastcr = false;
	int ret = 0;

	while (!(*terminate)) {
		int res = rx(&buf[pos], 1, terminate, cbdata);
		if (res < 0)
			return res;
		if (buf[pos] == '\r')
			lastcr = true;
		if (buf[pos] == '\n' && lastcr) {
			*bytes_received = pos + 1;
			return ret;
		}
		if (pos + 1 < bufsz)
			pos++;
		else
			ret = DEUCE_SSH_ERROR_TOOLONG;
	}
	return DEUCE_SSH_ERROR_TERMINATED;
}

static int
extra_line(uint8_t *buf, size_t bufsz, void *cbdata)
{
	fprintf(stdout, "%.*s\n", (int)bufsz, buf);
	return 0;
}


int main(int argc, char **argv)
{
	struct sockaddr_in sa = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET,
		.sin_port = htons(22),
		.sin_addr = htonl(0x7f000001),
	};
	mtx_init(&tx_mtx, mtx_plain);
	cnd_init(&tx_cnd);
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
		return 1;
	
	if (connect(sock, (struct sockaddr *)&sa, sa.sin_len) == -1)
		return 1;
	if (deuce_ssh_transport_set_callbacks(tx, rx, rxline, extra_line) != 0)
		return 1;
	thrd_t thr;
	thrd_create(&thr, tx_thread, NULL);
	deuce_ssh_session_init(&sess);
	int res;
	thrd_join(thr, &res);
}
