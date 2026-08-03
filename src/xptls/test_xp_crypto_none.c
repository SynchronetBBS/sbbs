#include <stddef.h>
#include "xp_ca.h"
#include "xp_digest.h"
#include "xp_sign.h"

int main(void)
{
	xp_ca_key_t key = (xp_ca_key_t)1;
	if (xp_ca_key_generate(&key, XP_CA_KEY_ED25519) != XP_CA_ERR_DISABLED || key != NULL)
		return 1;
	if (xp_ca_key_import_pem(&key, "x", 1, NULL, NULL) != XP_CA_ERR_DISABLED || key != NULL)
		return 1;
	if (xp_sign(NULL, XP_SIGN_ED25519, XP_SIGNATURE_ENCODING_STANDARD,
		"x", 1, NULL, NULL) != XP_CA_ERR_DISABLED)
		return 1;
	if (xp_verify(NULL, XP_SIGN_ED25519, XP_SIGNATURE_ENCODING_STANDARD,
		"x", 1, "x", 1) != XP_CA_ERR_DISABLED)
		return 1;
	if (xp_digest_size(XP_DIGEST_SHA256) != 0
	    || xp_digest_create(XP_DIGEST_SHA256) != NULL)
		return 1;
	return 0;
}
