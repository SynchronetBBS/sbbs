#include "xp_digest.h"

size_t
xp_digest_size(enum xp_digest_algorithm algorithm)
{
	(void)algorithm;
	return 0;
}
xp_digest_t
xp_digest_create(enum xp_digest_algorithm algorithm)
{
	(void)algorithm;
	return NULL;
}
int
xp_digest_update(xp_digest_t context, const void *data, size_t len)
{
	(void)context; (void)data; (void)len;
	return -1;
}
int
xp_digest_final(xp_digest_t context, void *out, size_t *len)
{
	(void)context; (void)out; (void)len;
	return -1;
}
void
xp_digest_free(xp_digest_t context)
{
	(void)context;
}

int
xp_digest(enum xp_digest_algorithm algorithm, const void *data, size_t len,
	      void *out, size_t *out_len)
{
	(void)algorithm; (void)data; (void)len; (void)out; (void)out_len;
	return -1;
}
