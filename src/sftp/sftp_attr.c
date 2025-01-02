#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sftp.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

struct sftp_file_attributes {
	uint32_t flags;
	uint64_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t perm;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ext_count;
	struct sftp_extended_file_attribute ext[];
};

sftp_file_attr_t
sftp_fattr_alloc(void)
{
	sftp_file_attr_t ret = calloc(1, sizeof(struct sftp_file_attributes));
	return ret;
}

void
sftp_fattr_free(sftp_file_attr_t fattr)
{
	uint32_t i;

	for (i = 0; i < fattr->ext_count; i++) {
		free_sftp_str(fattr->ext[i].type);
		free_sftp_str(fattr->ext[i].data);
	}
	free(fattr);
}

void
sftp_fattr_set_size(sftp_file_attr_t fattr, uint64_t sz)
{
	assert(fattr);
	fattr->size = sz;
	fattr->flags |= SSH_FILEXFER_ATTR_SIZE;
}

bool
sftp_fattr_get_size(sftp_file_attr_t fattr, uint64_t *sz)
{
	assert(fattr);
	if (fattr->flags & SSH_FILEXFER_ATTR_SIZE) {
		if (sz)
			*sz = fattr->size;
		return true;
	}
	return false;
}

void
sftp_fattr_set_uid_gid(sftp_file_attr_t fattr, uint32_t uid, uint32_t gid)
{
	assert(fattr);
	fattr->uid = uid;
	fattr->gid = gid;
	fattr->flags |= SSH_FILEXFER_ATTR_UIDGID;
}

bool
sftp_fattr_get_uid(sftp_file_attr_t fattr, uint32_t *uid)
{
	assert(fattr);
	if (fattr->flags & SSH_FILEXFER_ATTR_UIDGID) {
		if (uid)
			*uid = fattr->uid;
		return true;
	}
	return false;
}

bool
sftp_fattr_get_gid(sftp_file_attr_t fattr, uint32_t *gid)
{
	assert(fattr);
	if (fattr->flags & SSH_FILEXFER_ATTR_UIDGID) {
		if (gid)
			*gid = fattr->gid;
		return true;
	}
	return false;
}

void
sftp_fattr_set_permissions(sftp_file_attr_t fattr, uint32_t perm)
{
	assert(fattr);
	fattr->perm = perm;
	fattr->flags |= SSH_FILEXFER_ATTR_PERMISSIONS;
}

bool
sftp_fattr_get_permissions(sftp_file_attr_t fattr, uint32_t *perm)
{
	assert(fattr);
	if (fattr->flags & SSH_FILEXFER_ATTR_PERMISSIONS) {
		if (perm)
			*perm = fattr->perm;
		return true;
	}
	return false;
}

void
sftp_fattr_set_times(sftp_file_attr_t fattr, uint32_t atime, uint32_t mtime)
{
	assert(fattr);
	fattr->atime = atime;
	fattr->mtime = mtime;
	fattr->flags |= SSH_FILEXFER_ATTR_ACMODTIME;
}

bool
sftp_fattr_get_atime(sftp_file_attr_t fattr, uint32_t *atime)
{
	assert(fattr);
	if (fattr->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
		if (atime)
			*atime = fattr->atime;
		return true;
	}
	return false;
}

bool
sftp_fattr_get_mtime(sftp_file_attr_t fattr, uint32_t *mtime)
{
	assert(fattr);
	if (fattr->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
		if (mtime)
			*mtime = fattr->mtime;
		return true;
	}
	return false;
}

static bool
grow_ext(sftp_file_attr_t *fattr)
{
	size_t newsz = offsetof(struct sftp_file_attributes, ext);
	newsz += sizeof((*fattr)->ext[0]) * ((*fattr)->ext_count + 1);
	sftp_file_attr_t new_attr = realloc(*fattr, newsz);
	if (new_attr == NULL)
		return false;
	*fattr = new_attr;
	return true;
}

bool
sftp_fattr_add_ext(sftp_file_attr_t *fattr, sftp_str_t type, sftp_str_t data)
{
	assert(fattr);
	assert(*fattr);
	if (fattr == NULL)
		return false;
	if (*fattr == NULL)
		return false;
	assert(type != NULL);
	assert(data != NULL);
	if (type == NULL || data == NULL)
		return false;
	sftp_str_t newtype = sftp_memdup(type->c_str, type->len);
	if (newtype == NULL)
		return false;
	sftp_str_t newdata = sftp_memdup(data->c_str, data->len);
	if (newdata == NULL) {
		free_sftp_str(newtype);
		return false;
	}
	if (!grow_ext(fattr)) {
		free_sftp_str(newtype);
		free_sftp_str(newdata);
		return false;
	}
	(*fattr)->ext[(*fattr)->ext_count].type = newtype;
	(*fattr)->ext[(*fattr)->ext_count].data = newdata;
	(*fattr)->ext_count += 1;
	(*fattr)->flags |= SSH_FILEXFER_ATTR_EXTENDED;
	return true;
}

sftp_str_t
sftp_fattr_get_ext_type(sftp_file_attr_t fattr, uint32_t index)
{
	assert(fattr);
	if (index >= fattr->ext_count)
		return NULL;
	return fattr->ext[index].type;
}

sftp_str_t
sftp_fattr_get_ext_data(sftp_file_attr_t fattr, uint32_t index)
{
	assert(fattr);
	if (index >= fattr->ext_count)
		return NULL;
	return fattr->ext[index].data;
}

sftp_str_t
sftp_fattr_get_ext_by_type(sftp_file_attr_t fattr, const char *type)
{
	for (uint32_t i = 0; i < fattr->ext_count; i++) {
		if (strcmp((const char *)fattr->ext[i].type->c_str, type) == 0)
			return fattr->ext[i].data;
	}
	return NULL;
}

uint32_t
sftp_fattr_get_ext_count(sftp_file_attr_t fattr)
{
	assert(fattr);
	return fattr->ext_count;
}

static uint32_t
get_size(sftp_file_attr_t fattr)
{
	assert(fattr);
	if (fattr == NULL)
		return 0;
	uint32_t ret = sizeof(uint32_t);
	if (fattr->flags & SSH_FILEXFER_ATTR_SIZE)
		ret += sizeof(uint64_t);
	if (fattr->flags & SSH_FILEXFER_ATTR_UIDGID)
		ret += sizeof(uint32_t) * 2;
	if (fattr->flags & SSH_FILEXFER_ATTR_EXTENDED) {
		ret += sizeof(uint32_t);
		for (uint32_t i = 0; i < fattr->ext_count; i++) {
			ret += sizeof(uint32_t);
			ret += fattr->ext[i].type->len;
			ret += sizeof(uint32_t);
			ret += fattr->ext[i].data->len;
		}
	}

	return ret;
}

bool
sftp_appendfattr(sftp_tx_pkt_t *pktp, sftp_file_attr_t fattr)
{
	uint32_t sz = get_size(fattr);
	uint32_t oldused = (*pktp)->used;
	if (sz == 0)
		return false;
	if (!sftp_append32(pktp, fattr->flags))
		return false;
	if (fattr->flags & SSH_FILEXFER_ATTR_SIZE) {
		if (!sftp_append64(pktp, fattr->size))
			goto fail;
	}
	if (fattr->flags & SSH_FILEXFER_ATTR_UIDGID) {
		if (!sftp_append32(pktp, fattr->uid))
			goto fail;
		if (!sftp_append32(pktp, fattr->gid))
			goto fail;
	}
	if (fattr->flags & SSH_FILEXFER_ATTR_PERMISSIONS) {
		if (!sftp_append32(pktp, fattr->perm))
			goto fail;
	}
	if (fattr->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
		if (!sftp_append32(pktp, fattr->atime))
			goto fail;
		if (!sftp_append32(pktp, fattr->mtime))
			goto fail;
	}
	if (fattr->flags & SSH_FILEXFER_ATTR_EXTENDED) {
		if (!sftp_append32(pktp, fattr->ext_count))
			goto fail;
		for (uint32_t i = 0; i < fattr->ext_count; i++) {
			if (!sftp_appendstring(pktp, fattr->ext[i].type))
				goto fail;
			if (!sftp_appendstring(pktp, fattr->ext[i].data))
				goto fail;
		}
	}

	return true;
fail:
	(*pktp)->used = oldused;
	return false;
}

sftp_file_attr_t
sftp_getfattr(sftp_rx_pkt_t pkt)
{
	sftp_file_attr_t ret = sftp_fattr_alloc();
	if (ret == NULL)
		return ret;
	ret->flags = sftp_get32(pkt);
	if (ret->flags & SSH_FILEXFER_ATTR_SIZE) {
		ret->size = sftp_get64(pkt);
	}
	if (ret->flags & SSH_FILEXFER_ATTR_UIDGID) {
		ret->uid = sftp_get32(pkt);
		ret->gid = sftp_get32(pkt);
	}
	if (ret->flags & SSH_FILEXFER_ATTR_PERMISSIONS) {
		ret->perm = sftp_get32(pkt);
	}
	if (ret->flags & SSH_FILEXFER_ATTR_ACMODTIME) {
		ret->atime = sftp_get32(pkt);
		ret->mtime = sftp_get32(pkt);
	}
	if (ret->flags & SSH_FILEXFER_ATTR_EXTENDED) {
		uint32_t extcnt = sftp_get32(pkt);
		uint32_t ext;
		/*
		 * This is to silence Coverity...
		 * Coverity knows extcnt is tainted, and
		 * so I "should" range-check it before using
		 * it to control loop iterations.
		 * This loop is actually controlled by the
		 * size of the buffer since sftp_getstring()
		 * will fail long before we reach extcnt if
		 * it has a maliciously high value.
		 */
		extcnt &= 0x3FFFFFFF;
		for (ext = 0; ext < extcnt; ext++) {
			sftp_str_t type = sftp_getstring(pkt);
			if (type == NULL)
				break;
			sftp_str_t data = sftp_getstring(pkt);
			if (data == NULL) {
				free_sftp_str(type);
				break;
			}
			if (!sftp_fattr_add_ext(&ret, type, data)) {
				free_sftp_str(type);
				free_sftp_str(data);
				break;
			}
			free_sftp_str(type);
			free_sftp_str(data);
		}
		if (ext != extcnt) {
			sftp_fattr_free(ret);
			return NULL;
		}
	}
	return ret;
}
