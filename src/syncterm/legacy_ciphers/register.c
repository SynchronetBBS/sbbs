#include "legacy_ciphers.h"

void
legacy_ciphers_init(void)
{
	legacy_idea_register();
	legacy_rc2_register();
}
