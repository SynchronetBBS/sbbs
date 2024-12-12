#include "deucessh.h"

static int
encrypt(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess)
{
	return 0;
}

static int
decrypt(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess)
{
	return 0;
}

static void
cleanup(deuce_ssh_session_t sess)
{
	return;
}

static struct deuce_ssh_enc none_enc = {
	.next = NULL,
	.encrypt = encrypt,
	.decrypt = decrypt,
	.cleanup = cleanup,
	.flags = 0,
	.blocksize = 1,
	.key_size = 0,
	.name = "none",
};

int
register_none_enc(void)
{
	return deuce_ssh_transport_register_enc(&none_enc);
}
