/* Native application-side socket adapter for the existing SSH peer fixtures.
 * This file is test code; Winsock never enters the library's public boundary. */
#ifndef DSSH_MOCK_IO_WIN32_H
#define DSSH_MOCK_IO_WIN32_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

static void
mock_win_close(SOCKET socket)
{
	if (socket != INVALID_SOCKET && closesocket(socket) != 0)
		abort();
}

static int
mock_win_error(void)
{
	int error = WSAGetLastError();
	if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT)
		errno = EAGAIN;
	else if (error == WSAEINTR)
		errno = EINTR;
	else if (error == WSAENOTCONN)
		errno = ENOTCONN;
	else
		errno = EIO;
	return -1;
}

/* connect completes into the listener backlog before accept; no worker is
 * required to manufacture the pair. Every socket remains owned until join. */
static int
mock_win_pair(mock_io_handle pair[2])
{
	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKET writer = INVALID_SOCKET;
	SOCKET reader = INVALID_SOCKET;
	struct sockaddr_in address = {0};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	int length = sizeof(address);
	struct sockaddr *native_address = (struct sockaddr *)&address;
	const struct sockaddr *native_const_address = (const struct sockaddr *)&address;
	if (listener == INVALID_SOCKET
	    || bind(listener, native_const_address, sizeof(address)) != 0
	    || getsockname(listener, native_address, &length) != 0
	    || listen(listener, 1) != 0)
		goto fail;
	writer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (writer == INVALID_SOCKET
	    || connect(writer, native_const_address, length) != 0)
		goto fail;
	reader = accept(listener, NULL, NULL);
	if (reader == INVALID_SOCKET)
		goto fail;
	DWORD timeout = 100;
	int no_delay = 1;
	const char *delay_option = (const char *)&no_delay;
	const char *option = (const char *)&timeout;
	SOCKET sockets[] = {writer, reader};
	for (size_t i = 0; i < sizeof(sockets) / sizeof(sockets[0]); i++) {
		if (setsockopt(sockets[i], IPPROTO_TCP, TCP_NODELAY, delay_option, sizeof(no_delay)) != 0
		    || setsockopt(sockets[i], SOL_SOCKET, SO_RCVTIMEO, option, sizeof(timeout)) != 0
		    || setsockopt(sockets[i], SOL_SOCKET, SO_SNDTIMEO, option, sizeof(timeout)) != 0)
			goto fail;
	}
	mock_win_close(listener);
	pair[0] = writer;
	pair[1] = reader;
	return 0;
fail:
	mock_win_close(reader);
	mock_win_close(writer);
	mock_win_close(listener);
	return -1;
}

static intmax_t
mock_native_send(mock_io_handle fd, const uint8_t *data, size_t count)
{
	if (count > INT_MAX)
		return -1;
	int length = (int)count;
	const char *bytes = (const char *)data;
	int result = send(fd, bytes, length, 0);
	return result == SOCKET_ERROR ? mock_win_error() : result;
}

static intmax_t
mock_native_read(mock_io_handle fd, uint8_t *data, size_t count)
{
	if (count > INT_MAX)
		return -1;
	int length = (int)count;
	char *bytes = (char *)data;
	int result = recv(fd, bytes, length, 0);
	return result == SOCKET_ERROR ? mock_win_error() : result;
}

/* Like the POSIX helper, drain is used without a competing reader. select
 * avoids changing the shared socket's blocking mode. */
static intmax_t
mock_native_drain(mock_io_handle fd, uint8_t *data, size_t count)
{
	fd_set ready;
	FD_ZERO(&ready);
	FD_SET(fd, &ready);
	struct timeval timeout = {0};
	int result = select(0, &ready, NULL, NULL, &timeout);
	if (result == SOCKET_ERROR)
		return mock_win_error();
	if (result == 0) {
		errno = EAGAIN;
		return -1;
	}
	return mock_native_read(fd, data, count);
}

static int
mock_native_shutdown(mock_io_handle fd)
{
	return shutdown(fd, SD_BOTH) == SOCKET_ERROR ? mock_win_error() : 0;
}
#endif
