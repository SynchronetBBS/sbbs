/*
 * Part of the single-TU build.  All system and third-party includes
 * live in sftp.c; this file is #include'd from sftp.c and cannot be
 * compiled on its own.
 */

/* Release callback for heap-allocated strings: header and bytes live
 * in one allocation, so freeing the struct frees both. */
static void
sftp_heap_release(struct sftp_string *s)
{
	free(s);
}

sftp_str_t
sftp_alloc_str(uint32_t len)
{
	size_t hdr = sizeof(struct sftp_string);
	sftp_str_t ret = (sftp_str_t)malloc(hdr + (size_t)len + 1);
	if (ret == NULL)
		return NULL;
	ret->len            = len;
	ret->c_str          = (uint8_t *)ret + hdr;
	ret->release        = sftp_heap_release;
	ret->release_cbdata = NULL;
	ret->c_str[len]     = 0;
	return ret;
}

sftp_str_t
sftp_strdup(const char *str)
{
	size_t len = strlen(str);
	if (len > UINT32_MAX)
		return NULL;
	sftp_str_t ret = sftp_alloc_str(len);
	if (ret == NULL)
		return ret;
	memcpy(ret->c_str, str, ret->len);
	return ret;
}

sftp_str_t
sftp_asprintf(const char *format, ...)
{
	va_list va;

	va_start(va, format);
	char *str = xp_vasprintf(format, va);
	va_end(va);
	if (str == NULL)
		return NULL;
	size_t len = strlen(str);
	if (len > UINT32_MAX) {
		free(str);
		return NULL;
	}
	sftp_str_t ret = sftp_alloc_str(len);
	if (ret == NULL) {
		free(str);
		return NULL;
	}
	memcpy(ret->c_str, str, ret->len);
	free(str);
	return ret;
}

sftp_str_t
sftp_memdup(const uint8_t *buf, uint32_t sz)
{
	sftp_str_t ret = sftp_alloc_str(sz);
	if (ret == NULL)
		return ret;
	memcpy(ret->c_str, buf, sz);
	return ret;
}

void
sftp_strstatic(struct sftp_string *out, const char *str)
{
	out->len            = (uint32_t)strlen(str);
	out->c_str          = (uint8_t *)(uintptr_t)str;
	out->release        = NULL;
	out->release_cbdata = NULL;
}

void
sftp_memstatic(struct sftp_string *out, const uint8_t *buf, uint32_t len)
{
	out->len            = len;
	out->c_str          = (uint8_t *)(uintptr_t)buf;
	out->release        = NULL;
	out->release_cbdata = NULL;
}

void
sftp_strborrow(struct sftp_string *out, const char *str,
    void (*release)(struct sftp_string *), void *cbdata)
{
	out->len            = (uint32_t)strlen(str);
	out->c_str          = (uint8_t *)(uintptr_t)str;
	out->release        = release;
	out->release_cbdata = cbdata;
}

void
sftp_memborrow(struct sftp_string *out, const uint8_t *buf, uint32_t len,
    void (*release)(struct sftp_string *), void *cbdata)
{
	out->len            = len;
	out->c_str          = (uint8_t *)(uintptr_t)buf;
	out->release        = release;
	out->release_cbdata = cbdata;
}

void
free_sftp_str(sftp_str_t str)
{
	if (str == NULL)
		return;
	if (str->release != NULL)
		str->release(str);
}
