#include "ssh-trans.h"

static int
generate(uint8_t *key, uint8_t *buf, size_t bufsz, uint8_t *outbuf, deuce_ssh_session_t sess)
{
	return 0;
}

static void
cleanup(deuce_ssh_session_t sess)
{
	return;
}

static struct deuce_ssh_mac none_mac = {
	.next = NULL,
	.generate = generate,
	.cleanup = cleanup,
	.digest_size = 0,
	.key_size = 0,
	.name = "none",
};

int
register_none_mac(void)
{
	return deuce_ssh_transport_register_mac(&none_mac);
}
