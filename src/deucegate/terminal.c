#include "deucegate.h"

#include <stdlib.h>
#include <string.h>

#include "deucessh-conn.h"

static bool
channel_write_all(dssh_channel channel, const uint8_t *buf, size_t len)
{
	size_t sent = 0;
	while (sent < len) {
		int ev = dssh_chan_poll(channel, DSSH_POLL_WRITE, 5000);
		int64_t n;
		if ((ev & DSSH_POLL_WRITE) == 0)
			return false;
		n = dssh_chan_write(channel, 0, buf + sent, len - sent);
		if (n <= 0)
			return false;
		sent += (size_t)n;
	}
	return true;
}

bool
dg_client_write(dg_client_t *client, const uint8_t *utf8, size_t len)
{
	uint8_t *encoded;
	size_t outlen;
	if (client->client_encoding == DG_UTF8)
		return channel_write_all((dssh_channel)client->channel, utf8, len);
	encoded = malloc(len ? len : 1);
	if (encoded == NULL) return false;
	outlen = dg_encode(DG_CP437, utf8, len, encoded, len);
	bool ok = channel_write_all((dssh_channel)client->channel, encoded, outlen);
	free(encoded);
	return ok;
}

bool
dg_client_puts(dg_client_t *client, const char *utf8)
{
	return dg_client_write(client, (const uint8_t *)utf8, strlen(utf8));
}

int
dg_client_getch(dg_client_t *client, int timeout_ms)
{
	uint8_t ch;
	dssh_channel channel = (dssh_channel)client->channel;
	int ev = dssh_chan_poll(channel, DSSH_POLL_READ | DSSH_POLL_EVENT, timeout_ms);
	if (ev & DSSH_POLL_EVENT) {
		struct dssh_chan_event event;
		while (dssh_chan_read_event(channel, &event) == 0) {
			if (event.type == DSSH_EVENT_WINDOW_CHANGE) {
				client->cols = event.window_change.cols;
				client->rows = event.window_change.rows;
			}
			else if (event.type == DSSH_EVENT_CLOSE || event.type == DSSH_EVENT_EOF)
				return -1;
		}
	}
	if ((ev & DSSH_POLL_READ) == 0)
		return ev < 0 ? -1 : -2;
	return dssh_chan_read(channel, 0, &ch, 1) == 1 ? ch : -1;
}

bool
dg_detect_terminal(dg_client_t *client)
{
	/* This is the same CPR/BOM probe used by Synchronet's answer.cpp. */
	static const uint8_t probe[] =
	    "\r\n\x1b[s\x1b[0c\x1b[255B\x1b[255C\x1b[30;40m\b_\x1b[6n\x1b[u\x1b[!_\r"
	    "\xef\xbb\xbf\x1b[6n\x1b[0m\x1b[2J\x1b[H\x0c\r";
	uint8_t response[2048];
	size_t used = 0;
	unsigned cpr_count = 0, second_col = 999;
	time_t deadline = time(NULL) + 3;
	dssh_channel channel = (dssh_channel)client->channel;
	client->terminal = DG_TERM_ANSI;
	client->client_encoding = DG_CP437;
	response[0] = 0;
	if (!channel_write_all(channel, probe, sizeof(probe) - 1))
		return false;
	while (time(NULL) <= deadline && used < sizeof(response) - 1) {
		int ev = dssh_chan_poll(channel, DSSH_POLL_READ, 250);
		int64_t n;
		if (ev < 0) return false;
		if ((ev & DSSH_POLL_READ) == 0) continue;
		n = dssh_chan_read(channel, 0, response + used, sizeof(response) - used - 1);
		if (n <= 0) break;
		used += (size_t)n;
		response[used] = 0;
		cpr_count = 0;
		second_col = 999;
		for (size_t i = 0; i + 4 < used; i++) {
			unsigned row, col;
			int consumed = 0;
			if (response[i] == 0x1b && response[i + 1] == '[' &&
			    sscanf((char *)response + i + 2, "%u;%uR%n", &row, &col, &consumed) >= 2 && consumed > 0) {
				cpr_count++;
				if (cpr_count == 1) {
					if (row > 0) client->rows = row;
					if (col > 0) client->cols = col;
				}
				else if (cpr_count == 2) {
					second_col = col;
					break;
				}
				i += (size_t)consumed + 1;
			}
		}
		if (cpr_count >= 2) break;
	}
	if (cpr_count == 0)
		client->terminal = DG_TERM_ASCII;
	if (second_col < 3)
		client->client_encoding = DG_UTF8;
	if (strstr((char *)response, "RIPSCRIP") != NULL || strstr((char *)response, "RIP") != NULL)
		client->terminal = DG_TERM_RIP;
	dg_log(DG_LOG_INFO, "node %u terminal=%s encoding=%s size=%ux%u", client->node,
	    client->terminal == DG_TERM_RIP ? "RIP" : client->terminal == DG_TERM_ANSI ? "ANSI" : "ASCII",
	    client->client_encoding == DG_UTF8 ? "UTF-8" : "CP437", client->cols, client->rows);
	return true;
}
