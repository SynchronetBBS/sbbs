#include <assert.h>
#include <stdbool.h>

#include "bbslist.h"
#include "conn.h"
#include "sockwrap.h"
#include "stdio.h"
#include "syncterm.h"
#include "uifcinit.h"
#include "webget.h"

struct http_session {
	struct webget_request *req;
	struct bbslist hacky_list_entry;
	const char *hostname;
	const char *path;
	char *etag;
	time_t last_modified;
	time_t date;
	time_t request_time;
	time_t response_time;
	time_t expires;
	double age;
	double max_age;
	SOCKET sock;
	bool must_revalidate;
	bool no_cache;
	bool immutable;
	bool is_tls;
};

static void
close_socket(struct http_session *sess)
{
	if (sess->sock != INVALID_SOCKET) {
		close(sess->sock);
		sess->sock = INVALID_SOCKET;
	}
}

static void
free_session(struct http_session *sess)
{
	close_socket(sess);
	free((void *)sess->hostname);
	sess->hostname = NULL;
	free((void *)sess->etag);
	sess->etag = NULL;
}

static void
set_state_locked(struct webget_request *req, const char *newstate)
{
	free(req->state);
	req->state = strdup(newstate);
}

static void
set_state(struct webget_request *req, const char *newstate)
{
	assert_pthread_mutex_lock(&req->mtx);
	set_state_locked(req, newstate);
	assert_pthread_mutex_unlock(&req->mtx);
}

static void
set_msg_locked(struct webget_request *req, const char *newmsg)
{
	free(req->msg);
	req->msg = strdup(newmsg);
}

static void
set_msg(struct webget_request *req, const char *newmsg)
{
	assert_pthread_mutex_lock(&req->mtx);
	set_msg_locked(req, newmsg);
	assert_pthread_mutex_unlock(&req->mtx);
}

static bool
send_request(struct http_session *sess)
{
	char *reqstr = NULL;
	int len;

	len = asprintf(&reqstr, "GET /%s HTTP/1.1\r\n"
	    "Host: %s\r\n"
	    "Accept-Encoding: \r\n"
	    "User-Agent: %s\r\n"
	    "Connection: close\r\n"
	    "\r\n", sess->path, sess->hostname, syncterm_version);
	if (len == -1) {
		set_msg(sess->req, "asprintf() failure");
		free(reqstr);
		return false;
	}
	sess->request_time = time(NULL);
	ssize_t sent = send(sess->sock, reqstr, len, 0);
	free(reqstr);
	if (sent != len) {
		set_msg(sess->req, "send() failure");
		return false;
	}
	shutdown(sess->sock, SHUT_WR);
	return true;
}

static char *
recv_line(struct http_session *sess, int timeout, size_t *len)
{
	size_t retsz = 1024;
	size_t retpos = 0;
	size_t crcount = 0;
	char *ret = malloc(retsz);

	if (ret == NULL) {
		set_msg(sess->req, "malloc() failure");
		goto error_return;
	}
	for (;;) {
		if (!socket_readable(sess->sock, timeout)) {
			set_msg(sess->req, "Socket not readable.");
			goto error_return;
		}
		ssize_t rc = recv(sess->sock, &ret[retpos], 1, 0);
		if (rc == -1) {
			if (SOCKET_ERRNO == EINTR)
				continue;
			set_msg(sess->req, "recv() failure.");
			goto error_return;
		}
		if (rc == 0) {
			if (retpos == 0) {
				*len = 1;
				goto special_return;
			}
			set_msg(sess->req, "Unexpected EOF.");
			goto error_return;
		}
		if (ret[retpos] == '\r') {
			crcount++;
			if (crcount > 1) {
				set_msg(sess->req, "More than one CR in a line.");
				goto error_return;
			}
		}
		else if (ret[retpos] == '\n')
			break;
		retpos += rc;
		if (retpos == retsz) {
			if (retsz == 1024 * 2 * 2 * 2) {
				set_msg(sess->req, "Line too long.");
				goto error_return;
			}
			retsz *= 2;
			char *newret = realloc(ret, retsz);
			if (newret == NULL) {
				set_msg(sess->req, "realloc() failure.");
				goto error_return;
			}
			ret = newret;
		}
	}
	ret[retpos--] = 0;
	if (crcount) {
		if (ret[retpos] != '\r') {
			set_msg(sess->req, "CR not last character in line.");
			goto error_return;
		}
	}
	if (len)
		*len = retpos;

	return ret;

error_return:
	if (len)
		len = 0;
special_return:
	free(ret);
	return NULL;
}

const char *
skipws(const char *val)
{
	while (*val == ' ' || *val == '\t')
		val++;
	return val;
}

time_t
parse_http_date(const char *dstr)
{
	struct tm t = {0};
	const char *p = dstr;
	long l;
	char *end;

	p = skipws(p);

	if (memcmp(p, "Mon, ", 5) == 0)
		t.tm_wday = 1;
	else if (memcmp(p, "Tue, ", 5) == 0)
		t.tm_wday = 2;
	else if (memcmp(p, "Wed, ", 5) == 0)
		t.tm_wday = 3;
	else if (memcmp(p, "Thu, ", 5) == 0)
		t.tm_wday = 4;
	else if (memcmp(p, "Fri, ", 5) == 0)
		t.tm_wday = 5;
	else if (memcmp(p, "Sat, ", 5) == 0)
		t.tm_wday = 6;
	else if (memcmp(p, "Sun, ", 5) == 0)
		t.tm_wday = 0;
	else
		return 0;
	l = strtol(&p[5], &end, 10);
	if (l < 1 || l > 31)
		return 0;
	if (*end != ' ')
		return 0;
	t.tm_mday = l;
	if (memcmp(&p[8], "Jan ", 4) == 0)
		t.tm_mon = 0;
	else if (memcmp(&p[8], "Feb ", 4) == 0)
		t.tm_mon = 1;
	else if (memcmp(&p[8], "Mar ", 4) == 0)
		t.tm_mon = 2;
	else if (memcmp(&p[8], "Apr ", 4) == 0)
		t.tm_mon = 3;
	else if (memcmp(&p[8], "May ", 4) == 0)
		t.tm_mon = 4;
	else if (memcmp(&p[8], "Jun ", 4) == 0)
		t.tm_mon = 5;
	else if (memcmp(&p[8], "Jul ", 4) == 0)
		t.tm_mon = 6;
	else if (memcmp(&p[8], "Aug ", 4) == 0)
		t.tm_mon = 7;
	else if (memcmp(&p[8], "Sep ", 4) == 0)
		t.tm_mon = 8;
	else if (memcmp(&p[8], "Oct ", 4) == 0)
		t.tm_mon = 9;
	else if (memcmp(&p[8], "Nov ", 4) == 0)
		t.tm_mon = 10;
	else if (memcmp(&p[8], "Dec ", 4) == 0)
		t.tm_mon = 11;
	l = strtol(&p[12], &end, 10);
	if (*end != ' ')
		return 0;
	if (l < 1900 || l > 5000)
		return 0;
	t.tm_year = l - 1900;
	errno = 0;
	l = strtol(&p[17], &end, 10);
	if (l == 0 && errno)
		return 0;
	if (l < 0 || l > 23)
		return 0;
	if (*end != ':')
		return 0;
	t.tm_hour = l;
	errno = 0;
	l = strtol(&p[20], &end, 10);
	if (l == 0 && errno)
		return 0;
	if (l < 0 || l > 59)
		return 0;
	if (*end != ':')
		return 0;
	t.tm_min = l;
	errno = 0;
	l = strtol(&p[20], &end, 10);
	if (l == 0 && errno)
		return 0;
	if (l < 0 || l > 60)
		return 0;
	if (memcmp(end, " GMT", 4) != 0)
		return 0;
	t.tm_sec = l;
	time_t ret = timegm(&t);
	if (ret < 1)
		return 0;
	return ret;
}

static void
parse_etag(struct http_session *sess, const char *val)
{
	val = skipws(val);
	if (*val != '"')
		return;
	val++;
	size_t vlen = strlen(val);
	for (size_t i = 0; i < vlen; i++) {
		if (val[i] == '"') {
			sess->etag = malloc(i + 1);
			if (sess->etag == NULL)
				return;
			if (i > 0)
				memcpy(sess->etag, val, i);
			sess->etag[i] = 0;
			return;
		}
	}
	return;
}

const char *
find_end(const char *val, const char ** sep)
{
	bool in_quotes = false;
	bool had_quotes = false;
	bool in_value = false;
	*sep = NULL;
	for (; *val; val++) {
		if (!in_value) {
			if (*val == '=') {
				*sep = val;
				in_value = true;
			}
			else if (*val == ' ' || *val == '\t' || *val == ',')
				return val;
		}
		else {
			if (!in_quotes) {
				if (*val == '"') {
					in_quotes = true;
				}
				else if (*val == ' ' || *val == '\t' || *val == ',')
					return val;
				if (had_quotes)
					return NULL;
			}
			else {
				if (*val == '\\') {
					if (val[1])
						val++;
					else
						return NULL;
				}
				else if (*val == '"') {
					had_quotes = true;
					in_quotes = false;
				}
			}
		}
	}
	if (in_quotes)
		return NULL;
	return val;
}

static bool
parse_cache_control(struct http_session *sess, const char *val)
{
	while (*val) {
		val = skipws(val);
		const char *sep;
		const char *end = find_end(val, &sep);
		ptrdiff_t sz;
		if (sep == NULL)
			sz = end - val;
		else
			sz = sep - val;
		if (sz == 7 && strnicmp(val, "max-age=", 8) == 0) {
			errno = 0;
			long long ll = strtoll(&sep[0], NULL, 10);
			if (ll != 0 || errno == 0) {
				sess->max_age = ll;
			}
		}
		if (sz == 8 && sep == NULL && strnicmp(val, "no-cache", 8) == 0) {
			sess->no_cache = true;
		}
		if (sz == 8 && sep == NULL && strnicmp(val, "no-store", 8) == 0) {
			// We can only load from the cache!
			set_msg(sess->req, "Not allowed to store");
			return false;
		}
		if (sz == 15 && sep == NULL && strnicmp(val, "must-revalidate", 15) == 0) {
			sess->must_revalidate = true;
		}
	}

	return true;
}

static bool
parse_headers(struct http_session *sess)
{
	char *line = NULL;
	char *end;
	size_t len;
	long l;

	// Status line
	line = recv_line(sess, 5000, &len);
	if (line == NULL)
		goto error_return;
	if (line[len] != '\r') {
		set_msg(sess->req, "Status line must end with CRLF");
		goto error_return;
	}
	if (memcmp(line, "HTTP/1.1 ", 9)) {
		set_msg(sess->req, "protocol not HTTP/1.1");
		goto error_return;
	}
	if (!isdigit(line[9])) {
		set_msg(sess->req, "Status doesn't start on byte 9");
		goto error_return;
	}
	l = strtol(&line[9], &end, 10);
	if (l != 200) {
		set_msg(sess->req, "Status is not 200");
		goto error_return;
	}
	if (*end != ' ') {
		set_msg(sess->req, "Missing space after status");
		goto error_return;
	}
	free(line);
	sess->response_time = time(NULL);

	// Now "parse" headers until we're done...
	for (;;) {
		line = recv_line(sess, 5000, &len);
		if (line == NULL)
			goto error_return;
		if (line[len] != '\r') {
			set_msg(sess->req, "Headers must end with CRLF");
			goto error_return;
		}
		if (line[0] == '\r')
			break;
		if (strchr(line, ':') == NULL && line[0] != ' ' && line[0] != '\t') {
			set_msg(sess->req, "Invalid header");
			goto error_return;
		}
		if (strnicmp(line, "etag:", 5) == 0) {
			parse_etag(sess, &line[5]);
		}
		else if(strnicmp(line, "last-modified:", 14) == 0) {
			sess->last_modified = parse_http_date(&line[14]);
		}
		else if(strnicmp(line, "date:", 5) == 0) {
			sess->date = parse_http_date(&line[5]);
		}
		else if(strnicmp(line, "expires:", 8) == 0) {
			sess->expires = parse_http_date(&line[8]);
		}
		else if(strnicmp(line, "age:", 4) == 0) {
			if (sess->age == 0) {
				errno = 0;
				long long ll = strtoll(&line[4], NULL, 10);
				if (ll > 0 || errno == 0)
					sess->age = ll;
			}
		}
		else if(strnicmp(line, "cache-control:", 14) == 0) {
			if (!parse_cache_control(sess, &line[14]))
				goto error_return;
		}
		free(line);
	}
	free(line);
	return true;

error_return:
	free(line);
	return false;
}

static bool
parse_uri(struct http_session *sess)
{
	char *hostname = NULL;
	char *request;
	const char *p;

	assert_pthread_mutex_lock(&sess->req->mtx);
	if (sess->req->uri == NULL) {
		assert_pthread_mutex_unlock(&sess->req->mtx);
		goto error_return;
	}
	if (memcmp(sess->req->uri, "http", 4)) {
		assert_pthread_mutex_unlock(&sess->req->mtx);
		goto error_return;
	}
	if (sess->req->uri[4] == 's') {
		p = &sess->req->uri[5];
		sess->is_tls = true;
		sess->hacky_list_entry.port = 443;
	}
	else {
		p = &sess->req->uri[4];
	}
	if (memcmp(p, "://", 3)) {
		assert_pthread_mutex_unlock(&sess->req->mtx);
		goto error_return;
	}
	p += 3;
	sess->hostname = strdup(p);
	size_t copied = strlcpy(sess->hacky_list_entry.name, sess->req->name, sizeof(sess->hacky_list_entry.name));
	assert(copied <= LIST_NAME_MAX);
	assert_pthread_mutex_unlock(&sess->req->mtx);
	char *slash = strchr(sess->hostname, '/');
	if (slash == NULL)
		goto error_return;
	*slash = 0;
	sess->path = &slash[1];
	if (strlen(sess->hostname) > LIST_ADDR_MAX)
		goto error_return;
	copied = strlcpy(sess->hacky_list_entry.addr, sess->hostname, sizeof(sess->hacky_list_entry.addr));
	assert(copied <= LIST_ADDR_MAX);
	return true;

error_return:
	return false;
}

// TODO: Cache
bool
iniReadHttp(struct webget_request *req)
{
	bool is_tls = false;
	str_list_t ret = NULL;
	size_t index = 0;
	struct http_session sess = {
		.req = req,
		.hostname = NULL,
		.etag = NULL,
		.hacky_list_entry = {
			.hidepopups = true,
			.address_family = PF_UNSPEC,
			.port = 80,
		},
	};

	if (req == NULL)
		goto error_return;
	set_state(req, "Parsing");
	if (!parse_uri(&sess))
		goto error_return;
	set_state(req, "Connecting");
	sess.sock = conn_socket_connect(&sess.hacky_list_entry);
	if (sess.sock == INVALID_SOCKET)
		goto error_return;
	set_state(req, "Requesting");
	if (!send_request(&sess))
		goto error_return;
	set_state(req, "Parsing headers");
	if (!parse_headers(&sess))
		goto error_return;

	set_state(req, "Reading list");
	ret = strListInit();
	if (ret == NULL)
		goto error_return;

	for (;;) {
		size_t len;
		char *line = recv_line(&sess, 5000, &len);
		if (line == NULL)
			break;
		if (line[len] == '\r')
			line[len] = 0;
		if (strListAnnex(&ret, line, index) == NULL)
			goto error_return;
		index++;
	}
	free_session(&sess);
	return true;

error_return:
	free_session(&sess);
	return false;
}

bool
init_webget_req(struct webget_request *req, const char *cache_root, const char *name, const char *uri)
{
	if (pthread_mutex_init(&req->mtx, NULL) != 0)
		return false;
	req->name = strdup(name);
	if (req->name == NULL) {
		pthread_mutex_destroy(&req->mtx);
		return false;
	}
	req->uri = strdup(uri);
	if (req->uri == NULL) {
		free((void *)req->name);
		req->name = NULL;
		pthread_mutex_destroy(&req->mtx);
		return false;
	}
	req->cache_root = strdup(cache_root);
	if (req->cache_root == NULL) {
		free((void *)req->uri);
		free((void *)req->name);
		req->name = NULL;
		req->uri = NULL;
		pthread_mutex_destroy(&req->mtx);
		return false;
	}
	req->state = NULL;
	req->msg = NULL;
	req->remote_size = 0;
	req->received_size = 0;
	return true;
}

void
destroy_webget_req(struct webget_request *req)
{
	free((void *)req->name);
	req->name = NULL;
	free((void *)req->uri);
	req->uri = NULL;
	free((void *)req->msg);
	req->uri = NULL;
	free((void *)req->state);
	req->uri = NULL;
	pthread_mutex_destroy(&req->mtx);
}

#if 0
int
main(int argc, char **argv)
{
	struct webget_request req;
	uifc.size = sizeof(uifc);
	init_uifc(true, true);
	if (init_webget_req(&req, "/tmp", "Synchronet BBS List", "http://synchro.net/syncterm.lst")) {
		if (iniReadHttp(&req))
			puts("Success!");
		else
			printf("Failure: '%s'\n", req.msg);
		destroy_webget_req(&req);
	}
	return 0;
}
#endif
