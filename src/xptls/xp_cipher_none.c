#include "xp_cipher.h"

bool xp_cipher_supported(enum xp_cipher_algorithm algorithm, enum xp_cipher_mode mode)
{
	(void)algorithm; (void)mode; return false;
}
int xp_cipher_create(xp_cipher_t *out, const struct xp_cipher_config *config)
{
	if (out != NULL) *out = NULL; (void)config; return XP_CRYPTO_ERR_DISABLED;
}
int xp_cipher_aad(xp_cipher_t context, const void *data, size_t len)
{
	(void)context; (void)data; (void)len; return XP_CRYPTO_ERR_DISABLED;
}
int xp_cipher_update(xp_cipher_t context, const void *input, size_t input_len,
	void *out, size_t *out_len)
{
	(void)context; (void)input; (void)input_len; (void)out; (void)out_len;
	return XP_CRYPTO_ERR_DISABLED;
}
int xp_cipher_set_tag(xp_cipher_t context, const void *tag, size_t len)
{
	(void)context; (void)tag; (void)len; return XP_CRYPTO_ERR_DISABLED;
}
int xp_cipher_final(xp_cipher_t context, void *out, size_t *out_len)
{
	(void)context; (void)out; (void)out_len; return XP_CRYPTO_ERR_DISABLED;
}
int xp_cipher_get_tag(xp_cipher_t context, void *tag, size_t *len)
{
	(void)context; (void)tag; (void)len; return XP_CRYPTO_ERR_DISABLED;
}
const char *xp_cipher_errstr(xp_cipher_t context)
{
	(void)context; return "crypto support disabled";
}
void xp_cipher_free(xp_cipher_t context) { (void)context; }
