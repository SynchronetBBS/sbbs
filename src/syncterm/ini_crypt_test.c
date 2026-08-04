#include <stdio.h>
#include <string.h>

#include "ini_crypt.h"
#include "legacy_ciphers/legacy_ciphers.h"

static bool
password(char *buffer, size_t *length)
{
	static const char value[] = "correct horse battery staple";
	if (*length < sizeof(value) - 1)
		return false;
	memcpy(buffer, value, sizeof(value) - 1);
	*length = sizeof(value) - 1;
	return true;
}

static int
round_trip(enum iniCryptAlgo algorithm)
{
	str_list_t input = strListInit();
	if (input == NULL
	    || strListPush(&input, "[first]") == NULL
	    || strListPush(&input, "value=one") == NULL
	    || strListPush(&input, "[second]") == NULL
	    || strListPush(&input, "value=two") == NULL) {
		strListFree(&input);
		return 1;
	}
	FILE *file = tmpfile();
	if (file == NULL
	    || !iniWriteEncryptedFile(file, input, algorithm, 256,
	        "scrypt-N8-r8-p1", "correct horse battery staple")) {
		if (file != NULL) fclose(file);
		strListFree(&input);
		return 1;
	}
	enum iniCryptAlgo actual_algorithm = INI_CRYPT_ALGO_NONE;
	enum xp_kdf_algorithm actual_kdf = 0;
	int key_size = 0;
	str_list_t output = iniReadEncryptedFile(file, password, NULL,
	    &actual_algorithm, &key_size, &actual_kdf);
	int result = output == NULL || actual_algorithm != algorithm
	    || key_size != 256 || actual_kdf != XP_KDF_SCRYPT
	    || strListCount(input) != strListCount(output);
	if (!result) {
		for (size_t i = 0; input[i] != NULL; i++) {
			if (strcmp(input[i], output[i]) != 0) {
				result = 1;
				break;
			}
		}
	}
	strListFree(&output);
	strListFree(&input);
	fclose(file);
	return result;
}

static int
legacy_cipher_vectors(void)
{
	static const unsigned char iv[8] = {0};
	static const unsigned char idea_key[16] = {
		0x00,0x01,0x00,0x02,0x00,0x03,0x00,0x04,
		0x00,0x05,0x00,0x06,0x00,0x07,0x00,0x08
	};
	static const unsigned char idea_plaintext[8] = {
		0x00,0x00,0x00,0x01,0x00,0x02,0x00,0x03
	};
	unsigned char idea[8] = {
		0x11,0xfb,0xed,0x2b,0x01,0x98,0x6d,0xe5
	};
	if (legacy_idea_decrypt_cbc(idea_key, sizeof(idea_key), iv,
	    idea, sizeof(idea)) != 0
	    || memcmp(idea, idea_plaintext, sizeof(idea)) != 0)
		return 1;
	static const unsigned char rc2_key[16] = {
		0x88,0xbc,0xa9,0x0e,0x90,0x87,0x5a,0x7f,
		0x0f,0x79,0xc3,0x84,0x62,0x7b,0xaf,0xb2
	};
	static const unsigned char rc2_plaintext[8] = {0};
	unsigned char rc2[8] = {
		0x22,0x69,0x55,0x2a,0xb0,0xf8,0x5c,0xa6
	};
	return legacy_rc2_decrypt_cbc(rc2_key, sizeof(rc2_key), iv,
	    rc2, sizeof(rc2)) != 0
	    || memcmp(rc2, rc2_plaintext, sizeof(rc2)) != 0;
}

int
main(void)
{
	if (legacy_cipher_vectors() != 0
	    || round_trip(INI_CRYPT_ALGO_AES) != 0
	    || round_trip(INI_CRYPT_ALGO_CHACHA20) != 0)
		return 1;
	FILE *file = tmpfile();
	if (file == NULL)
		return 1;
	str_list_t empty = strListInit();
	bool rejected = !iniWriteEncryptedFile(file, empty,
	    INI_CRYPT_ALGO_3DES, 192, "scrypt-N8", "password");
	strListFree(&empty);
	fclose(file);
	return rejected ? 0 : 1;
}
