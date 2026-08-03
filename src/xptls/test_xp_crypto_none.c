#include <stddef.h>
#include <string.h>
#include "xp_ca.h"
#include "xp_digest.h"
#include "xp_sign.h"
#include "xp_tls.h"

int main(void)
{
	xp_key_t key = (xp_key_t)1;
	const struct xp_key_spec spec = { XP_KEY_ED25519, 0, XP_KEY_CURVE_NONE };
	if (xp_key_generate(&key, &spec) != XP_CA_ERR_DISABLED || key != NULL)
		return 1;
	if (xp_key_import_private_pem(&key, "x", 1, NULL, NULL) != XP_CA_ERR_DISABLED || key != NULL)
		return 1;
	struct xp_key_store_config key_store = {
		.policy = XP_KEY_STORAGE_NAMED,
		.store = "pkcs11",
		.store_uri = "pkcs11:?module-path=missing",
	};
	struct xp_key_store_capabilities capabilities;
	if (xp_key_store_query(&key_store, &spec, &capabilities)
		!= XP_CRYPTO_ERR_DISABLED
		|| capabilities.available
		|| capabilities.availability_status != XP_CRYPTO_ERR_DISABLED)
		return 1;
	if (xp_key_generate_stored(&key, &key_store, &spec)
		!= XP_CRYPTO_ERR_DISABLED || key != NULL)
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
	struct xp_tls_client_config client = {0};
	struct xp_tls_server_config server = {0};
	static const unsigned char psk[] = {1, 2, 3, 4};
	if (xp_tls_client_open(INVALID_SOCKET, "disabled", 1) != NULL
	    || xp_tls_client_open_config(INVALID_SOCKET, &client) != NULL
	    || xp_tls_client_open_psk(INVALID_SOCKET, "disabled", 1,
	        "identity", psk, sizeof(psk)) != NULL
	    || xp_tls_server_open(INVALID_SOCKET, NULL, &server) != NULL
	    || strstr(xp_tls_last_err(), "disabled") == NULL)
		return 1;
	xp_tls_server_credentials_t credentials = (xp_tls_server_credentials_t)1;
	struct xp_tls_server_credentials_config credential_config = {
		.certificate_chain_file = "disabled.pem",
		.private_key = (xp_key_t)1,
	};
	if (xp_tls_server_credentials_load(&credentials, &credential_config)
	    != XP_CRYPTO_ERR_DISABLED || credentials != NULL)
		return 1;
	return 0;
}
