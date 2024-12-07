#include "ssh-trans.h"

static int
compress(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess)
{
	return 0;
}

static int
uncompress(uint8_t *buf, uint8_t *bufsz, deuce_ssh_session_t sess)
{
	return 0;
}

static void
cleanup(deuce_ssh_session_t sess)
{
	return;
}

static struct deuce_ssh_comp none_comp = {
	.next = NULL,
	.compress = compress,
	.uncompress = uncompress,
	.cleanup = cleanup,
	.name = "none",
};

int
register_none_comp(void)
{
	return deuce_ssh_transport_register_comp(&none_comp);
}
