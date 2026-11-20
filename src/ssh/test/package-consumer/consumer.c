#include <deucessh.h>
#include <deucessh-crypto.h>
#include <deucessh-algorithms.h>

#include <string.h>

/* The same ordinary C17 calls exercise static, shared and installed targets. */
#ifdef DSSH_ABI_CONSUMER_DSO
#define consumer_entry dssh_abi_consumer_check
#ifdef _WIN32
__declspec(dllexport)
#endif
#else
#define consumer_entry main
#endif
int consumer_entry(void);
int
consumer_entry(void)
{
	static const uint8_t expected[32] = {
		0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
		0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
		0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
		0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
	};
	static const uint8_t input[] = { 'a', 'b', 'c' };
	uint8_t digest[32];
	if (dssh_hash_oneshot("SHA-256", input, sizeof(input), digest, sizeof(digest)) != 0)
		return 1;
	if (memcmp(digest, expected, sizeof(expected)) != 0 || dssh_register_ssh_ed25519() != 0)
		return 1;
	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL)
		return 1;
	int ret = dssh_session_generate_client_key(sess, "ssh-ed25519", 256);
	if (ret == 0 && dssh_session_get_client_key_pub_str(sess, "ssh-ed25519", NULL, 0) <= 0)
		ret = DSSH_ERROR_INIT;
	dssh_session_cleanup(sess);
	return ret != 0;
}
