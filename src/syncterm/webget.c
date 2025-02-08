#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#ifndef WITHOUT_CRYPTLIB
#include <cryptlib.h>
#endif

#include "bbslist.h"
#include "conn.h"
#include "datewrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "stdio.h"
#include "syncterm.h"
#include "webget.h"
#include "xpprintf.h"

#define MAX_LIST_SIZE (16 * 1024 * 1024)

struct http_cache_info {
	char *etag;
	time_t last_modified;
	time_t date;
	time_t request_time;
	time_t response_time;
	time_t expires;
	double age;
	double max_age;
	bool must_revalidate;
	bool no_cache;
	bool immutable;
};

struct http_session {
	struct webget_request *req;
	const char *hostname;
	const char *path;
	FILE *cache_info;
	struct bbslist hacky_list_entry;
	struct http_cache_info cache;
	SOCKET sock;
#ifndef WITHOUT_CRYPTLIB
	CRYPT_SESSION tls;
#endif
	bool is_tls;
	bool is_chunked;
	bool not_modified;
	bool got_size;
};

static bool
paranoid_strtol(const char *nptr, char ** endptr, int base, long *result)
{
	char *extra_endptr;
	if (endptr == NULL)
		endptr = &extra_endptr;
	errno = 0;
	*result = strtol(nptr, endptr, base);
	if (*result == 0) {
		if (errno)
			return false;
		if (*endptr == nptr)
			return false;
	}
	return true;
}

static bool
paranoid_strtoll(const char *nptr, char ** endptr, int base, long long *result)
{
	char *extra_endptr;
	if (endptr == NULL)
		endptr = &extra_endptr;
	errno = 0;
	*result = strtoll(nptr, endptr, base);
	if (*result == 0) {
		if (errno)
			return false;
		if (*endptr == nptr)
			return false;
	}
	return true;
}

static bool
paranoid_strtoul(const char *nptr, char ** endptr, int base, unsigned long *result)
{
	char *extra_endptr;
	if (endptr == NULL)
		endptr = &extra_endptr;
	errno = 0;
	*result = strtoul(nptr, endptr, base);
	if (*result == 0) {
		if (errno)
			return false;
		if (*endptr == nptr)
			return false;
	}
	return true;
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

static void
set_msgf(struct webget_request *req, const char *newmsgf, ...)
{
	char *newmsg = NULL;
	va_list ap;
	va_start(ap, newmsgf);
	if (vasprintf(&newmsg, newmsgf, ap) >= 0) {
		va_end(ap);
		assert_pthread_mutex_lock(&req->mtx);
		set_msg_locked(req, newmsg);
		assert_pthread_mutex_unlock(&req->mtx);
	}
	else
		va_end(ap);
	free(newmsg);
}

static ssize_t
recv_nbytes(struct http_session *sess, uint8_t *buf, const size_t chunk_size, bool *eof)
{
	ssize_t received = 0;

	// coverity[tainted_data_argument:SUPPRESS]
	while (received < chunk_size) {
		ssize_t rc;
		if (sess->is_tls) {
#ifndef WITHOUT_CRYPTLIB
			int copied = 0;
			int status = cryptPopData(sess->tls, &buf[received], chunk_size - received, &copied);
			if (status == CRYPT_ERROR_COMPLETE) {
				// We're done here...
			}
			else if (cryptStatusError(status)) {
				set_msgf(sess->req, "Error %d Popping Data", status);
				goto error_return;
			}
			rc = copied;
#endif
		}
		else {
			if (!socket_readable(sess->sock, 5000)) {
				set_msg(sess->req, "Socket Unreadable");
				goto error_return;
			}
			// coverity[tainted_data_return:SUPPRESS]
			rc = recv(sess->sock, &buf[received], chunk_size - received, 0);
			if (rc < 0) {
				set_msgf(sess->req, "recv() error %d", SOCKET_ERRNO);
				goto error_return;
			}
		}
		if (rc == 0) {
			if (eof) {
				*eof = true;
				break;
			}
			set_msg(sess->req, "Unexpected End of Data");
			goto error_return;
		}
		received += rc;
		assert_pthread_mutex_lock(&sess->req->mtx);
		sess->req->received_size += rc;
		assert_pthread_mutex_unlock(&sess->req->mtx);
	}

	return received;
error_return:
	if (eof)
		*eof = false;
	return -1;
}

static void
close_socket(struct http_session *sess)
{
	if (sess->sock != INVALID_SOCKET) {
		closesocket(sess->sock);
		sess->sock = INVALID_SOCKET;
	}
}

static void
free_session(struct http_session *sess)
{
#ifndef WITHOUT_CRYPTLIB
	if (sess->is_tls && sess->tls != -1) {
		cryptSetAttribute(sess->tls, CRYPT_SESSINFO_ACTIVE, 0);
		cryptDestroySession(sess->tls);
		sess->tls = -1;
	}
#endif
	close_socket(sess);
	if (sess->cache_info) {
		fclose(sess->cache_info);
		sess->cache_info = NULL;
	}
	free((void *)sess->hostname);
	sess->hostname = NULL;
	free((void *)sess->cache.etag);
	sess->cache.etag = NULL;
}

static char *
gen_inm_header(struct http_session *sess)
{
	if (!sess->cache.etag)
		return NULL;
	char *ret;
	int len = asprintf(&ret, "If-None-Match: \"%s\"\r\n", sess->cache.etag);
	if (len < 1) {
		free(ret);
		return NULL;
	}
	return ret;
}

static const char * const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char * const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static char *
gen_ims_header(struct http_session *sess)
{

	if (sess->cache.last_modified <= 0)
		return NULL;
	struct tm tm;
	if (gmtime_r(&sess->cache.last_modified, &tm) == NULL)
		return NULL;
	char *ret;
	int len = asprintf(&ret, "If-Modified-Since: %s, %02d %s %04d %02d:%02d:%02d GMT\r\n",
	    days[tm.tm_wday], tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (len < 1) {
		free(ret);
		return NULL;
	}
	return ret;
}

static bool
send_request(struct http_session *sess)
{
	char *reqstr = NULL;
	int len;
	char *inm = gen_inm_header(sess);
	char *ims = gen_ims_header(sess);

	len = asprintf(&reqstr, "GET /%s HTTP/1.1\r\n"
	    "Host: %s\r\n%s%s"
	    "Accept-Encoding: \r\n"
	    "User-Agent: %s\r\n"
	    "Connection: close\r\n"
	    "\r\n", sess->path, sess->hostname, inm ? inm : "", ims ? ims : "", syncterm_version);
	free(inm);
	free(ims);
	if (len == -1) {
		set_msgf(sess->req, "asprintf() failure %d", len);
		free(reqstr);
		return false;
	}
	sess->cache.request_time = time(NULL);
	ssize_t sent;
	if (sess->is_tls) {
#ifndef WITHOUT_CRYPTLIB
		int copied;
		int ret = cryptPushData(sess->tls, reqstr, len, &copied);
		if (cryptStatusError(ret)) {
			sent = -1;
		}
		else
			sent = copied;
		ret = cryptFlushData(sess->tls);
		if (cryptStatusError(ret)) {
			sent = -1;
		}
#endif
	}
	else {
		sent = send(sess->sock, reqstr, len, 0);
		shutdown(sess->sock, SHUT_WR);
	}
	free(reqstr);
	if (sent != len) {
		set_msg(sess->req, "send() failure");
		return false;
	}
#ifdef _WIN32
	unsigned long nb = 1;
#else
	int nb = 1;
#endif
	ioctlsocket(sess->sock, FIONBIO, &nb);
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
		bool eof = false;
		ssize_t rc = recv_nbytes(sess, (uint8_t *)&ret[retpos], 1, &eof);
		if (rc == -1) {
			if (SOCKET_ERRNO == EINTR)
				continue;
			goto error_return;
		}
		if (rc == 0 || eof) {
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
	if (retpos == 0)
		ret[retpos] = 0;
	else {
		ret[retpos--] = 0;
		if (crcount) {
			if (ret[retpos] != '\r') {
				set_msg(sess->req, "CR not last character in line.");
				goto error_return;
			}
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

static time_t
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
	if (!paranoid_strtol(&p[5], &end, 10, &l))
		return 0;
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
	if (!paranoid_strtol(&p[12], &end, 10, &l))
		return 0;
	if (*end != ' ')
		return 0;
	if (l < 1900 || l > 5000)
		return 0;
	t.tm_year = l - 1900;
	if (!paranoid_strtol(&p[17], &end, 10, &l))
		return 0;
	if (l < 0 || l > 23)
		return 0;
	if (*end != ':')
		return 0;
	t.tm_hour = l;
	if (!paranoid_strtol(&p[20], &end, 10, &l))
		return 0;
	if (l < 0 || l > 59)
		return 0;
	if (*end != ':')
		return 0;
	t.tm_min = l;
	if (!paranoid_strtol(&p[23], &end, 10, &l))
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
			sess->cache.etag = malloc(i + 1);
			if (sess->cache.etag == NULL)
				return;
			if (i > 0)
				memcpy(sess->cache.etag, val, i);
			sess->cache.etag[i] = 0;
			return;
		}
	}
	return;
}

static const char *
find_end(const char *val, const char ** sep)
{
	bool in_quotes = false;
	bool had_quotes = false;
	bool in_value = false;
	*sep = NULL;
	for (; *val && *val != '\r'; val++) {
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
	while (*val && *val != '\r') {
		val = skipws(val);
		const char *sep;
		const char *end = find_end(val, &sep);
		ptrdiff_t sz;
		if (sep == NULL)
			sz = end - val;
		else
			sz = sep - val;
		// The sep check is for Coverity...
		if (sz == 7 && strnicmp(val, "max-age=", 8) == 0 && sep) {
			long long ll;
			if (paranoid_strtoll(&sep[0], NULL, 10, &ll))
				sess->cache.max_age = ll;
		}
		if (sz == 8 && sep == NULL && strnicmp(val, "no-cache", 8) == 0) {
			sess->cache.no_cache = true;
		}
		if (sz == 8 && sep == NULL && strnicmp(val, "no-store", 8) == 0) {
			// We can only load from the cache!
			set_msg(sess->req, "Not allowed to store");
			return false;
		}
		if (sz == 15 && sep == NULL && strnicmp(val, "must-revalidate", 15) == 0) {
			sess->cache.must_revalidate = true;
		}
		val = end + 1;
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
	switch (l) {
		case 200: // New copy...
			break;
		case 304: //
			sess->not_modified = true;
			break;
		default:
			set_msgf(sess->req, "Unhandled %ld Status", l);
			goto error_return;
	}
	if (*end != ' ') {
		set_msg(sess->req, "Missing space after status");
		goto error_return;
	}
	free(line);
	sess->cache.response_time = time(NULL);

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
			sess->cache.last_modified = parse_http_date(&line[14]);
		}
		else if(strnicmp(line, "date:", 5) == 0) {
			sess->cache.date = parse_http_date(&line[5]);
		}
		else if(strnicmp(line, "expires:", 8) == 0) {
			sess->cache.expires = parse_http_date(&line[8]);
		}
		else if(strnicmp(line, "age:", 4) == 0) {
			if (sess->cache.age == 0) {
				long long ll;
				if (paranoid_strtoll(&line[4], NULL, 10, &ll))
					sess->cache.age = ll;
			}
		}
		else if(strnicmp(line, "cache-control:", 14) == 0) {
			if (!parse_cache_control(sess, &line[14]))
				goto error_return;
		}
		else if(strnicmp(line, "content-length:", 15) == 0) {
			long long ll;
			if (paranoid_strtoll(&line[15], NULL, 10, &ll)) {
				if (ll > MAX_LIST_SIZE) {
					set_msgf(sess->req, "Content Too Large (%lld)", ll);
					goto error_return;
				}
				sess->got_size = true;
				assert_pthread_mutex_lock(&sess->req->mtx);
				sess->req->remote_size = ll;
				assert_pthread_mutex_unlock(&sess->req->mtx);
			}
		}
		else if(strnicmp(line, "content-transfer-encoding:", 26) == 0) {
			const char *val = skipws(&line[26]);
			const char *sep;
			const char *end = find_end(val, &sep);
			if (sep != NULL) {
				set_msgf(sess->req, "Unknown Content-Transfer-Encoding \"%.*s\"", end - val, val);
				goto error_return;
			}
			if ((end - val) != 7) {
				set_msgf(sess->req, "Unknown Content-Transfer-Encoding \"%.*s\"", end - val, val);
				goto error_return;
			}
			if (strnicmp(val, "chunked", 7)) {
				set_msgf(sess->req, "Unknown Content-Transfer-Encoding \"%.*s\"", end - val, val);
				goto error_return;
			}
			sess->is_chunked = true;
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
	const char *p;

	assert_pthread_mutex_lock(&sess->req->mtx);
	if (sess->req->uri == NULL) {
		set_msg_locked(sess->req, "URI is NULL");
		assert_pthread_mutex_unlock(&sess->req->mtx);
		goto error_return;
	}
	if (memcmp(sess->req->uri, "http", 4)) {
		set_msg_locked(sess->req, "URI is not http[s]://");
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
		set_msg_locked(sess->req, "URI is not http[s]://");
		assert_pthread_mutex_unlock(&sess->req->mtx);
		goto error_return;
	}
	p += 3;
	sess->hostname = strdup(p);
	size_t copied = strlcpy(sess->hacky_list_entry.name, sess->req->name, sizeof(sess->hacky_list_entry.name));
	assert(copied <= LIST_NAME_MAX);
	assert_pthread_mutex_unlock(&sess->req->mtx);
	char *slash = strchr(sess->hostname, '/');
	if (slash == NULL) {
		set_msg_locked(sess->req, "No slash after hostname");
		goto error_return;
	}
	*slash = 0;
	sess->path = &slash[1];
	if (strlen(sess->hostname) > LIST_ADDR_MAX) {
		set_msg_locked(sess->req, "Hostname Too Long");
		goto error_return;
	}
	copied = strlcpy(sess->hacky_list_entry.addr, sess->hostname, sizeof(sess->hacky_list_entry.addr));
	assert(copied <= LIST_ADDR_MAX);
	return true;

error_return:
	return false;
}

static bool
open_cacheinfo(struct http_session *sess)
{
	char *path = NULL;

	int len = asprintf(&path, "%s/%s.cacheinfo", sess->req->cache_root, sess->req->name);
	if (len < 0) {
		set_msg(sess->req, "asprintf() failure");
		goto error_return;
	}
	uint64_t end = xp_timer64() + 5000;
	while (sess->cache_info == NULL) {
		sess->cache_info = _fsopen(path, "ab+", SH_DENYRW);
		if (sess->cache_info == NULL) {
			if (xp_timer64() > end) {
				set_msg(sess->req, "Share Timeout");
				goto error_return;
			}
			SLEEP(25+xp_random(50));
		}
	}
	free(path);
	return true;

error_return:
	free(path);
	return false;
}

static bool
parse_cacheinfo(struct http_session *sess)
{
	str_list_t info = iniReadFile(sess->cache_info);
	if (info == NULL) {
		set_msg(sess->req, "Unable to read file");
		fclose(sess->cache_info);
		sess->cache_info = NULL;
		return false;
	}

	char *dstr;
	dstr = iniGetExistingString(info, NULL, "ETag", NULL, NULL);
	if (dstr)
		sess->cache.etag = strdup(dstr);
	sess->cache.last_modified = iniGetDateTime(info, NULL, "LastModified", 0);
	sess->cache.date = iniGetDateTime(info, NULL, "Date", 0);
	sess->cache.request_time = iniGetDateTime(info, NULL, "RequestTime", 0);
	sess->cache.response_time = iniGetDateTime(info, NULL, "ResponseTime", 0);
	sess->cache.expires = iniGetDateTime(info, NULL, "Expires", 0);
	sess->cache.age = iniGetDuration(info, NULL, "Age", 0.0);
	sess->cache.max_age = iniGetDuration(info, NULL, "MaxAge", 0.0);
	sess->cache.must_revalidate = iniGetBool(info, NULL, "MustRevalidate", false);
	sess->cache.no_cache = iniGetBool(info, NULL, "NoCache", false);
	sess->cache.immutable = iniGetBool(info, NULL, "Immutable", false);
	strListFree(&info);
	return true;
}

static bool
write_cacheinfo(struct http_session *sess)
{
	str_list_t info = strListInit();

	if (info == NULL)
		return false;
	if (sess->cache.etag) {
		if (!iniSetString(&info, NULL, "ETag", sess->cache.etag, NULL))
			goto error_return;
	}
	if (sess->cache.last_modified) {
		if (!iniSetDateTime(&info, NULL, "LastModified", true, sess->cache.last_modified, NULL))
			goto error_return;
	}
	if (sess->cache.date) {
		if (!iniSetDateTime(&info, NULL, "Date", true, sess->cache.date, NULL))
			goto error_return;
	}
	if (sess->cache.request_time) {
		if (!iniSetDateTime(&info, NULL, "RequestTime", true, sess->cache.request_time, NULL))
			goto error_return;
	}
	if (sess->cache.response_time) {
		if (!iniSetDateTime(&info, NULL, "ResponseTime", true, sess->cache.response_time, NULL))
			goto error_return;
	}
	if (sess->cache.expires) {
		if (!iniSetDateTime(&info, NULL, "Expires", true, sess->cache.expires, NULL))
			goto error_return;
	}
	if (sess->cache.age) {
		if (!iniSetDuration(&info, NULL, "Age", sess->cache.age, NULL))
			goto error_return;
	}
	if (sess->cache.max_age) {
		if (!iniSetDuration(&info, NULL, "MaxAge", sess->cache.max_age, NULL))
			goto error_return;
	}
	if (sess->cache.must_revalidate) {
		if (!iniSetBool(&info, NULL, "MustRevalidate", sess->cache.must_revalidate, NULL))
			goto error_return;
	}
	if (sess->cache.no_cache) {
		if (!iniSetBool(&info, NULL, "NoCache", sess->cache.no_cache, NULL))
			goto error_return;
	}
	if (sess->cache.immutable) {
		if (!iniSetBool(&info, NULL, "Immutable", sess->cache.immutable, NULL))
			goto error_return;
	}
	fflush(sess->cache_info);
	bool ret;
	if (chsize(fileno(sess->cache_info), 0) != 0) {
		fseek(sess->cache_info, 0, SEEK_SET);
		ret = false;
	}
	else {
		fseek(sess->cache_info, 0, SEEK_SET);
		ret = iniWriteFile(sess->cache_info, info);
	}
	fclose(sess->cache_info);
	sess->cache_info = NULL;
	strListFree(&info);
	return ret;

error_return:
	fclose(sess->cache_info);
	sess->cache_info = NULL;
	strListFree(&info);
	return false;
}

static bool
read_chunked(struct http_session *sess, FILE *out)
{
	char *line;
	size_t bufsz = 0;
	size_t total = 0;
	void *buf = NULL;

	for (;;) {
		line = recv_line(sess, 5000, NULL);
		if (line == NULL)
			goto error_return;
		unsigned long chunk_size;
		if (!paranoid_strtoul(line, NULL, 16, &chunk_size)) {
			set_msgf(sess->req, "strtoul() failure %d", errno);
			goto error_return;
		}
		if (chunk_size == 0)
			break;
		total += chunk_size;
		if (total > MAX_LIST_SIZE)  {
			set_msg(sess->req, "Total Size Too Large");
			goto error_return;
		}
		if (chunk_size > bufsz) {
			void *newbuf = realloc(buf, chunk_size);
			if (newbuf == NULL) {
				set_msg(sess->req, "realloc() failure");
				goto error_return;
			}
			buf = newbuf;
			bufsz = chunk_size;
		}
		if (recv_nbytes(sess, buf, chunk_size, NULL) != chunk_size)
			goto error_return;
		if (fwrite(buf, 1, chunk_size, out) != chunk_size) {
			set_msg(sess->req, "Short fwrite()");
			goto error_return;
		}
	}
	free(buf);
	free(line);
	return true;

error_return:
	free(buf);
	free(line);
	return false;
}

static bool
read_body(struct http_session *sess, FILE *out)
{
	size_t bufsz = 4 * 1024 * 1024;
	uint8_t *buf = malloc(bufsz);
	if (buf == NULL)
		return false;
	size_t received = 0;
	size_t total_size = 0;
	if (sess->got_size) {
		assert_pthread_mutex_lock(&sess->req->mtx);
		total_size = sess->req->remote_size;
		assert_pthread_mutex_unlock(&sess->req->mtx);
	}

	for (;;) {
		if (sess->got_size) {
			size_t remain = total_size - received;
			if (remain < bufsz)
				bufsz = remain;
			ssize_t rb = recv_nbytes(sess, buf, bufsz, NULL);
			if (rb < 0)
				goto error_return;
			if (rb > 0)
				fwrite(buf, 1, rb, out);
			received += rb;
			if (total_size == received)
				break;
		}
		else {
			bool eof = false;
			ssize_t rb = recv_nbytes(sess, buf, bufsz, &eof);
			if (rb < 0)
				goto error_return;
			if (rb > 0)
				fwrite(buf, 1, rb, out);
			if (eof)
				break;
			received += rb;
			if (received >= MAX_LIST_SIZE) {
				set_msg(sess->req, "Total Size Too Large");
				goto error_return;
			}
		}
	}

	free(buf);
	return true;

error_return:
	free(buf);
	return false;
}

static bool
tls_setup(struct http_session *sess)
{
#ifndef WITHOUT_CRYPTLIB
	int status;
	status = cryptCreateSession(&sess->tls, CRYPT_UNUSED, CRYPT_SESSION_SSL);
	if (cryptStatusError(status)) {
		set_msgf(sess->req, "Unable To Create Session (%d)", status);
		goto error_return;
	}
	int off = 1;
	if (setsockopt(sess->sock, IPPROTO_TCP, TCP_NODELAY, (char *)&off, sizeof(off)) == -1) {
		set_msgf(sess->req, "setsockopt() failed (%d)", SOCKET_ERRNO);
		goto error_return;
	}
	status = cryptSetAttribute(sess->tls, CRYPT_SESSINFO_NETWORKSOCKET, sess->sock);
	if (cryptStatusError(status)) {
		set_msgf(sess->req, "Unable To Set Socket (%d)", status);
		goto error_return;
	}
	cryptSetAttribute(sess->tls, CRYPT_OPTION_NET_READTIMEOUT, 5);
	cryptSetAttributeString(sess->tls, CRYPT_SESSINFO_SERVER_NAME, sess->hostname, strlen(sess->hostname));
	status = cryptSetAttribute(sess->tls, CRYPT_SESSINFO_ACTIVE, 1);
	if (cryptStatusError(status)) {
		set_msgf(sess->req, "Unable To Activate Session (%d)", status);
		goto error_return;
	}
	status = cryptSetAttribute(sess->tls, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED);
	if (cryptStatusError(status)) {
		set_msgf(sess->req, "Unable Clear Ownership (%d)", status);
		goto error_return;
	}
	return true;

error_return:
#endif
	return false;
}

static bool
do_request(struct http_session *sess)
{
	char *npath = NULL;
	char *path = NULL;
	FILE *newfile = NULL;
	bool ret;

	set_state(sess->req, "Parsing");
	if (!parse_uri(sess))
		goto error_return;
	set_state(sess->req, "Connecting");
	sess->sock = conn_socket_connect(&sess->hacky_list_entry, false);
	if (sess->sock == INVALID_SOCKET) {
		set_msg(sess->req, "Connection Failed");
		goto error_return;
	}
	if (sess->is_tls) {
		set_state(sess->req, "TLS Setup");
		if (!tls_setup(sess))
			goto error_return;
	}
	set_state(sess->req, "Requesting");
	if (!send_request(sess))
		goto error_return;
	set_state(sess->req, "Parsing headers");
	if (!parse_headers(sess))
		goto error_return;
	if (sess->not_modified)
		goto success_return;

	int len = asprintf(&npath, "%s/%s.new", sess->req->cache_root, sess->req->name);
	if (len < 1) {
		set_msg(sess->req, "asprintf(&npath, ...) error");
		goto error_return;
	}
	newfile = fopen(npath, "wb");
	if (newfile == NULL) {
		set_msg(sess->req, "fopen() error");
		goto error_return;
	}
	if (!sess->not_modified) {
		len = asprintf(&path, "%s/%s.lst", sess->req->cache_root, sess->req->name);
		if (len < 1) {
			set_msg(sess->req, "asprintf(&path, ...) error");
			goto error_return;
		}
		set_state(sess->req, "Reading list");
		if (sess->is_chunked)
			ret = read_chunked(sess, newfile);
		else
			ret = read_body(sess, newfile);
		if (!ret)
			goto error_return;
		fclose(newfile);
		newfile = NULL;
		if (remove(path)) {
			// Ignore error
		}

		if (rename(npath, path) != 0) {
			set_msg(sess->req, "rename(npath, path) error");
			goto error_return;
		}
	}

success_return:
	ret = true;
	goto ret_return;

error_return:
	ret = false;
	goto ret_return;

ret_return:
	free(npath);
	free(path);
	if (newfile)
		fclose(newfile);
	return ret;
}

double dmax(double d1, double d2)
{
	if (d1 > d2)
		return d1;
	return d2;
}

// RFC 9111 Section 4.2
static bool
is_fresh(struct http_session *sess)
{
	if (sess->cache.no_cache)
		return false;
	if (sess->cache.immutable)
		return true;
	double freshness_lifetime;
	time_t date_value = sess->cache.date;
	if (date_value == 0)
		date_value = sess->cache.response_time;
	if (sess->cache.max_age)
		freshness_lifetime = sess->cache.max_age;
	else if (sess->cache.expires != 0)
		freshness_lifetime = difftime(sess->cache.expires, date_value);
	else
		freshness_lifetime = 60 * 60 * 24; // One day

	double apparent_age = dmax(0, sess->cache.response_time - date_value);
	double response_delay = difftime(sess->cache.response_time, sess->cache.request_time);
	double corrected_age_value = sess->cache.age + response_delay;
	double corrected_initial_age = dmax(apparent_age, corrected_age_value);
	double resident_time = difftime(time(NULL), sess->cache.response_time);
	double current_age = corrected_initial_age + resident_time;

	if (current_age < freshness_lifetime)
		return true;
	if (sess->cache.must_revalidate) {
		// Delete stale file
		char *path;
		int len = asprintf(&path, "%s/%s.lst", sess->req->cache_root, sess->req->name);
		if (len > 0) {
			if (remove(path))
				fprintf(stderr, "Failed to remove %s from cache\n", path);
		}
		free(path);
	}
	return false;
}

// TODO: Cache
bool
iniReadHttp(struct webget_request *req)
{
	struct http_session sess = {
		.sock = INVALID_SOCKET,
		.req = req,
#ifndef WITHOUT_CRYPTLIB
		.tls = -1,
#endif
		.hacky_list_entry = {
			.hidepopups = true,
			.address_family = PF_UNSPEC,
			.port = 80,
		},
	};

	if (req == NULL)
		goto error_return;
	set_state(req, "Opening Cache Info");
	if (!open_cacheinfo(&sess))
		goto error_return;
	set_state(req, "Reading Cache Info");
	if (!parse_cacheinfo(&sess))
		goto error_return;
	if (!is_fresh(&sess)) {
		if (!do_request(&sess))
			goto error_return;
	}
	set_state(req, "Writing Cache Info");
	write_cacheinfo(&sess);
	free_session(&sess);
	set_state(req, "Done");
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
#include "uifcinit.h"

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
