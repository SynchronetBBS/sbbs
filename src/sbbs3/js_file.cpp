/* Synchronet JavaScript "File" Object */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "xpendian.h"
#include "md5.h"
#include "base64.h"
#include "uucode.h"
#include "yenc.h"
#include "ini_file.h"

#if !defined(__unix__)
	#include <conio.h>      /* for kbhit() */
#endif

#ifdef JAVASCRIPT

#include "js_request.h"

typedef struct
{
	FILE* fp;
	char name[MAX_PATH + 1];
	char mode[7];
	uchar etx;
	BOOL debug;
	BOOL rot13;
	BOOL yencoded;
	BOOL uuencoded;
	BOOL b64encoded;
	BOOL network_byte_order;
	BOOL pipe;          /* Opened with popen() use pclose() to close */
	ini_style_t ini_style;

} private_t;

static void dbprintf(BOOL error, private_t* p, const char* fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	if (p == NULL || (!p->debug && !error))
		return;

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);

	lprintf(LOG_DEBUG, "%04d File %s%s", p->fp ? fileno(p->fp) : 0, error ? "ERROR: ":"", sbuf);
}

/* Converts fopen() style 'mode' string into open() style 'flags' integer */
static int fopenflags(char *mode)
{
	int flags = 0;

	if (strchr(mode, 'b'))
		flags |= O_BINARY;
	else
		flags |= O_TEXT;

	if (strchr(mode, 'x'))
		flags |= O_EXCL;

	if (strchr(mode, 'w')) {
		flags |= O_CREAT | O_TRUNC;
		if (strchr(mode, '+'))
			flags |= O_RDWR;
		else
			flags |= O_WRONLY;
		return flags;
	}

	if (strchr(mode, 'a')) {
		flags |= O_CREAT | O_APPEND;
		if (strchr(mode, '+'))
			flags |= O_RDWR;
		else
			flags |= O_WRONLY;
		return flags;
	}

	if (strchr(mode, 'r')) {
		if (strchr(mode, '+'))
			flags |= O_RDWR;
		else
			flags |= O_RDONLY;
	}

	return flags;
}

/* File Object Methods */

extern JSClass js_file_class;
static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	BOOL       shareable = FALSE;
	int        file = -1;
	uintN      i;
	jsint      bufsize = 2 * 1024;
	JSString*  str;
	private_t* p;
	jsrefcount rc;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (p->fp != NULL)
		return JS_TRUE;

	SAFECOPY(p->mode, "w+");     /* default mode */
	for (i = 0; i < argc; i++) {
		if (JSVAL_IS_STRING(argv[i])) {  /* mode */
			if ((str = JS_ValueToString(cx, argv[i])) == NULL)
				return JS_FALSE;
			JSSTRING_TO_STRBUF(cx, str, p->mode, sizeof(p->mode), NULL);
		}
		else if (JSVAL_IS_BOOLEAN(argv[i]))  /* shareable */
			shareable = JSVAL_TO_BOOLEAN(argv[i]);
		else if (JSVAL_IS_NUMBER(argv[i])) { /* bufsize */
			if (!JS_ValueToInt32(cx, argv[i], &bufsize))
				return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (shareable)
		p->fp = fopen(p->name, p->mode);
	else {
		if ((file = nopen(p->name, fopenflags(p->mode))) != -1) {
			char *fdomode = strdup(p->mode);
			char *e = fdomode;

			if (fdomode && e) {
				/* Remove deprecated (never-worked, non-standard) 'e'xclusive mode char (and warn): */
				for (e = strchr(fdomode, 'e'); e ; e = strchr(e, 'e')) {
					JS_ReportWarning(cx, "Deprecated file open mode: 'e'");
					memmove(e, e + 1, strlen(e));
				}
				/* Remove (C11 standard) 'x'clusive mode char to avoid MSVC assertion: */
				for (e = strchr(fdomode, 'x'); e ; e = strchr(e, 'x'))
					memmove(e, e + 1, strlen(e));
				if ((p->fp = fdopen(file, fdomode)) == NULL) {
					JS_ReportWarning(cx, "fdopen(%s, %s) ERROR %d: %s", p->name, fdomode, errno, strerror(errno));
				}
			}
			free(fdomode);
		}
	}
	if (p->fp != NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		dbprintf(FALSE, p, "opened: %s", p->name);
		if (!bufsize)
			setvbuf(p->fp, NULL, _IONBF, 0); /* no buffering */
		else {
#ifdef _WIN32
			if (bufsize < 2)
				bufsize = 2;
#endif
			setvbuf(p->fp, NULL, _IOFBF, bufsize);
		}
	} else if (file >= 0)
		close(file);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_popen(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i;
	jsint      bufsize = 2 * 1024;
	JSString*  str;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {

		return JS_FALSE;
	}

	if (p->fp != NULL)
		return JS_TRUE;

	SAFECOPY(p->mode, "r+"); /* default mode */
	for (i = 0; i < argc; i++) {
		if (JSVAL_IS_STRING(argv[i])) {  /* mode */
			if ((str = JS_ValueToString(cx, argv[i])) == NULL)
				return JS_FALSE;
			JSSTRING_TO_STRBUF(cx, str, p->mode, sizeof(p->mode), NULL);
		}
		else if (JSVAL_IS_NUMBER(argv[i])) { /* bufsize */
			if (!JS_ValueToInt32(cx, argv[i], &bufsize))
				return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	p->fp = popen(p->name, p->mode);
	if (p->fp != NULL) {
		p->pipe = TRUE;
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		dbprintf(FALSE, p, "popened: %s", p->name);
		if (!bufsize)
			setvbuf(p->fp, NULL, _IONBF, 0); /* no buffering */
		else
			setvbuf(p->fp, NULL, _IOFBF, bufsize);
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (p->fp == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
#ifdef __unix__
	if (p->pipe)
		pclose(p->fp);
	else
#endif
	fclose(p->fp);

	p->fp = NULL;
	dbprintf(FALSE, p, "closed: %s", p->name);

	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_raw_pollin(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	private_t* p;
	jsrefcount rc;
	int32      timeout = -1;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && !JSVAL_NULL_OR_VOID(argv[0])) {
		if (!JS_ValueToInt32(cx, argv[0], &timeout))
			return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(FALSE));
	rc = JS_SUSPENDREQUEST(cx);
#ifdef __unix__
	/*
	 * TODO: macOS poll() page has the ominous statement that "The poll() system call currently does not support devices."
	 *       But, since we don't support OS X in Synchronet that's likely OK?
	 */
	if (socket_readable(fileno(p->fp), timeout))
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(TRUE));
#else
	while (timeout) {
		if (isatty(fileno(p->fp))) {
			if (kbhit()) {
				JS_RESUMEREQUEST(cx, rc);
				JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(TRUE));
				rc = JS_SUSPENDREQUEST(cx);
				break;
			}
			SLEEP(1);
			if (timeout > 0)
				timeout--;
		}
		else {
			if (!eof(fileno(p->fp))) {
				JS_RESUMEREQUEST(cx, rc);
				JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(TRUE));
				rc = JS_SUSPENDREQUEST(cx);
				break;
			}
			SLEEP(1);
			if (timeout > 0)
				timeout--;
		}
	}
#endif
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_raw_read(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	int32      len;
	JSString*  str;
	private_t* p;
	jsrefcount rc;
	int        fd;
	off_t      pos;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {

		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && !JSVAL_NULL_OR_VOID(argv[0])) {
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	} else
		len = 1;
	if (len < 0)
		len = 1;

	if ((buf = static_cast<char *>(malloc(len))) == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	// https://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_05.html#tag_02_05_01
	/* For the first handle, the first applicable condition below applies. After the actions
	 * required below are taken, if the handle is still open, the application can close it.
	 *   If it is a file descriptor, no action is required.
	 *   If the only further action to be performed on any handle to this open file descriptor
	     *      is to close it, no action need be taken.
	 *   If it is a stream which is unbuffered, no action need be taken.
	 *   If it is a stream which is line buffered, and the last byte written to the stream was a
	 *      <newline> (that is, as if a: putc('\n') was the most recent operation on that stream),
	 *      no action need be taken.
	 *   If it is a stream which is open for writing or appending (but not also open for reading),
	 *      the application shall either perform an fflush(), or the stream shall be closed.
	 *   If the stream is open for reading and it is at the end of the file ( feof() is true), no
	 *      action need be taken.
	 *   If the stream is open with a mode that allows reading and the underlying open file
	 *      description refers to a device that is capable of seeking, the application shall
	 *      either perform an fflush(), or the stream shall be closed.
	 * Otherwise, the result is undefined.
	 * For the second handle:
	 *   If any previous active handle has been used by a function that explicitly changed the file
	 *      offset, except as required above for the first handle, the application shall perform an
	 *      lseek() or fseek() (as appropriate to the type of handle) to an appropriate location.
	 */
	/*
	 * Since we don't want to overcomplicate this, it basically boils down to:
	 * Call fflush() on the stream, lseek() on the descriptor, diddle the descriptor, then fseek() the
	 * stream.
	 *
	 * The only option bit is the fflush() on the stream, but it never hurts and is sometimes
	 * required by POSIX.
	 */
	fflush(p->fp);
	pos = ftello(p->fp);
	if (pos < 0)
		len = 0;
	else {
		fd = fileno(p->fp);
		lseek(fd, pos, SEEK_SET);
		len = read(fileno(p->fp), buf, len);
		fseeko(p->fp, pos + (len >= 0 ? len : 0), SEEK_SET);
		dbprintf(FALSE, p, "read %d raw bytes", len);
		if (len < 0)
			len = 0;
	}
	JS_RESUMEREQUEST(cx, rc);

	str = JS_NewStringCopyN(cx, buf, len);
	free(buf);

	if (str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));

	return JS_TRUE;
}


static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      cp;
	char*      buf;
	char*      uubuf;
	int32      len;
	int32      offset;
	int32      uulen;
	JSString*  str;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && !JSVAL_NULL_OR_VOID(argv[0])) {
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	} else {
		rc = JS_SUSPENDREQUEST(cx);
		len = (long)filelength(fileno(p->fp));
		offset = (long)ftell(p->fp);
		if (offset > 0)
			len -= offset;
		JS_RESUMEREQUEST(cx, rc);
	}
	if (len < 0)
		len = 512;

	if ((buf = static_cast<char *>(malloc(len + 1))) == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	len = fread(buf, 1, len, p->fp);
	dbprintf(FALSE, p, "read %u bytes", len);
	if (len < 0)
		len = 0;
	buf[len] = 0;

	if (p->etx) {
		cp = strchr(buf, p->etx);
		if (cp)
			*cp = 0;
		len = strlen(buf);
	}

	if (p->rot13)
		rot13(buf);

	if (p->uuencoded || p->b64encoded || p->yencoded) {
		uulen = len * 2;
		if ((uubuf = static_cast<char *>(malloc(uulen))) == NULL) {
			free(buf);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;
		}
		if (p->uuencoded)
			uulen = uuencode(uubuf, uulen, buf, len);
		else if (p->yencoded)
			uulen = yencode(uubuf, uulen, buf, len);
		else
			uulen = b64_encode(uubuf, uulen, buf, len);
		if (uulen >= 0) {
			free(buf);
			buf = uubuf;
			len = uulen;
		}
		else
			free(uubuf);
	}
	JS_RESUMEREQUEST(cx, rc);

	str = JS_NewStringCopyN(cx, buf, len);
	free(buf);

	if (str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));

	return JS_TRUE;
}

static JSBool
js_readln(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      cp;
	char*      buf;
	int32      len = 512;
	JSString*  js_str;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && !JSVAL_NULL_OR_VOID(argv[0])) {
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	if ((buf = static_cast<char *>(malloc(len + 1))) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	if (fgets(buf, len + 1, p->fp) != NULL) {
		len = strlen(buf);
		while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n'))
			len--;
		buf[len] = 0;
		if (p->etx) {
			cp = strchr(buf, p->etx);
			if (cp)
				*cp = 0;
		}
		if (p->rot13)
			rot13(buf);
		JS_RESUMEREQUEST(cx, rc);
		if ((js_str = JS_NewStringCopyZ(cx, buf)) != NULL)    /* exception here Feb-12-2005 */
			JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	} else {
		JS_RESUMEREQUEST(cx, rc);
	}
	free(buf);

	return JS_TRUE;
}

static JSBool
js_readbin(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	BYTE *     b;
	WORD *     w;
	DWORD *    l;
	uint64_t * q;
	int32      size = sizeof(DWORD);
	private_t* p;
	int32      count = 1;
	size_t     retlen;
	void *     buffer = NULL;
	size_t     i;
	JSObject*  array;
	jsval      v;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && !JSVAL_NULL_OR_VOID(argv[0])) {
		if (!JS_ValueToInt32(cx, argv[0], &size))
			return JS_FALSE;
		if (argc > 1 && !JSVAL_NULL_OR_VOID(argv[1])) {
			if (!JS_ValueToInt32(cx, argv[1], &count))
				return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (size != sizeof(BYTE) && size != sizeof(WORD) && size != sizeof(DWORD) && size != sizeof(uint64_t)) {
		/* unknown size */
		dbprintf(TRUE, p, "unsupported binary read size: %d", size);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}

	buffer = malloc(size * count);
	if (buffer == NULL) {
		dbprintf(TRUE, p, "malloc failure of %u bytes", size * count);
		JS_RESUMEREQUEST(cx, rc);
		return JS_FALSE;
	}
	b = static_cast<BYTE *>(buffer);
	w = static_cast<WORD *>(buffer);
	l = static_cast<DWORD *>(buffer);
	q = static_cast<uint64_t *>(buffer);
	retlen = fread(buffer, size, count, p->fp);
	if (count == 1) {
		if (retlen == 1) {
			switch (size) {
				case sizeof(BYTE):
					JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(*b));
					break;
				case sizeof(WORD):
					if (p->network_byte_order)
						*w = BE_SHORT(*w);
					else
						*w = LE_SHORT(*w);
					JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(*w));
					break;
				case sizeof(DWORD):
					if (p->network_byte_order)
						*l = BE_LONG(*l);
					else
						*l = LE_LONG(*l);
					JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(*l));
					break;
				case sizeof(uint64_t):
					if (p->network_byte_order)
						*q = BE_INT64(*q);
					else
						*q = LE_INT64(*q);
					JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)*q));
					break;
			}
		}
	}
	else {
		JS_RESUMEREQUEST(cx, rc);
		array = JS_NewArrayObject(cx, 0, NULL);

		for (i = 0; i < retlen; i++) {
			switch (size) {
				case sizeof(BYTE):
					v = INT_TO_JSVAL(*(b++));
					break;
				case sizeof(WORD):
					if (p->network_byte_order)
						*w = BE_SHORT(*w);
					else
						*w = LE_SHORT(*w);
					v = INT_TO_JSVAL(*(w++));
					break;
				case sizeof(DWORD):
					if (p->network_byte_order)
						*l = BE_LONG(*l);
					else
						*l = LE_LONG(*l);
					v = UINT_TO_JSVAL(*(l++));
					break;
				case sizeof(uint64_t):
					if (p->network_byte_order)
						*q = BE_INT64(*q);
					else
						*q = LE_INT64(*q);
					v = DOUBLE_TO_JSVAL((double)*(q++));
					break;
			}
			if (!JS_SetElement(cx, array, i, &v)) {
				rc = JS_SUSPENDREQUEST(cx);
				goto end;
			}
		}
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
	}

end:
	free(buffer);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_readall(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsint      len = 0;
	JSObject*  array;
	private_t* p;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	array = JS_NewArrayObject(cx, 0, NULL);

	while (!feof(p->fp)) {
		js_readln(cx, argc, arglist);
		if (JS_RVAL(cx, arglist) == JSVAL_NULL)
			break;
		if (!JS_SetElement(cx, array, len++, &JS_RVAL(cx, arglist)))
			break;
	}
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static jsval get_value(JSContext *cx, char* value, bool blanks)
{
	char* p;
	BOOL  f = FALSE;
	jsval val;

	if (value == NULL || (*value == 0 && !blanks))
		return JSVAL_VOID;

	/* integer or float? */
	for (p = value; *p; p++) {
		if (*p == '.' && !f)
			f = TRUE;
		else if (!IS_DIGIT(*p))
			break;
	}
	if (p != value && *p == 0) {
		if (f)
			val = DOUBLE_TO_JSVAL(atof(value));
		else
			val = UINT_TO_JSVAL(strtoul(value, NULL, 10));
		return val;
	}
	/* hexadecimal number? */
	if (!strncmp(value, "0x", 2)) {
		for (p = value + 2; *p; p++)
			if (!isxdigit((uchar) * p))
				break;
		if (*p == 0) {
			val = UINT_TO_JSVAL(strtoul(value, NULL, 0));
			return val;
		}
	}
	/* Boolean? */
	if (!stricmp(value, "true"))
		return JSVAL_TRUE;
	if (!stricmp(value, "false"))
		return JSVAL_FALSE;

	/* String */
	return STRING_TO_JSVAL(JS_NewStringCopyZ(cx, value));
}

static double js_DateGetMsecSinceEpoch(JSContext *cx, JSObject *obj)
{
	jsval rval;

	if (!JS_CallFunctionName(cx, obj, "getTime", 0, NULL, &rval)) {
		return ((double)time(NULL)) * 1000;
	}
	return JSVAL_TO_DOUBLE(rval);
}

static JSBool
js_iniGetValue(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      section = ROOT_SECTION;
	char*      key;
	char**     list;
	char       buf[INI_MAX_VALUE_LEN];
	int32      i;
	jsval      val;
	jsval      dflt = argv[2];
	private_t* p;
	JSObject*  array;
	JSObject*  dflt_obj;
	JSObject*  date_obj;
	jsrefcount rc;
	double     dbl;
	time_t     tt;
	char*      cstr = NULL;
	char*      cstr2;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (argc && argv[0] != JSVAL_VOID && argv[0] != JSVAL_NULL)
		JSVALUE_TO_MSTRING(cx, argv[0], section, NULL);
	JSVALUE_TO_MSTRING(cx, argv[1], key, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(section);
		FREE_AND_NULL(key);
		return JS_FALSE;
	}
	/*
	 * Although section can be NULL (ie: root), a NULL key will cause a
	 * segfault.
	 */
	if (key == NULL) {
		JS_ReportError(cx, "Invalid NULL key specified");
		FREE_AND_NULL(section);
		return JS_FALSE;
	}

	str_list_t ini = iniReadFile(p->fp);
	if (argc < 3 || dflt == JSVAL_VOID) {  /* unspecified default value */
		rc = JS_SUSPENDREQUEST(cx);
		cstr = iniGetString(ini, section, key, NULL, buf);
		FREE_AND_NULL(section);
		FREE_AND_NULL(key);
		strListFree(&ini);
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist, get_value(cx, cstr, /* blanks */ false));
		return JS_TRUE;
	}

	if (JSVAL_IS_BOOLEAN(dflt)) {
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(
						iniGetBool(ini, section, key, JSVAL_TO_BOOLEAN(dflt))));
	}
	else if (JSVAL_IS_OBJECT(dflt)) {
		if ((dflt_obj = JSVAL_TO_OBJECT(dflt)) != NULL && (strcmp("Date", JS_GetClass(cx, dflt_obj)->name) == 0)) {
			tt = (time_t)(js_DateGetMsecSinceEpoch(cx, dflt_obj) / 1000.0);
			rc = JS_SUSPENDREQUEST(cx);
			dbl = (double)iniGetDateTime(ini, section, key, tt);
			dbl *= 1000;
			JS_RESUMEREQUEST(cx, rc);
			date_obj = JS_NewDateObjectMsec(cx, dbl);
			if (date_obj != NULL) {
				JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(date_obj));
			}
		}
		else {
			array = JS_NewArrayObject(cx, 0, NULL);
			cstr = NULL;
			JSVALUE_TO_MSTRING(cx, dflt, cstr, NULL);
			if (JS_IsExceptionPending(cx)) {
				FREE_AND_NULL(cstr);
				FREE_AND_NULL(section);
				FREE_AND_NULL(key);
				strListFree(&ini);
				return JS_FALSE;
			}
			rc = JS_SUSPENDREQUEST(cx);
			list = iniGetStringList(ini, section, key, ",", cstr);
			FREE_AND_NULL(cstr);
			JS_RESUMEREQUEST(cx, rc);
			for (i = 0; list && list[i]; i++) {
				val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, list[i]));
				if (!JS_SetElement(cx, array, i, &val))
					break;
			}
			rc = JS_SUSPENDREQUEST(cx);
			iniFreeStringList(list);
			JS_RESUMEREQUEST(cx, rc);
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));
		}
	}
	else if (JSVAL_IS_DOUBLE(dflt)) {
		rc = JS_SUSPENDREQUEST(cx);
		dbl = iniGetFloat(ini, section, key, JSVAL_TO_DOUBLE(dflt));
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL(dbl));
	}
	else if (JSVAL_IS_NUMBER(dflt)) {
		if (!JS_ValueToInt32(cx, dflt, &i)) {
			FREE_AND_NULL(section);
			FREE_AND_NULL(key);
			strListFree(&ini);
			return JS_FALSE;
		}
		rc = JS_SUSPENDREQUEST(cx);
		i = iniGetInteger(ini, section, key, i);
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(i));
	} else {
		cstr = NULL;
		JSVALUE_TO_MSTRING(cx, dflt, cstr, NULL);
		if (JS_IsExceptionPending(cx)) {
			FREE_AND_NULL(cstr);
			FREE_AND_NULL(section);
			FREE_AND_NULL(key);
			strListFree(&ini);
			return JS_FALSE;
		}
		rc = JS_SUSPENDREQUEST(cx);
		cstr2 = iniGetString(ini, section, key, cstr, buf);
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cstr2)));
		FREE_AND_NULL(cstr);
	}
	FREE_AND_NULL(section);
	FREE_AND_NULL(key);
	strListFree(&ini);

	return JS_TRUE;
}

static JSBool
js_iniSetValue_internal(JSContext *cx, JSObject *obj, uintN argc, jsval* argv, str_list_t* list)
{
	char*      section = ROOT_SECTION;
	char*      key = NULL;
	char*      result = NULL;
	int32      i;
	jsval      value = argv[2];
	private_t* p;
	JSObject*  value_obj;
	jsrefcount rc;
	char*      cstr;
	time_t     tt;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && argv[0] != JSVAL_VOID && argv[0] != JSVAL_NULL)
		JSVALUE_TO_MSTRING(cx, argv[0], section, NULL);
	JSVALUE_TO_MSTRING(cx, argv[1], key, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(section);
		FREE_AND_NULL(key);
		return JS_FALSE;
	}

	if (value == JSVAL_VOID) {     /* unspecified value */
		rc = JS_SUSPENDREQUEST(cx);
		result = iniSetString(list, section, key, "", &p->ini_style);
		JS_RESUMEREQUEST(cx, rc);
	}
	else if (JSVAL_IS_BOOLEAN(value)) {
		result = iniSetBool(list, section, key, JSVAL_TO_BOOLEAN(value), &p->ini_style);
	}
	else if (JSVAL_IS_DOUBLE(value)) {
		result = iniSetFloat(list, section, key, JSVAL_TO_DOUBLE(value), &p->ini_style);
	}
	else if (JSVAL_IS_NUMBER(value)) {
		if (!JS_ValueToInt32(cx, value, &i)) {
			FREE_AND_NULL(section);
			FREE_AND_NULL(key);
			return JS_FALSE;
		}
		rc = JS_SUSPENDREQUEST(cx);
		result = iniSetInteger(list, section, key, i, &p->ini_style);
		JS_RESUMEREQUEST(cx, rc);
	} else if (JSVAL_IS_OBJECT(value)
	           && (value_obj = JSVAL_TO_OBJECT(value)) != NULL
	           && (strcmp("Date", JS_GetClass(cx, value_obj)->name) == 0)) {
		tt = (time_t)(js_DateGetMsecSinceEpoch(cx, value_obj) / 1000.0);
		rc = JS_SUSPENDREQUEST(cx);
		result = iniSetDateTime(list, section, key, /* include_time */ TRUE, tt, &p->ini_style);
		JS_RESUMEREQUEST(cx, rc);
	} else {
		cstr = NULL;
		JSVALUE_TO_MSTRING(cx, value, cstr, NULL);
		if (JS_IsExceptionPending(cx)) {
			FREE_AND_NULL(cstr);
			FREE_AND_NULL(section);
			FREE_AND_NULL(key);
			return JS_FALSE;
		}
		rc = JS_SUSPENDREQUEST(cx);
		result = iniSetString(list, section, key, cstr, &p->ini_style);
		FREE_AND_NULL(cstr);
		JS_RESUMEREQUEST(cx, rc);
	}
	FREE_AND_NULL(section);
	FREE_AND_NULL(key);

	return result != NULL;
}

static JSBool
js_iniSetValue(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	jsval      rval = JSVAL_FALSE;
	private_t* p;
	str_list_t list;
	jsrefcount rc;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if ((list = iniReadFile(p->fp)) != NULL) {
		if (js_iniSetValue_internal(cx, obj, argc, argv, &list))
			rval = BOOLEAN_TO_JSVAL(iniWriteFile(p->fp, list));
	}
	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, rval);
	return JS_TRUE;
}

static JSBool
js_iniRemoveKey(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      section = ROOT_SECTION;
	char*      key = NULL;
	private_t* p;
	str_list_t list;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && argv[0] != JSVAL_VOID && argv[0] != JSVAL_NULL) {
		JSVALUE_TO_MSTRING(cx, argv[0], section, NULL);
		HANDLE_PENDING(cx, section);
	}
	JSVALUE_TO_MSTRING(cx, argv[1], key, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(key);
		FREE_AND_NULL(section);
		return JS_FALSE;
	}

	if (key == NULL) {
		JS_ReportError(cx, "Invalid NULL key specified");
		FREE_AND_NULL(section);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((list = iniReadFile(p->fp)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		FREE_AND_NULL(section);
		FREE_AND_NULL(key);
		return JS_TRUE;
	}

	if (iniRemoveKey(&list, section, key))
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(iniWriteFile(p->fp, list)));

	FREE_AND_NULL(section);
	FREE_AND_NULL(key);

	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_iniRemoveSection(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      section = ROOT_SECTION;
	private_t* p;
	str_list_t list;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && argv[0] != JSVAL_VOID && argv[0] != JSVAL_NULL) {
		JSVALUE_TO_MSTRING(cx, argv[0], section, NULL);
		HANDLE_PENDING(cx, section);
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((list = iniReadFile(p->fp)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		FREE_AND_NULL(section);
		return JS_TRUE;
	}

	if (iniRemoveSection(&list, section))
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(iniWriteFile(p->fp, list)));

	FREE_AND_NULL(section);
	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_iniRemoveSections(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      prefix = NULL;
	private_t* p;
	str_list_t list;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && argv[0] != JSVAL_VOID && argv[0] != JSVAL_NULL) {
		JSVALUE_TO_MSTRING(cx, argv[0], prefix, NULL);
		HANDLE_PENDING(cx, prefix);
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((list = iniReadFile(p->fp)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		FREE_AND_NULL(prefix);
		return JS_TRUE;
	}

	if (iniRemoveSections(&list, prefix))
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(iniWriteFile(p->fp, list)));

	FREE_AND_NULL(prefix);
	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_iniGetSections(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      prefix = NULL;
	char**     list;
	jsint      i;
	jsval      val;
	JSObject*  array;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], prefix, NULL);
		HANDLE_PENDING(cx, prefix);
	}

	array = JS_NewArrayObject(cx, 0, NULL);

	rc = JS_SUSPENDREQUEST(cx);
	str_list_t ini = iniReadFile(p->fp);
	list = iniGetSectionList(ini, prefix);
	strListFree(&ini);
	FREE_AND_NULL(prefix);
	JS_RESUMEREQUEST(cx, rc);
	for (i = 0; list && list[i]; i++) {
		val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, list[i]));
		if (!JS_SetElement(cx, array, i, &val))
			break;
	}
	rc = JS_SUSPENDREQUEST(cx);
	iniFreeStringList(list);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static JSBool
js_iniGetKeys(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      section = ROOT_SECTION;
	char**     list;
	jsint      i;
	jsval      val;
	JSObject*  array;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (argc && argv[0] != JSVAL_VOID && argv[0] != JSVAL_NULL) {
		JSVALUE_TO_MSTRING(cx, argv[0], section, NULL);
		HANDLE_PENDING(cx, section);
	}
	array = JS_NewArrayObject(cx, 0, NULL);

	rc = JS_SUSPENDREQUEST(cx);
	str_list_t ini = iniReadFile(p->fp);
	list = iniGetKeyList(ini, section);
	strListFree(&ini);
	FREE_AND_NULL(section);
	JS_RESUMEREQUEST(cx, rc);
	for (i = 0; list && list[i]; i++) {
		val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, list[i]));
		if (!JS_SetElement(cx, array, i, &val))
			break;
	}
	rc = JS_SUSPENDREQUEST(cx);
	iniFreeStringList(list);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static JSBool
js_iniGetObject(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *       obj = JS_THIS_OBJECT(cx, arglist);
	jsval *          argv = JS_ARGV(cx, arglist);
	char*            section = ROOT_SECTION;
	jsint            i;
	JSObject*        object;
	private_t*       p;
	named_string_t** list;
	jsrefcount       rc;
	bool             lowercase = false;
	bool             blanks = false;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	uintN argn = 0;
	if (argc > argn && !JSVAL_IS_BOOLEAN(argv[argn]) && !JSVAL_NULL_OR_VOID(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], section, NULL);
		HANDLE_PENDING(cx, section);
		argn++;
	}
	if (argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		lowercase = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if (argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		blanks = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}

	rc = JS_SUSPENDREQUEST(cx);
	str_list_t ini = iniReadFile(p->fp);
	list = iniGetNamedStringList(ini, section);
	strListFree(&ini);
	FREE_AND_NULL(section);
	JS_RESUMEREQUEST(cx, rc);

	if (list == NULL)
		return JS_TRUE;

	object = JS_NewObject(cx, NULL, NULL, obj);

	for (i = 0; list && list[i]; i++) {
		if (lowercase)
			strlwr(list[i]->name);
		JS_DefineProperty(cx, object, list[i]->name
		                  , get_value(cx, list[i]->value, blanks)
		                  , NULL, NULL, JSPROP_ENUMERATE);

	}
	rc = JS_SUSPENDREQUEST(cx);
	iniFreeNamedStringList(list);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(object));

	return JS_TRUE;
}

static JSBool
js_iniSetObject(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	jsint      i;
	JSObject*  object;
	JSIdArray* id_array;
	jsval      set_argv[3];
	jsval      rval;
	char*      cp;
	private_t* p;
	str_list_t list;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	set_argv[0] = argv[0];    /* section */

	if (!JSVAL_IS_OBJECT(argv[1]) || argv[1] == JSVAL_NULL)
		return JS_TRUE;

	object = JSVAL_TO_OBJECT(argv[1]);

	if (object == NULL || (id_array = JS_Enumerate(cx, object)) == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if ((list = iniReadFile(p->fp)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_DestroyIdArray(cx, id_array);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	rval = JSVAL_TRUE;
	for (i = 0; i < id_array->length; i++)  {
		/* property */
		JS_IdToValue(cx, id_array->vector[i], &set_argv[1]);
		/* value */
		cp = NULL;
		JSVALUE_TO_MSTRING(cx, set_argv[1], cp, NULL);
		if (cp == NULL) {
			JS_DestroyIdArray(cx, id_array);
			JS_ReportError(cx, "Invalid NULL property");
			return JS_FALSE;
		}
		if (JS_IsExceptionPending(cx)) {
			FREE_AND_NULL(cp);
			JS_DestroyIdArray(cx, id_array);
			return JS_FALSE;
		}
		(void)JS_GetProperty(cx, object, cp, &set_argv[2]);
		FREE_AND_NULL(cp);
		if (!js_iniSetValue_internal(cx, obj, 3, set_argv, &list)) {
			rval = JSVAL_FALSE;
			break;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (rval == JSVAL_TRUE)
		rval = BOOLEAN_TO_JSVAL(iniWriteFile(p->fp, list));
	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, rval);

	JS_DestroyIdArray(cx, id_array);

	return JS_TRUE;
}


static JSBool
js_iniGetAllObjects(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *       obj = JS_THIS_OBJECT(cx, arglist);
	jsval *          argv = JS_ARGV(cx, arglist);
	const char *     name_def = "name";
	char*            name = (char *)name_def;
	char*            sec_name;
	char*            prefix = NULL;
	char**           sec_list;
	str_list_t       ini;
	jsint            i, k;
	jsval            val;
	JSObject*        array;
	JSObject*        object;
	private_t*       p;
	named_string_t** key_list;
	jsrefcount       rc;
	bool             lowercase = false;
	bool             blanks = false;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	uintN argn = 0;
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) {
			JS_ReportError(cx, "Invalid name argument");
			return JS_FALSE;
		}
		argn++;
	}
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], prefix, NULL);
		argn++;
	}
	if (argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		lowercase = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}
	if (argc > argn && JSVAL_IS_BOOLEAN(argv[argn])) {
		blanks = JSVAL_TO_BOOLEAN(argv[argn]);
		argn++;
	}

	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(prefix);
		if (name != name_def)
			free(name);
		return JS_FALSE;
	}

	array = JS_NewArrayObject(cx, 0, NULL);

	rc = JS_SUSPENDREQUEST(cx);
	ini = iniReadFile(p->fp);
	sec_list = iniGetSectionList(ini, prefix);
	JS_RESUMEREQUEST(cx, rc);
	for (i = 0; sec_list && sec_list[i]; i++) {
		object = JS_NewObject(cx, NULL, NULL, obj);
		sec_name = sec_list[i];
		if (prefix != NULL)
			sec_name += strlen(prefix);
		if (lowercase)
			strlwr(sec_name);
		JS_DefineProperty(cx, object, name
		                  , STRING_TO_JSVAL(JS_NewStringCopyZ(cx, sec_name))
		                  , NULL, NULL, JSPROP_ENUMERATE);

		rc = JS_SUSPENDREQUEST(cx);
		key_list = iniGetNamedStringList(ini, sec_list[i]);
		JS_RESUMEREQUEST(cx, rc);
		for (k = 0; key_list && key_list[k]; k++) {
			if (lowercase)
				strlwr(key_list[k]->name);
			JS_DefineProperty(cx, object, key_list[k]->name
			                  , get_value(cx, key_list[k]->value, blanks)
			                  , NULL, NULL, JSPROP_ENUMERATE);
		}
		rc = JS_SUSPENDREQUEST(cx);
		iniFreeNamedStringList(key_list);
		JS_RESUMEREQUEST(cx, rc);

		val = OBJECT_TO_JSVAL(object);
		if (!JS_SetElement(cx, array, i, &val))
			break;
	}
	rc = JS_SUSPENDREQUEST(cx);
	FREE_AND_NULL(prefix);
	if (name != name_def)
		free(name);
	iniFreeStringList(sec_list);
	iniFreeStringList(ini);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static JSBool
js_iniSetAllObjects(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *  obj = JS_THIS_OBJECT(cx, arglist);
	jsval *     argv = JS_ARGV(cx, arglist);
	const char *name_def = "name";
	char*       name = (char *)name_def;
	jsuint      i;
	jsint       j;
	jsuint      count;
	JSObject*   array;
	JSObject*   object;
	jsval       oval;
	jsval       set_argv[3];
	JSIdArray*  id_array;
	jsval       rval;
	char*       cp = NULL;
	str_list_t  list;
	jsrefcount  rc;
	private_t*  p;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0]))
		return JS_TRUE;

	array = JSVAL_TO_OBJECT(argv[0]);

	if (array == NULL || !JS_IsArrayObject(cx, array))
		return JS_TRUE;

	if (!JS_GetArrayLength(cx, array, &count))
		return JS_TRUE;

	if (argc > 1) {
		JSVALUE_TO_MSTRING(cx, argv[1], name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) {
			JS_ReportError(cx, "Invalid NULL name property");
			return JS_FALSE;
		}
	}
	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		if (name != name_def)
			free(name);
		return JS_FALSE;
	}

	if (p->fp == NULL) {
		if (name != name_def)
			free(name);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if ((list = iniReadFile(p->fp)) == NULL) {
		JS_RESUMEREQUEST(cx, rc);
		if (name != name_def)
			free(name);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	/* enumerate the array */
	rval = JSVAL_TRUE;
	for (i = 0; i < count && rval == JSVAL_TRUE; i++)  {
		if (!JS_GetElement(cx, array, i, &oval))
			break;
		if (!JSVAL_IS_OBJECT(oval))  /* must be an array of objects */
			break;
		object = JSVAL_TO_OBJECT(oval);
		if (object == NULL || !JS_GetProperty(cx, object, name, &set_argv[0]))
			continue;
		if ((id_array = JS_Enumerate(cx, object)) == NULL) {
			if (name != name_def)
				free(name);
			return JS_TRUE;
		}

		for (j = 0; j < id_array->length; j++)  {
			/* property */
			JS_IdToValue(cx, id_array->vector[j], &set_argv[1]);
			/* check if not name */
			JSVALUE_TO_MSTRING(cx, set_argv[1], cp, NULL);
			if (JS_IsExceptionPending(cx)) {
				FREE_AND_NULL(cp);
				JS_DestroyIdArray(cx, id_array);
				if (name != name_def)
					free(name);
				return JS_FALSE;
			}
			if (cp == NULL)
				continue;
			if (strcmp(cp, name) == 0) {
				FREE_AND_NULL(cp);
				continue;
			}
			/* value */
			if (!JS_GetProperty(cx, object, cp, &set_argv[2])) {
				FREE_AND_NULL(cp);
				continue;
			}
			FREE_AND_NULL(cp);  /* Moved from before JS_GetProperty() call */
			if (!js_iniSetValue_internal(cx, obj, 3, set_argv, &list)) {
				rval = JSVAL_FALSE;
				break;
			}
		}
		JS_DestroyIdArray(cx, id_array);
	}
	if (name != name_def)
		free(name);

	rc = JS_SUSPENDREQUEST(cx);
	if (rval == JSVAL_TRUE)
		rval = BOOLEAN_TO_JSVAL(iniWriteFile(p->fp, list));
	strListFree(&list);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, rval);

	return JS_TRUE;
}

static JSBool
js_iniReadAll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL)
		return JS_FALSE;

	if (p->fp == NULL)
		return JS_TRUE;

	JSObject* array = JS_NewArrayObject(cx, 0, NULL);
	if (array == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	str_list_t list = iniReadFile(p->fp);
	JS_RESUMEREQUEST(cx, rc);
	for (size_t i = 0; list != NULL && list[i] != NULL; i++) {
		JSString* js_str;
		if ((js_str = JS_NewStringCopyZ(cx, list[i])) == NULL)
			break;
		jsval     val = STRING_TO_JSVAL(js_str);
		if (!JS_SetElement(cx, array, i, &val))
			break;
	}
	strListFree(&list);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static JSBool
js_raw_write(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      cp = NULL;
	size_t     len;     /* string length */
	JSString*  str;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if ((str = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_MSTRING(cx, str, cp, &len);
	HANDLE_PENDING(cx, cp);
	if (cp == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if (write(fileno(p->fp), cp, len) == (int)len) {
		free(cp);
		dbprintf(FALSE, p, "wrote %lu raw bytes", len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		free(cp);
		dbprintf(TRUE, p, "raw write of %lu bytes failed", len);
	}

	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      cp = NULL;
	char*      uubuf = NULL;
	size_t     len;     /* string length */
	int        decoded_len;
	size_t     tlen;    /* total length to write (may be greater than len) */
	int32      i;
	JSString*  str;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if ((str = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_MSTRING(cx, str, cp, &len);
	HANDLE_PENDING(cx, cp);
	if (cp == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if ((p->uuencoded || p->b64encoded || p->yencoded)
	    && len && (uubuf = static_cast<char *>(malloc(len))) != NULL) {
		if (p->uuencoded)
			decoded_len = uudecode(uubuf, len, cp, len);
		else if (p->yencoded)
			decoded_len = ydecode(uubuf, len, cp, len);
		else
			decoded_len = b64_decode(uubuf, len, cp, len);
		if (decoded_len < 0) {
			free(uubuf);
			free(cp);
			JS_RESUMEREQUEST(cx, rc);
			return JS_TRUE;
		}
		free(cp);
		cp = uubuf;
		len = decoded_len;
	}

	if (p->rot13)
		rot13(cp);

	JS_RESUMEREQUEST(cx, rc);
	tlen = len;
	if (argc > 1 && !JSVAL_NULL_OR_VOID(argv[1])) {
		if (!JS_ValueToInt32(cx, argv[1], &i)) {
			free(cp);
			return JS_FALSE;
		}
		tlen = i;
		if (len > tlen)
			len = tlen;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (fwrite(cp, 1, len, p->fp) == (size_t)len) {
		free(cp);
		if (tlen > len) {
			len = tlen - len;
			if ((cp = static_cast<char *>(malloc(len))) == NULL) {
				JS_RESUMEREQUEST(cx, rc);
				JS_ReportError(cx, "malloc failure of %u bytes", len);
				return JS_FALSE;
			}
			memset(cp, p->etx, len);
			if (fwrite(cp, 1, len, p->fp) < len) {
				free(cp);
				JS_RESUMEREQUEST(cx, rc);
				return JS_TRUE;
			}
			free(cp);
		}
		dbprintf(FALSE, p, "wrote %lu bytes", (ulong)tlen);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		free(cp);
		dbprintf(TRUE, p, "write of %lu bytes failed", (ulong)len);
	}

	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_writeln_internal(JSContext *cx, JSObject *obj, jsval *arg, jsval *rval)
{
	const char *cp_def = "";
	char*       cp = (char *)cp_def;
	JSString*   str;
	private_t*  p;
	jsrefcount  rc;

	*rval = JSVAL_FALSE;

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (arg) {
		if ((str = JS_ValueToString(cx, *arg)) == NULL) {
			JS_ReportError(cx, "JS_ValueToString failed");
			return JS_FALSE;
		}
		JSSTRING_TO_MSTRING(cx, str, cp, NULL);
		HANDLE_PENDING(cx, cp);
		if (cp == NULL)
			cp = (char *)cp_def;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (p->rot13)
		rot13(cp);

	if (fprintf(p->fp, "%s\n", cp) != 0)
		*rval = JSVAL_TRUE;
	if (cp != cp_def)
		free(cp);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsval *   argv = JS_ARGV(cx, arglist);
	jsval     rval;
	JSBool    ret;

	if (argc) {
		ret = js_writeln_internal(cx, obj, &argv[0], &rval);
	}
	else {
		ret = js_writeln_internal(cx, obj, NULL, &rval);
	}
	JS_SET_RVAL(cx, arglist, rval);

	return ret;
}

static JSBool
js_writebin(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsval *   argv = JS_ARGV(cx, arglist);
	union {
		uint8_t *b;
		uint16_t *w;
		uint32_t *l;
		uint64_t *q;
		int8_t *sb;
		int16_t *sw;
		int32_t *sl;
		int64_t *sq;
	} o;
	size_t     wr = 0;
	int32      size = sizeof(int32_t);
	jsuint     count = 1;
	void *     buffer;
	private_t* p;
	JSObject*  array = NULL;
	jsval      elemval;
	jsdouble   val = 0;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (JSVAL_IS_OBJECT(argv[0]) && !JSVAL_IS_NULL(argv[0])) {
		array = JSVAL_TO_OBJECT(argv[0]);
		if (array != NULL && JS_IsArrayObject(cx, array)) {
			if (!JS_GetArrayLength(cx, array, &count))
				return JS_TRUE;
		}
		else
			array = NULL;
	}
	if (array == NULL) {
		if (!JS_ValueToNumber(cx, argv[0], &val))
			return JS_FALSE;
	}
	if (argc > 1 && !JSVAL_NULL_OR_VOID(argv[1])) {
		if (!JS_ValueToInt32(cx, argv[1], &size))
			return JS_FALSE;
	}
	if (size != sizeof(BYTE) && size != sizeof(WORD) && size != sizeof(DWORD) && size != sizeof(uint64_t)) {
		rc = JS_SUSPENDREQUEST(cx);
		dbprintf(TRUE, p, "unsupported binary write size: %d", size);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	buffer = calloc(size, count);
	if (buffer == NULL) {
		rc = JS_SUSPENDREQUEST(cx);
		dbprintf(TRUE, p, "malloc failure of %u bytes", size * count);
		JS_RESUMEREQUEST(cx, rc);
		return JS_FALSE;
	}
	o.b = static_cast<uint8_t *>(buffer);
	if (array == NULL) {
		switch (size) {
			case sizeof(int8_t):
				if (val < 0)
					*o.sb = (int8_t)val;
				else
					*o.b = (uint8_t)val;
				break;
			case sizeof(int16_t):
				if (val < 0)
					*o.sw = (int16_t)val;
				else
					*o.w = (uint16_t)val;
				if (p->network_byte_order)
					*o.w = BE_SHORT(*o.w);
				else
					*o.w = LE_SHORT(*o.w);
				break;
			case sizeof(int32_t):
				if (val < 0)
					*o.sl = (int32_t)val;
				else
					*o.l = (uint32_t)val;
				if (p->network_byte_order)
					*o.l = BE_LONG(*o.l);
				else
					*o.l = LE_LONG(*o.l);
				break;
			case sizeof(int64_t):
				if (val < 0)
					*o.sq = (int64_t)val;
				else
					*o.q = (uint64_t)val;
				if (p->network_byte_order)
					*o.q = BE_INT64(*o.q);
				else
					*o.q = LE_INT64(*o.q);
				break;
		}
	}
	else {
		for (wr = 0; wr < count; wr++) {
			if (!JS_GetElement(cx, array, wr, &elemval))
				goto end;
			if (!JS_ValueToNumber(cx, elemval, &val))
				goto end;
			switch (size) {
				case sizeof(int8_t):
					if (val < 0)
						*o.sb = (int8_t)val;
					else
						*o.b = (uint8_t)val;
					o.b++;
					break;
				case sizeof(int16_t):
					if (val < 0)
						*o.sw = (int16_t)val;
					else
						*o.w = (uint16_t)val;
					if (p->network_byte_order)
						*o.w = BE_SHORT(*o.w);
					else
						*o.w = LE_SHORT(*o.w);
					o.w++;
					break;
				case sizeof(int32_t):
					if (val < 0)
						*o.sl = (int32_t)val;
					else
						*o.l = (uint32_t)val;
					if (p->network_byte_order)
						*o.l = BE_LONG(*o.l);
					else
						*o.l = LE_LONG(*o.l);
					o.l++;
					break;
				case sizeof(int64_t):
					if (val < 0)
						*o.sq = (int64_t)val;
					else
						*o.q = (uint64_t)val;
					if (p->network_byte_order)
						*o.q = BE_INT64(*o.q);
					else
						*o.q = LE_INT64(*o.q);
					o.q++;
					break;
			}
		}
	}
	rc = JS_SUSPENDREQUEST(cx);
	wr = fwrite(buffer, size, count, p->fp);
	JS_RESUMEREQUEST(cx, rc);
	if (wr == count)
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

end:
	free(buffer);
	return JS_TRUE;
}

static JSBool
js_writeall(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	jsuint     i;
	jsuint     limit;
	JSObject*  array;
	jsval      elemval;
	private_t* p;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if (JSVAL_IS_NULL(argv[0]) || !JSVAL_IS_OBJECT(argv[0]))
		return JS_TRUE;

	array = JSVAL_TO_OBJECT(argv[0]);

	if (array == NULL || !JS_IsArrayObject(cx, array))
		return JS_TRUE;

	if (!JS_GetArrayLength(cx, array, &limit))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	for (i = 0; i < limit; i++) {
		jsval rval;

		if (!JS_GetElement(cx, array, i, &elemval))
			break;
		js_writeln_internal(cx, obj, &elemval, &rval);
		JS_SET_RVAL(cx, arglist, rval);
		if (rval != JSVAL_TRUE)
			break;
	}

	return JS_TRUE;
}

static JSBool
js_lock(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	off_t      offset = 0;
	off_t      len = 0;
	private_t* p;
	jsrefcount rc;
	jsdouble   val;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	/* offset */
	if (argc) {
		if (!JS_ValueToNumber(cx, argv[0], &val))
			return JS_FALSE;
		offset = (off_t)val;
	}

	/* length */
	if (argc > 1) {
		if (!JS_ValueToNumber(cx, argv[1], &val))
			return JS_FALSE;
		len = (off_t)val;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (len == 0)
		len = filelength(fileno(p->fp)) - offset;

	if (lock(fileno(p->fp), offset, len) == 0)
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_unlock(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	off_t      offset = 0;
	off_t      len = 0;
	private_t* p;
	jsrefcount rc;
	jsdouble   val;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	/* offset */
	if (argc) {
		if (!JS_ValueToNumber(cx, argv[0], &val))
			return JS_FALSE;
		offset = (off_t)val;
	}

	/* length */
	if (argc > 1) {
		if (!JS_ValueToNumber(cx, argv[1], &val))
			return JS_FALSE;
		len = (off_t)val;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (len == 0)
		len = filelength(fileno(p->fp)) - offset;

	if (unlock(fileno(p->fp), offset, len) == 0)
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_delete(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp != NULL) {   /* close it if it's open */
		fclose(p->fp);
		p->fp = NULL;
	}

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(remove(p->name) == 0));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_flush(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (p->fp == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	else
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(fflush(p->fp) == 0));
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_rewind(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (p->fp == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	else  {
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		rewind(p->fp);
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_truncate(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	private_t* p;
	int32      len = 0;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (argc && !JSVAL_NULL_OR_VOID(argv[0])) {
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (p->fp != NULL && chsize(fileno(p->fp), len) == 0) {
		fseek(p->fp, len, SEEK_SET);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_clear_error(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (p->fp == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	else  {
		clearerr(p->fp);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_fprintf(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      cp;
	private_t* p;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if ((p = (private_t*)js_GetClassPrivate(cx, obj, &js_file_class)) == NULL) {
		return JS_FALSE;
	}

	if (p->fp == NULL)
		return JS_TRUE;

	if ((cp = js_sprintf(cx, 0, argc, argv)) == NULL) {
		JS_ReportError(cx, "js_sprintf failed");
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(fwrite(cp, 1, strlen(cp), p->fp)));
	JS_RESUMEREQUEST(cx, rc);
	js_sprintf_free(cp);

	return JS_TRUE;
}


/* File Object Properties */
enum {
	FILE_PROP_NAME
	, FILE_PROP_MODE
	, FILE_PROP_ETX
	, FILE_PROP_EXISTS
	, FILE_PROP_DATE
	, FILE_PROP_IS_OPEN
	, FILE_PROP_EOF
	, FILE_PROP_ERROR
	, FILE_PROP_DESCRIPTOR
	, FILE_PROP_DEBUG
	, FILE_PROP_POSITION
	, FILE_PROP_LENGTH
	, FILE_PROP_ATTRIBUTES
	, FILE_PROP_YENCODED
	, FILE_PROP_UUENCODED
	, FILE_PROP_B64ENCODED
	, FILE_PROP_ROT13
	, FILE_PROP_NETWORK_ORDER
	/* dynamically calculated */
	, FILE_PROP_CHKSUM
	, FILE_PROP_CRC16
	, FILE_PROP_CRC32
	, FILE_PROP_MD5_HEX
	, FILE_PROP_MD5_B64
	, FILE_PROP_SHA1_HEX
	, FILE_PROP_SHA1_B64
	/* ini style */
	, FILE_INI_KEY_LEN
	, FILE_INI_KEY_PREFIX
	, FILE_INI_SECTION_SEPARATOR
	, FILE_INI_VALUE_SEPARATOR
	, FILE_INI_BIT_SEPARATOR
	, FILE_INI_LITERAL_SEPARATOR
};

static JSBool js_file_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval      idval;
	int        result;
	int32      i = 0;
	uint32     u = 0;
	jsint      tiny;
	private_t* p;
	jsrefcount rc;
	char*      str = NULL;

	if ((p = (private_t*)JS_GetInstancePrivate(cx, obj, &js_file_class, NULL)) == NULL) {
		return JS_TRUE;
	}

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	rc = JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "setting property %d", tiny);
	JS_RESUMEREQUEST(cx, rc);

	switch (tiny) {
		case FILE_PROP_DEBUG:
			JS_ValueToBoolean(cx, *vp, &(p->debug));
			break;
		case FILE_PROP_YENCODED:
			JS_ValueToBoolean(cx, *vp, &(p->yencoded));
			break;
		case FILE_PROP_UUENCODED:
			JS_ValueToBoolean(cx, *vp, &(p->uuencoded));
			break;
		case FILE_PROP_B64ENCODED:
			JS_ValueToBoolean(cx, *vp, &(p->b64encoded));
			break;
		case FILE_PROP_ROT13:
			JS_ValueToBoolean(cx, *vp, &(p->rot13));
			break;
		case FILE_PROP_NETWORK_ORDER:
			JS_ValueToBoolean(cx, *vp, &(p->network_byte_order));
			break;
		case FILE_PROP_POSITION:
			if (p->fp != NULL) {
				if (!JS_ValueToECMAUint32(cx, *vp, &u))
					return JS_FALSE;
				rc = JS_SUSPENDREQUEST(cx);
				fseek(p->fp, u, SEEK_SET);
				JS_RESUMEREQUEST(cx, rc);
			}
			break;
		case FILE_PROP_DATE:
			if (!JS_ValueToECMAUint32(cx, *vp, &u))
				return JS_FALSE;
			rc = JS_SUSPENDREQUEST(cx);
			setfdate(p->name, u);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case FILE_PROP_LENGTH:
			if (p->fp != NULL) {
				if (!JS_ValueToECMAUint32(cx, *vp, &u))
					return JS_FALSE;
				rc = JS_SUSPENDREQUEST(cx);
				result = chsize(fileno(p->fp), u);
				JS_RESUMEREQUEST(cx, rc);
				if (result != 0) {
					JS_ReportError(cx, "Error %d changing file size", errno);
					return JS_FALSE;
				}
			}
			break;
		case FILE_PROP_ATTRIBUTES:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			rc = JS_SUSPENDREQUEST(cx);
			(void)CHMOD(p->name, i);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case FILE_PROP_ETX:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->etx = (uchar)i;
			break;
		case FILE_INI_KEY_LEN:
			if (!JS_ValueToInt32(cx, *vp, &i))
				return JS_FALSE;
			p->ini_style.key_len = i;
			break;
		case FILE_INI_KEY_PREFIX:
			FREE_AND_NULL(p->ini_style.key_prefix);
			if (!JSVAL_NULL_OR_VOID(*vp)) {
				JSVALUE_TO_MSTRING(cx, *vp, str, NULL);
				HANDLE_PENDING(cx, str);
				p->ini_style.key_prefix = str;
			}
			break;
		case FILE_INI_SECTION_SEPARATOR:
			FREE_AND_NULL(p->ini_style.section_separator);
			if (!JSVAL_NULL_OR_VOID(*vp)) {
				JSVALUE_TO_MSTRING(cx, *vp, str, NULL);
				HANDLE_PENDING(cx, str);
				p->ini_style.section_separator = str;
			}
			break;
		case FILE_INI_VALUE_SEPARATOR:
			FREE_AND_NULL(p->ini_style.value_separator);
			if (!JSVAL_NULL_OR_VOID(*vp)) {
				JSVALUE_TO_MSTRING(cx, *vp, str, NULL);
				HANDLE_PENDING(cx, str);
				p->ini_style.value_separator = str;
			}
			break;
		case FILE_INI_BIT_SEPARATOR:
			FREE_AND_NULL(p->ini_style.bit_separator);
			if (!JSVAL_NULL_OR_VOID(*vp)) {
				JSVALUE_TO_MSTRING(cx, *vp, str, NULL);
				HANDLE_PENDING(cx, str);
				p->ini_style.bit_separator = str;
			}
			break;
		case FILE_INI_LITERAL_SEPARATOR:
			FREE_AND_NULL(p->ini_style.literal_separator);
			if (!JSVAL_NULL_OR_VOID(*vp)) {
				JSVALUE_TO_MSTRING(cx, *vp, str, NULL);
				HANDLE_PENDING(cx, str);
				p->ini_style.literal_separator = str;
			}
			break;
	}

	return JS_TRUE;
}

static JSBool js_file_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval      idval;
	char       str[128];
	char*      s = NULL;
	size_t     i;
	size_t     rd;
	off_t      offset;
	ulong      sum = 0;
	ushort     c16 = 0;
	uint32     c32 = ~0;
	MD5        md5_ctx;
	SHA1_CTX   sha1_ctx;
	BYTE       block[4096];
	BYTE       digest[SHA1_DIGEST_SIZE];
	jsint      tiny;
	JSString*  js_str = NULL;
	private_t* p;
	jsrefcount rc;
	time_t     tt;
	off_t      lng;
	int        in;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_TRUE;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case FILE_PROP_NAME:
			s = p->name;
			break;
		case FILE_PROP_MODE:
			s = p->mode;
			break;
		case FILE_PROP_EXISTS:
			if (p->fp)   /* open? */
				*vp = JSVAL_TRUE;
			else {
				rc = JS_SUSPENDREQUEST(cx);
				*vp = BOOLEAN_TO_JSVAL(fexistcase(p->name));
				JS_RESUMEREQUEST(cx, rc);
			}
			break;
		case FILE_PROP_DATE:
			rc = JS_SUSPENDREQUEST(cx);
			tt = fdate(p->name);
			JS_RESUMEREQUEST(cx, rc);
			*vp = DOUBLE_TO_JSVAL((double)tt);
			break;
		case FILE_PROP_IS_OPEN:
			*vp = BOOLEAN_TO_JSVAL(p->fp != NULL);
			break;
		case FILE_PROP_EOF:
			if (p->fp) {
				rc = JS_SUSPENDREQUEST(cx);
				*vp = BOOLEAN_TO_JSVAL(feof(p->fp) != 0);
				JS_RESUMEREQUEST(cx, rc);
			}
			else
				*vp = JSVAL_TRUE;
			break;
		case FILE_PROP_ERROR:
			if (p->fp) {
				rc = JS_SUSPENDREQUEST(cx);
				*vp = INT_TO_JSVAL(ferror(p->fp));
				JS_RESUMEREQUEST(cx, rc);
			}
			else
				*vp = INT_TO_JSVAL(errno);
			break;
		case FILE_PROP_POSITION:
			if (p->fp) {
				rc = JS_SUSPENDREQUEST(cx);
				lng = ftell(p->fp);
				JS_RESUMEREQUEST(cx, rc);
				*vp = DOUBLE_TO_JSVAL((double)lng);
			}
			else
				*vp = INT_TO_JSVAL(-1);
			break;
		case FILE_PROP_LENGTH:
			rc = JS_SUSPENDREQUEST(cx);
			if (p->fp)   /* open? */
				lng = filelength(fileno(p->fp));
			else
				lng = flength(p->name);
			JS_RESUMEREQUEST(cx, rc);
			*vp = DOUBLE_TO_JSVAL((double)lng);
			break;
		case FILE_PROP_ATTRIBUTES:
			rc = JS_SUSPENDREQUEST(cx);
			in = getfmode(p->name);
			JS_RESUMEREQUEST(cx, rc);
			*vp = INT_TO_JSVAL(in);
			break;
		case FILE_PROP_DEBUG:
			*vp = BOOLEAN_TO_JSVAL(p->debug);
			break;
		case FILE_PROP_YENCODED:
			*vp = BOOLEAN_TO_JSVAL(p->yencoded);
			break;
		case FILE_PROP_UUENCODED:
			*vp = BOOLEAN_TO_JSVAL(p->uuencoded);
			break;
		case FILE_PROP_B64ENCODED:
			*vp = BOOLEAN_TO_JSVAL(p->b64encoded);
			break;
		case FILE_PROP_ROT13:
			*vp = BOOLEAN_TO_JSVAL(p->rot13);
			break;
		case FILE_PROP_NETWORK_ORDER:
			*vp = BOOLEAN_TO_JSVAL(p->network_byte_order);
			break;
		case FILE_PROP_DESCRIPTOR:
			if (p->fp)
				*vp = INT_TO_JSVAL(fileno(p->fp));
			else
				*vp = INT_TO_JSVAL(-1);
			break;
		case FILE_PROP_ETX:
			*vp = INT_TO_JSVAL(p->etx);
			break;
		case FILE_PROP_CHKSUM:
		case FILE_PROP_CRC16:
		case FILE_PROP_CRC32:
			*vp = JSVAL_ZERO;
			if (p->fp == NULL)
				break;
		/* fall-through */
		case FILE_PROP_MD5_HEX:
		case FILE_PROP_MD5_B64:
		case FILE_PROP_SHA1_HEX:
		case FILE_PROP_SHA1_B64:
			*vp = JSVAL_VOID;
			if (p->fp == NULL)
				break;
			rc = JS_SUSPENDREQUEST(cx);
			offset = ftell(p->fp);            /* save current file position */
			fseek(p->fp, 0, SEEK_SET);

			/* Initialization */
			switch (tiny) {
				case FILE_PROP_MD5_HEX:
				case FILE_PROP_MD5_B64:
					MD5_open(&md5_ctx);
					break;
				case FILE_PROP_SHA1_HEX:
				case FILE_PROP_SHA1_B64:
					SHA1Init(&sha1_ctx);
					break;
			}

			/* calculate */
			while (!feof(p->fp)) {
				if ((rd = fread(block, 1, sizeof(block), p->fp)) < 1)
					break;
				switch (tiny) {
					case FILE_PROP_CHKSUM:
						for (i = 0; i < rd; i++)
							sum += block[i];
						break;
					case FILE_PROP_CRC16:
						for (i = 0; i < rd; i++)
							c16 = ucrc16(block[i], c16);
						break;
					case FILE_PROP_CRC32:
						for (i = 0; i < rd; i++)
							c32 = ucrc32(block[i], c32);
						break;
					case FILE_PROP_MD5_HEX:
					case FILE_PROP_MD5_B64:
						MD5_digest(&md5_ctx, block, rd);
						break;
					case FILE_PROP_SHA1_HEX:
					case FILE_PROP_SHA1_B64:
						SHA1Update(&sha1_ctx, block, rd);
						break;
				}
			}
			JS_RESUMEREQUEST(cx, rc);

			/* finalize */
			switch (tiny) {
				case FILE_PROP_CHKSUM:
					*vp = DOUBLE_TO_JSVAL((double)sum);
					break;
				case FILE_PROP_CRC16:
					*vp = UINT_TO_JSVAL(c16);
					break;
				case FILE_PROP_CRC32:
					*vp = UINT_TO_JSVAL(~c32);
					break;
				case FILE_PROP_MD5_HEX:
				case FILE_PROP_MD5_B64:
					MD5_close(&md5_ctx, digest);
					if (tiny == FILE_PROP_MD5_HEX)
						MD5_hex(str, digest);
					else
						b64_encode(str, sizeof(str) - 1, (char *)digest, sizeof(digest));
					js_str = JS_NewStringCopyZ(cx, str);
					break;
				case FILE_PROP_SHA1_HEX:
				case FILE_PROP_SHA1_B64:
					SHA1Final(&sha1_ctx, digest);
					if (tiny == FILE_PROP_SHA1_HEX)
						SHA1_hex(str, digest);
					else
						b64_encode(str, sizeof(str) - 1, (char *)digest, sizeof(digest));
					js_str = JS_NewStringCopyZ(cx, str);
					break;
			}
			rc = JS_SUSPENDREQUEST(cx);
			fseeko(p->fp, offset, SEEK_SET);  /* restore saved file position */
			JS_RESUMEREQUEST(cx, rc);
			if (js_str != NULL)
				*vp = STRING_TO_JSVAL(js_str);
			break;
		case FILE_INI_KEY_LEN:
			*vp = INT_TO_JSVAL(p->ini_style.key_len);
			break;
		case FILE_INI_KEY_PREFIX:
			s = p->ini_style.key_prefix;
			if (s == NULL)
				*vp = JSVAL_NULL;
			break;
		case FILE_INI_SECTION_SEPARATOR:
			s = p->ini_style.section_separator;
			if (s == NULL)
				*vp = JSVAL_NULL;
			break;
		case FILE_INI_VALUE_SEPARATOR:
			s = p->ini_style.value_separator;
			if (s == NULL)
				*vp = JSVAL_NULL;
			break;
		case FILE_INI_BIT_SEPARATOR:
			s = p->ini_style.bit_separator;
			if (s == NULL)
				*vp = JSVAL_NULL;
			break;
		case FILE_INI_LITERAL_SEPARATOR:
			s = p->ini_style.literal_separator;
			if (s == NULL)
				*vp = JSVAL_NULL;
			break;

	}
	if (s != NULL) {
		if ((js_str = JS_NewStringCopyZ(cx, s)) == NULL)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str);
	}

	return JS_TRUE;
}

#define FILE_PROP_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_file_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/
	{   "name", FILE_PROP_NAME, FILE_PROP_FLAGS,   310},
	{   "mode", FILE_PROP_MODE, FILE_PROP_FLAGS,   310},
	{   "exists", FILE_PROP_EXISTS, FILE_PROP_FLAGS,   310},
	{   "is_open", FILE_PROP_IS_OPEN, FILE_PROP_FLAGS,   310},
	{   "eof", FILE_PROP_EOF, FILE_PROP_FLAGS,   310},
	{   "error", FILE_PROP_ERROR, FILE_PROP_FLAGS,   310},
	{   "descriptor", FILE_PROP_DESCRIPTOR, FILE_PROP_FLAGS,   310},
	/* writeable */
	{   "etx", FILE_PROP_ETX, JSPROP_ENUMERATE,  310},
	{   "debug", FILE_PROP_DEBUG, JSPROP_ENUMERATE,  310},
	{   "position", FILE_PROP_POSITION, JSPROP_ENUMERATE,  310},
	{   "date", FILE_PROP_DATE, JSPROP_ENUMERATE,  311},
	{   "length", FILE_PROP_LENGTH, JSPROP_ENUMERATE,  310},
	{   "attributes", FILE_PROP_ATTRIBUTES, JSPROP_ENUMERATE,  310},
	{   "network_byte_order", FILE_PROP_NETWORK_ORDER, JSPROP_ENUMERATE,  311},
	{   "rot13", FILE_PROP_ROT13, JSPROP_ENUMERATE,  311},
	{   "uue", FILE_PROP_UUENCODED, JSPROP_ENUMERATE,  311},
	{   "yenc", FILE_PROP_YENCODED, JSPROP_ENUMERATE,  311},
	{   "base64", FILE_PROP_B64ENCODED, JSPROP_ENUMERATE,  311},
	/* dynamically calculated */
	{   "crc16", FILE_PROP_CRC16, FILE_PROP_FLAGS,   311},
	{   "crc32", FILE_PROP_CRC32, FILE_PROP_FLAGS,   311},
	{   "chksum", FILE_PROP_CHKSUM, FILE_PROP_FLAGS,   311},
	{   "md5_hex", FILE_PROP_MD5_HEX, FILE_PROP_FLAGS,   311},
	{   "md5_base64", FILE_PROP_MD5_B64, FILE_PROP_FLAGS,   311},
	{   "sha1_hex", FILE_PROP_SHA1_HEX, FILE_PROP_FLAGS,   31900},
	{   "sha1_base64", FILE_PROP_SHA1_B64, FILE_PROP_FLAGS,   31900},
	/* ini style elements */
	{   "ini_key_len", FILE_INI_KEY_LEN, JSPROP_ENUMERATE,  317},
	{   "ini_key_prefix", FILE_INI_KEY_PREFIX, JSPROP_ENUMERATE,  317},
	{   "ini_section_separator", FILE_INI_SECTION_SEPARATOR, JSPROP_ENUMERATE,  317},
	{   "ini_value_separator", FILE_INI_VALUE_SEPARATOR, JSPROP_ENUMERATE,  317},
	{   "ini_bit_separator", FILE_INI_BIT_SEPARATOR, JSPROP_ENUMERATE,  317},
	{   "ini_literal_separator", FILE_INI_LITERAL_SEPARATOR, JSPROP_ENUMERATE,  317},
	{0}
};

#ifdef BUILD_JSDOCS
static const char*      file_prop_desc[] = {
	"Filename specified in constructor - <small>READ ONLY</small>"
	, "Mode string specified in <i>open</i> call - <small>READ ONLY</small>"
	, "<i>true</i> if the file is open or exists (case-insensitive) - <small>READ ONLY</small>"
	, "<i>true</i> if the file has been opened successfully - <small>READ ONLY</small>"
	, "<i>true</i> if the current file position is at the <i>end of file</i> - <small>READ ONLY</small>"
	, "The last occurred error value (use <i>clear_error</i> to clear) - <small>READ ONLY</small>"
	, "The open file descriptor (advanced use only) - <small>READ ONLY</small>"
	, "End-of-text character (advanced use only), if non-zero used by <i>read</i>, <i>readln</i>, and <i>write</i>"
	, "Set to <i>true</i> to enable debug log output"
	, "The current file position (offset in bytes), change value to seek within file"
	, "Last modified date/time (in time_t format)"
	, "The current length of the file (in bytes)"
	, "File type/mode flags (i.e. <tt>struct stat.st_mode</tt> value, compatible with <tt>file_chmod()</tt>)"
	, "Set to <i>true</i> if binary data is to be written and read in Network Byte Order (big end first)"
	, "Set to <i>true</i> to enable automatic ROT13 translation of text"
	, "Set to <i>true</i> to enable automatic Unix-to-Unix encode and decode on <tt>read</tt> and <tt>write</tt> calls"
	, "Set to <i>true</i> to enable automatic yEnc encode and decode on <tt>read</tt> and <tt>write</tt> calls"
	, "Set to <i>true</i> to enable automatic Base64 encode and decode on <tt>read</tt> and <tt>write</tt> calls"
	, "Calculated 16-bit CRC of file contents - <small>READ ONLY</small>"
	, "Calculated 32-bit CRC of file contents - <small>READ ONLY</small>"
	, "Calculated 32-bit checksum of file contents - <small>READ ONLY</small>"
	, "Calculated 128-bit MD5 digest of file contents as hexadecimal string - <small>READ ONLY</small>"
	, "Calculated 128-bit MD5 digest of file contents as base64-encoded string - <small>READ ONLY</small>"
	, "Calculated 160-bit SHA1 digest of file contents as hexadecimal string - <small>READ ONLY</small>"
	, "Calculated 160-bit SHA1 digest of file contents as base64-encoded string - <small>READ ONLY</small>"
	, "Ini style: minimum key length (for left-justified white-space padded keys)"
	, "Ini style: key prefix (e.g. '\\t', null = default prefix)"
	, "Ini style: section separator (e.g. '\\n', null = default separator)"
	, "Ini style: value separator (e.g. ' = ', null = default separator)"
	, "Ini style: bit separator (e.g. ' | ', null = default separator)"
	, "Ini style: literal separator (null = default separator)"
	, NULL
};
#endif

static jsSyncMethodSpec js_file_functions[] = {
	{"open",            js_open,            1,  JSTYPE_BOOLEAN, JSDOCSTR("[<i>string</i> mode=\"w+\"] [,<i>bool</i> shareable=false] [,<i>number</i> buffer_length]")
	 , JSDOCSTR("Open file, <i>shareable</i> defaults to <i>false</i>, <i>buffer_length</i> defaults to 2048 bytes, "
		        "mode (default: <tt>'w+'</tt>) specifies the type of access requested for the file, as follows:<br>"
		        "<tt>r&nbsp</tt> open for reading; if the file does not exist or cannot be found, the open call fails<br>"
		        "<tt>w&nbsp</tt> open an empty file for writing; if the given file exists, its contents are destroyed<br>"
		        "<tt>a&nbsp</tt> open for writing at the end of the file (appending); creates the file first if it doesn't exist<br>"
		        "<tt>r+</tt> open for both reading and writing (the file must exist)<br>"
		        "<tt>w+</tt> open an empty file for both reading and writing; if the given file exists, its contents are destroyed<br>"
		        "<tt>a+</tt> open for reading and appending<br>"
		        "<tt>b&nbsp</tt> open in binary (untranslated) mode; translations involving carriage-return and linefeed characters are suppressed (e.g. <tt>r+b</tt>)<br>"
		        "<tt>x&nbsp</tt> open a <i>non-shareable</i> file (that must not already exist) for <i>exclusive</i> access<br>"
		        "<br><b>Note:</b> When using the <tt>iniSet</tt> methods to modify a <tt>.ini</tt> file, "
		        "the file must be opened for both reading <b>and</b> writing.<br>"
		        "<br><b>Note:</b> To open an existing or create a new file for both reading and writing "
		        "(e.g. updating an <tt>.ini</tt> file) "
		        "use the <i>exists</i> property like so:<br>"
		        "<tt>file.open(file.exists ? 'r+':'w+');</tt><br>"
		        "<br><b>Note:</b> When <i>shareable</i> is <tt>false</tt>, uses the Synchronet <tt>nopen()</tt> function which will lock the file "
		        "and perform automatic retries.  The lock mode is as follows:<br>"
		        "<tt>r&nbsp</tt> DENYWRITE - Allows other scripts to open the file for reading, but not for writing.<br>"
		        "<tt>w&nbsp</tt> DENYALL - Does not allow other scripts to open the file when <i>shareable</i> is set to true<br>"
		        "<tt>a&nbsp</tt> DENYALL - Does not allow other scripts to open the file when <i>shareable</i> is set to true<br>"
		        "<tt>r+</tt> DENYALL - Does not allow other scripts to open the file when <i>shareable</i> is set to true<br>"
		        "<tt>w+</tt> DENYALL - Does not allow other scripts to open the file when <i>shareable</i> is set to true<br>"
		        "<tt>a+</tt> DENYALL - Does not allow other scripts to open the file when <i>shareable</i> is set to true<br>"
		        "<br>When <i>shareable</i> is <tt>true</tt> uses the standard C <tt>fopen()</tt> function, "
		        "and will only attempt to open the file once and will perform no locking.<br>"
		        "The behavior when one script has a file opened with <i>shareable</i> set to a different value than is used "
		        "with a new call is OS specific.  On Windows, the second open will always fail and on *nix, "
		        "the second open will always succeed.<br>"
		        )
	 , 310},
	{"popen",           js_popen,           1,  JSTYPE_BOOLEAN, JSDOCSTR("[<i>string</i> mode=\"r+\"] [,<i>number</i> buffer_length]")
	 , JSDOCSTR("Open pipe to command, <i>buffer_length</i> defaults to 2048 bytes, "
		        "mode (default: <tt>'r+'</tt>) specifies the type of access requested for the file, as follows:<br>"
		        "<tt>r&nbsp</tt> read the programs stdout;<br>"
		        "<tt>w&nbsp</tt> write to the programs stdin<br>"
		        "<tt>r+</tt> open for both reading stdout and writing stdin<br>"
		        "(<b>only functional on UNIX systems</b>)"
		        )
	 , 315},
	{"close",           js_close,           0,  JSTYPE_VOID,    JSDOCSTR("")
	 , JSDOCSTR("Close file")
	 , 310},
	{"remove",          js_delete,          0,  JSTYPE_BOOLEAN, JSDOCSTR("")
	 , JSDOCSTR("Remove the file from the disk")
	 , 310},
	{"clearError",      js_clear_error,     0,  JSTYPE_ALIAS },
	{"clear_error",     js_clear_error,     0,  JSTYPE_BOOLEAN, JSDOCSTR("")
	 , JSDOCSTR("Clears the current error value (AKA clearError)")
	 , 310},
	{"flush",           js_flush,           0,  JSTYPE_BOOLEAN, JSDOCSTR("")
	 , JSDOCSTR("Flush/commit buffers to disk")
	 , 310},
	{"rewind",          js_rewind,          0,  JSTYPE_BOOLEAN, JSDOCSTR("")
	 , JSDOCSTR("Repositions the file pointer (<i>position</i>) to the beginning of a file "
		        "and clears error and end-of-file indicators")
	 , 311},
	{"truncate",        js_truncate,        0,  JSTYPE_BOOLEAN, JSDOCSTR("[length=0]")
	 , JSDOCSTR("Changes the file <i>length</i> (default: 0) and repositions the file pointer "
		        "(<i>position</i>) to the new end-of-file")
	 , 314},
	{"lock",            js_lock,            2,  JSTYPE_BOOLEAN, JSDOCSTR("[offset=0] [,length=<i>file_length</i>-<i>offset</i>]")
	 , JSDOCSTR("Lock file record for exclusive access (file must be opened <i>shareable</i>)")
	 , 310},
	{"unlock",          js_unlock,          2,  JSTYPE_BOOLEAN, JSDOCSTR("[offset=0] [,length=<i>file_length</i>-<i>offset</i>]")
	 , JSDOCSTR("Unlock file record for exclusive access")
	 , 310},
	{"read",            js_read,            0,  JSTYPE_STRING,  JSDOCSTR("[maxlen=<i>file_length</i>-<i>file_position</i>]")
	 , JSDOCSTR("Read a string from file (optionally unix-to-unix or base64 encoding in the process), "
		        "<i>maxlen</i> defaults to the current length of the file minus the current file position")
	 , 310},
	{"readln",          js_readln,          0,  JSTYPE_STRING,  JSDOCSTR("[maxlen=512]")
	 , JSDOCSTR("Read a line-feed terminated string, <i>maxlen</i> defaults to 512 characters. "
		        "Returns <i>null</i> upon end of file.")
	 , 310},
	{"readBin",         js_readbin,         0,  JSTYPE_NUMBER,  JSDOCSTR("[bytes=4 [,count=1]]")
	 , JSDOCSTR("Read one or more binary integers from the file, default number of <i>bytes</i> is 4 (32-bits). "
		        "if count is not equal to 1, an array is returned (even if no integers were read)")
	 , 310},
	{"readAll",         js_readall,         0,  JSTYPE_ARRAY,   JSDOCSTR("[maxlen=512]")
	 , JSDOCSTR("Read all lines into an array of strings, <i>maxlen</i> defaults to 512 characters")
	 , 310},
	{"raw_read",        js_raw_read,        0,  JSTYPE_STRING,  JSDOCSTR("[maxlen=1]")
	 , JSDOCSTR("Read a string from underlying file descriptor. "
		        "Undefined results when mixed with any other read/write methods except raw_write, including indirect ones. "
		        "<i>maxlen</i> defaults to one")
	 , 317},
	{"raw_pollin",      js_raw_pollin,      0,  JSTYPE_BOOLEAN, JSDOCSTR("[timeout]")
	 , JSDOCSTR("Waits up to <i>timeout</i> milliseconds (or forever if timeout is not specified) for data to be available "
		        "via raw_read().")
	 , 317},
	{"write",           js_write,           1,  JSTYPE_BOOLEAN, JSDOCSTR("text [,length=<i>text_length</i>]")
	 , JSDOCSTR("Write a string to the file (optionally unix-to-unix or base64 decoding in the process). "
		        "If the specified <i>length</i> is longer than the <i>text</i>, the remaining length will be written as NUL bytes.")
	 , 310},
	{"writeln",         js_writeln,         0,  JSTYPE_BOOLEAN, JSDOCSTR("[text]")
	 , JSDOCSTR("Write a new-line terminated string (a line of text) to the file")
	 , 310},
	{"writeBin",        js_writebin,        1,  JSTYPE_BOOLEAN, JSDOCSTR("value(s) [,bytes=4]")
	 , JSDOCSTR("Write one or more binary integers to the file, default number of <i>bytes</i> is 4 (32-bits). "
		        "If value is an array, writes the entire array to the file.")
	 , 310},
	{"writeAll",        js_writeall,        0,  JSTYPE_BOOLEAN, JSDOCSTR("array lines")
	 , JSDOCSTR("Write an array of new-line terminated strings (lines of text) to the file")
	 , 310},
	{"raw_write",       js_raw_write,       1,  JSTYPE_BOOLEAN, JSDOCSTR("text")
	 , JSDOCSTR("Write a string to the underlying file descriptor. "
		        "Undefined results when mixed with any other read/write methods except raw_read, including indirect ones.")
	 , 317},
	{"printf",          js_fprintf,         0,  JSTYPE_NUMBER,  JSDOCSTR("format [,args]")
	 , JSDOCSTR("Write a C-style formatted string to the file (ala the standard C <tt>fprintf</tt> function)")
	 , 310},
	{"iniGetSections",  js_iniGetSections,  0,  JSTYPE_ARRAY,   JSDOCSTR("[prefix=<i>none</i>]")
	 , JSDOCSTR("Parse all section names from a <tt>.ini</tt> file (format = '<tt>[section]</tt>') "
		        "and return the section names as an <i>array of strings</i>, "
		        "optionally, only those section names that begin with the specified <i>prefix</i>")
	 , 311},
	{"iniGetKeys",      js_iniGetKeys,      1,  JSTYPE_ARRAY,   JSDOCSTR("[section=<i>root</i>]")
	 , JSDOCSTR("Parse all key names from the specified <i>section</i> in a <tt>.ini</tt> file "
		        "and return the key names as an <i>array of strings</i>. "
		        "if <i>section</i> is undefined, returns key names from the <i>root</i> section")
	 , 311},
	{"iniGetValue",     js_iniGetValue,     3,  JSTYPE_UNDEF,   JSDOCSTR("section, key [,default=<i>none</i>]")
	 , JSDOCSTR("Parse a key from a <tt>.ini</tt> file and return its value (format = '<tt>key = value</tt>'). "
		        "To parse a key from the <i>root</i> section, pass <i>null</i> for <i>section</i>. "
		        "Returns the specified <i>default</i> value if the key or value is missing or invalid.<br>"
		        "Returns a <i>bool</i>, <i>number</i>, <i>string</i>, or an <i>array of strings</i> "
		        "determined by the type of <i>default</i> value specified. "
		        "<br><b>Note:</b> To insure that any/all values are returned as a string (e.g. numeric passwords are <b>not</b> returned as a <i>number</i>), "
		        "pass an empty string ('') for the <i>default</i> value." )
	 , 311},
	{"iniSetValue",     js_iniSetValue,     3,  JSTYPE_BOOLEAN, JSDOCSTR("section, key, [value=<i>none</i>]")
	 , JSDOCSTR("Set the specified <i>key</i> to the specified <i>value</i> in the specified <i>section</i> "
		        "of a <tt>.ini</tt> file. "
		        "to set a key in the <i>root</i> section, pass <i>null</i> for <i>section</i>. ")
	 , 312},
	{"iniGetObject",    js_iniGetObject,    1,  JSTYPE_OBJECT,  JSDOCSTR("[section=<i>root</i>] [,<i>bool</i> lowercase=false] "
		                                                                 "[,<i>bool</i> blanks=false]")
	 , JSDOCSTR("Parse an entire section from a .ini file "
		        "and return all of its keys (optionally lowercased) and values as properties of an object.<br>"
		        "If <i>section</i> is <tt>null</tt> or <tt>undefined</tt>, returns keys and values from the <i>root</i> section.<br>"
		        "If <i>blanks</i> is <tt>true</tt> then empty string (instead of <tt>undefined</tt>) values may included in the returned object.<br>"
		        "Returns <i>null</i> if the specified <i>section</i> does not exist in the file or the file has not been opened.")
	 , 311},
	{"iniSetObject",    js_iniSetObject,    2,  JSTYPE_BOOLEAN, JSDOCSTR("section, <i>object</i> object")
	 , JSDOCSTR("Write all the properties of the specified <i>object</i> as separate <tt>key=value</tt> pairs "
		        "in the specified <i>section</i> of a <tt>.ini</tt> file.<br>"
		        "To write an object in the <i>root</i> section, pass <i>null</i> for <i>section</i>."
		        "<br><b>Note:</b> this method does not remove unreferenced keys from an existing section. "
		        "If your intention is to <i>replace</i> an existing section, use the <tt>iniRemoveSection</tt> function first." )
	 , 312},
	{"iniGetAllObjects", js_iniGetAllObjects, 1,  JSTYPE_ARRAY,   JSDOCSTR("[<i>string</i> name_property] [,<i>string</i> prefix=<i>none</i>] [,<i>bool</i> lowercase=false] "
		                                                                   "[,blanks=false]")
	 , JSDOCSTR("Parse all sections from a .ini file and return all (non-<i>root</i>) sections "
		        "in an array of objects with each section's keys (optionally lowercased) as properties of each object.<br>"
		        "<i>name_property</i> is the name of the property to create to contain the section's name "
		        "(optionally lowercased, default is <tt>\"name\"</tt>), "
		        "the optional <i>prefix</i> has the same use as in the <tt>iniGetSections</tt> method.<br>"
		        "If a (String) <i>prefix</i> is specified, it is removed from each section's name.<br>"
		        "If <i>blanks</i> is <tt>true</tt> then empty string (instead of <tt>undefined</tt>) values may be included in the returned objects."
		        )
	 , 311},
	{"iniSetAllObjects", js_iniSetAllObjects, 1,  JSTYPE_BOOLEAN, JSDOCSTR("<i>object</i> array [,name_property=\"name\"]")
	 , JSDOCSTR("Write an array of objects to a .ini file, each object in its own section named "
		        "after the object's <i>name_property</i> (default: <tt>name</tt>)")
	 , 312},
	{"iniRemoveKey",    js_iniRemoveKey,    2,  JSTYPE_BOOLEAN, JSDOCSTR("section, key")
	 , JSDOCSTR("Remove specified <i>key</i> from specified <i>section</i> in <tt>.ini</tt> file.")
	 , 314},
	{"iniRemoveSection", js_iniRemoveSection, 1,  JSTYPE_BOOLEAN, JSDOCSTR("section")
	 , JSDOCSTR("Remove specified <i>section</i> from <tt>.ini</tt> file.")
	 , 314},
	{"iniRemoveSections", js_iniRemoveSections, 1, JSTYPE_BOOLEAN,    JSDOCSTR("[prefix]")
	 , JSDOCSTR("Remove all sections from <tt>.ini</tt> file, optionally only sections with the specified section name <i>prefix</i>.")
	 , 32000},
	{"iniReadAll",      js_iniReadAll,      0,  JSTYPE_ARRAY,   JSDOCSTR("")
	 , JSDOCSTR("Read entire <tt>.ini</tt> file into an array of strings (with <tt>!include</tt>ed files).")
	 , 31802},
	{0}
};

/* File Destructor */

static void js_finalize_file(JSContext *cx, JSObject *obj)
{
	private_t* p;

	if ((p = (private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return;

	if (p->fp != NULL)
		fclose(p->fp);

	dbprintf(FALSE, p, "finalized: %s", p->name);

	FREE_AND_NULL(p->ini_style.key_prefix);
	FREE_AND_NULL(p->ini_style.section_separator);
	FREE_AND_NULL(p->ini_style.value_separator);
	FREE_AND_NULL(p->ini_style.bit_separator);
	FREE_AND_NULL(p->ini_style.literal_separator);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}

static JSBool js_file_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*  name = NULL;
	JSBool ret;

	if (id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval))
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
	}

	ret = js_SyncResolve(cx, obj, name, js_file_properties, js_file_functions, NULL, 0);
	if (name)
		free(name);
	return ret;
}

static JSBool js_file_enumerate(JSContext *cx, JSObject *obj)
{
	return js_file_resolve(cx, obj, JSID_VOID);
}

JSClass js_file_class = {
	"File"                  /* name			*/
	, JSCLASS_HAS_PRIVATE    /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_file_get            /* getProperty	*/
	, js_file_set            /* setProperty	*/
	, js_file_enumerate      /* enumerate	*/
	, js_file_resolve        /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_finalize_file       /* finalize		*/
};

/* File Constructor (creates file descriptor) */

static JSBool
js_file_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj;
	jsval *    argv = JS_ARGV(cx, arglist);
	JSString*  str;
	private_t* p;

	obj = JS_NewObject(cx, &js_file_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if (argc < 1 || (str = JS_ValueToString(cx, argv[0])) == NULL) {
		JS_ReportError(cx, "No filename specified");
		return JS_FALSE;
	}

	if ((p = (private_t*)calloc(1, sizeof(private_t))) == NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}

	JSSTRING_TO_STRBUF(cx, str, p->name, sizeof(p->name), NULL);

	if (!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used for opening, creating, reading, or writing files on the local file system<p>"
	                      "Special features include:</h2><ol style=list-style-type:disc>"
	                      "<li>Exclusive-access files (default) or shared files<ol type=circle>"
	                      "<li>optional record-locking"
	                      "<li>buffered or non-buffered I/O"
	                      "</ol>"
	                      "<li>Support for binary files<ol style=list-style-type:circle>"
	                      "<li>native or network byte order (endian)"
	                      "<li>automatic Unix-to-Unix (<i>UUE</i>), yEncode (<i>yEnc</i>) or Base64 encoding/decoding"
	                      "</ol>"
	                      "<li>Support for ASCII text files<ol style=list-style-type:circle>"
	                      "<li>supports line-based I/O<ol style=list-style-type:square>"
	                      "<li>entire file may be read or written as an array of strings"
	                      "<li>individual lines may be read or written one line at a time"
	                      "</ol>"
	                      "<li>supports fixed-length records<ol style=list-style-type:square>"
	                      "<li>optional end-of-text (<i>etx</i>) character for automatic record padding/termination"
	                      "<li>Synchronet <tt>.dat</tt> files use an <i>etx</i> value of 3 (Ctrl-C)"
	                      "</ol>"
	                      "<li>supports <tt>.ini</tt> formatted configuration files"
	                      "</ol>"
	                      "<li>Dynamically-calculated industry standard checksums (e.g. CRC-16, CRC-32, MD5)"
	                      "</ol>"
	                      , 310
	                      );
	js_DescribeSyncConstructor(cx, obj, "To create a new File object: <tt>var f = new File(<i>filename</i>)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", file_prop_desc, JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object constructed");
	return JS_TRUE;
}

JSObject* js_CreateFileClass(JSContext* cx, JSObject* parent)
{
	JSObject* obj;

	obj = JS_InitClass(cx, parent, NULL
	                   , &js_file_class
	                   , js_file_constructor
	                   , 1 /* number of constructor args */
	                   , NULL /* props, set in constructor */
	                   , NULL /* funcs, set in constructor */
	                   , NULL, NULL);

	return obj;
}

JSObject* js_CreateFileObject(JSContext* cx, JSObject* parent, const char *name, int fd, const char* mode)
{
	JSObject*  obj;
	private_t* p;
	int        newfd = dup(fd);
	FILE*      fp;

	if (newfd == -1)
		return NULL;

	fp = fdopen(newfd, mode);
	if (fp == NULL) {
		close(newfd);
		return NULL;
	}

	obj = JS_DefineObject(cx, parent, name, &js_file_class, NULL
	                      , JSPROP_ENUMERATE | JSPROP_READONLY);

	if (obj == NULL) {
		fclose(fp);
		return NULL;
	}

	if ((p = (private_t*)calloc(1, sizeof(private_t))) == NULL) {
		fclose(fp);
		return NULL;
	}

	p->fp = fp;
	p->debug = JS_FALSE;

	if (!JS_SetPrivate(cx, obj, p)) {
		fclose(fp);
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return NULL;
	}
	dbprintf(FALSE, p, "object created");

	return obj;
}


#endif  /* JAVSCRIPT */
