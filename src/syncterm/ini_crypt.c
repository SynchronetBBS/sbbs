/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 ****************************************************************************/

/*
 * Encrypted INI file format — read/write, header parsing, KDF spec, and
 * the iniCryptGetAlgoName/iniCryptGetAlgoFromName helpers.  Kept in its
 * own compilation unit so consumers that only call plain iniReadFile /
 * iniGetString / etc. don't transitively pull xp_crypt (and with it the
 * OpenSSL or Botan backend) into the link.  Only SyncTERM and anyone
 * else that actually reads/writes encrypted bbslist.ini needs to pull
 * ini_crypt.o.
 *
 * Two on-disk header formats coexist:
 *
 *   v1 — Cryptlib-era:   "; Encrypted INI File, Algorithm: <ALGO>[-bits] <salt>"
 *        KDF is PBKDF2-HMAC-SHA256 implicitly (salt is printable ASCII, the
 *        caller supplies an iteration count).
 *
 *   v2 — current:        "; Encrypted INI File v2, Algorithm: <ALGO>[-bits], \
 *                          KDF: <spec>, Salt: <hex>"
 *        KDF spec is one of:
 *            PBKDF2-SHA256-i<iters>
 *            scrypt-N<cost_log2>-r<blocksize>-p<parallelism>
 *        Salt is raw bytes, hex-encoded to keep the header single-line and
 *        free of whitespace / field separators.
 *
 * Readers accept both; writers emit v2 with scrypt defaults.
 */

#include <ctype.h>      /* isxdigit */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen_defs.h"   /* SKIP_WHITESPACE */
#include "genwrap.h"    /* xp_random */
#include "ini_crypt.h"
#include "ini_file.h"
#include "netwrap.h"    /* htons, ntohs */
#include "strwrap.h"    /* truncnl, truncsp */
#include "xp_crypt.h"

/* Mirrors the private definitions in ini_file.c. */
#define INI_MAX_LINE_LEN   (INI_MAX_VALUE_LEN * 2)
#define INI_COMMENT_CHAR   ';'

const char *encryptedHeaderPrefix   = "; Encrypted INI File, Algorithm: ";
const char *encryptedHeaderPrefixV2 = "; Encrypted INI File v2, Algorithm: ";

/* v2 scrypt defaults — RFC 7914 interactive. ~16 MiB memory. */
#define INI_SCRYPT_COST_LOG2 14
#define INI_SCRYPT_R         8
#define INI_SCRYPT_P         1

/* v2 binary salt length in bytes (32 hex chars). */
#define INI_V2_SALT_BYTES    16

/* Matches the maximum salt length historically allowed by Cryptlib
   (CRYPT_MAX_HASHSIZE = 64); preserved for on-disk compatibility. */
#define INI_MAX_SALT_SIZE 64

const char *
iniCryptGetAlgoName(enum iniCryptAlgo a)
{
	switch(a) {
		case INI_CRYPT_ALGO_3DES:     return "3DES";
		case INI_CRYPT_ALGO_AES:      return "AES";
		case INI_CRYPT_ALGO_CAST:     return "CAST";
		case INI_CRYPT_ALGO_CHACHA20: return "ChaCha20";
		case INI_CRYPT_ALGO_IDEA:     return "IDEA";
		case INI_CRYPT_ALGO_NONE:     return "NONE";
		case INI_CRYPT_ALGO_RC2:      return "RC2";
		case INI_CRYPT_ALGO_RC4:      return "RC4";
	}
	return NULL;
}

enum iniCryptAlgo
iniCryptGetAlgoFromName(const char *n)
{
	if (!strcmp(n, "3DES"))     return INI_CRYPT_ALGO_3DES;
	if (!strcmp(n, "AES"))      return INI_CRYPT_ALGO_AES;
	if (!strcmp(n, "CAST"))     return INI_CRYPT_ALGO_CAST;
	if (!strcmp(n, "ChaCha20")) return INI_CRYPT_ALGO_CHACHA20;
	if (!strcmp(n, "IDEA"))     return INI_CRYPT_ALGO_IDEA;
	if (!strcmp(n, "RC2"))      return INI_CRYPT_ALGO_RC2;
	if (!strcmp(n, "RC4"))      return INI_CRYPT_ALGO_RC4;
	return INI_CRYPT_ALGO_NONE;
}

/* Decode a hex string (nybble count = 2 * out_len) into bytes. */
static bool
hex_decode(const char *hex, size_t out_len, uint8_t *out)
{
	for (size_t i = 0; i < out_len; i++) {
		unsigned v;
		char c = hex[2 * i];
		char d = hex[2 * i + 1];
		if (!isxdigit((unsigned char)c) || !isxdigit((unsigned char)d))
			return false;
		if (sscanf(&hex[2 * i], "%2x", &v) != 1)
			return false;
		out[i] = (uint8_t)v;
	}
	return true;
}

/* Encode `in_len` bytes as a lowercase hex string in `out` (2*in_len+1 bytes). */
static void
hex_encode(const uint8_t *in, size_t in_len, char *out)
{
	static const char H[] = "0123456789abcdef";
	for (size_t i = 0; i < in_len; i++) {
		out[2 * i]     = H[in[i] >> 4];
		out[2 * i + 1] = H[in[i] & 0x0f];
	}
	out[2 * in_len] = 0;
}

/*
 * Parse a v2 KDF spec (e.g. "scrypt-N14-r8-p1", "PBKDF2-SHA256-i50000") into
 * kdf_params.  Returns true on success.  Leading/trailing whitespace in spec
 * is tolerated.
 */
static bool
parse_kdf_spec(const char *spec, struct xp_crypt_kdf_params *out)
{
	while (*spec == ' ')
		spec++;
	size_t len = strlen(spec);
	while (len > 0 && (spec[len - 1] == ' ' || spec[len - 1] == '\r'))
		len--;
	memset(out, 0, sizeof(*out));
	if (len >= 7 && strncmp(spec, "scrypt-", 7) == 0) {
		out->kdf = XP_CRYPT_KDF_SCRYPT;
		out->cost_log2  = INI_SCRYPT_COST_LOG2;
		out->block_size = INI_SCRYPT_R;
		out->parallelism = INI_SCRYPT_P;
		const char *p = spec + 6;
		while (p < spec + len && *p == '-') {
			p++;
			if (p >= spec + len)
				return false;
			char tag = *p++;
			char *endp = NULL;
			long v = strtol(p, &endp, 10);
			if (endp == p || v < 0 || v > INT_MAX)
				return false;
			switch (tag) {
				case 'N': out->cost_log2  = (int)v; break;
				case 'r': out->block_size = (int)v; break;
				case 'p': out->parallelism = (int)v; break;
				default:  return false;
			}
			p = endp;
		}
		return out->cost_log2 > 0 && out->block_size > 0 && out->parallelism > 0;
	}
	if (len >= 14 && strncmp(spec, "PBKDF2-SHA256-", 14) == 0) {
		out->kdf = XP_CRYPT_KDF_PBKDF2_HMAC_SHA256;
		const char *p = spec + 13;
		while (p < spec + len && *p == '-') {
			p++;
			if (p >= spec + len)
				return false;
			char tag = *p++;
			char *endp = NULL;
			long v = strtol(p, &endp, 10);
			if (endp == p || v < 1 || v > INT_MAX)
				return false;
			if (tag == 'i')
				out->iterations = (int)v;
			else
				return false;
			p = endp;
		}
		return out->iterations > 0;
	}
	return false;
}

/* Emit a v2 KDF spec for the given params into buf.  Returns true on success. */
static bool
format_kdf_spec(const struct xp_crypt_kdf_params *kdf, char *buf, size_t buflen)
{
	int n = 0;
	switch (kdf->kdf) {
		case XP_CRYPT_KDF_PBKDF2_HMAC_SHA256:
			n = snprintf(buf, buflen, "PBKDF2-SHA256-i%d", kdf->iterations);
			break;
		case XP_CRYPT_KDF_SCRYPT:
			n = snprintf(buf, buflen, "scrypt-N%d-r%d-p%d",
			    kdf->cost_log2, kdf->block_size, kdf->parallelism);
			break;
		default:
			return false;
	}
	return n > 0 && (size_t)n < buflen;
}

/*
 * Parse a v1 or v2 encrypted-INI header line into (algo, keySize, salt bytes,
 * kdf_params).  str is mutated.  Returns true on success.
 */
static bool
parse_encrypted_header(char *str, int fallback_pbkdf2_iters,
                       enum iniCryptAlgo *algo_out, size_t *keySize_out,
                       uint8_t *salt, size_t salt_cap, size_t *salt_len_out,
                       struct xp_crypt_kdf_params *kdf_out)
{
	char *start = NULL;
	const size_t v1_len = strlen(encryptedHeaderPrefix);
	const size_t v2_len = strlen(encryptedHeaderPrefixV2);
	bool is_v2 = false;

	if (strncmp(str, encryptedHeaderPrefixV2, v2_len) == 0) {
		start = str + v2_len;
		is_v2 = true;
	}
	else if (strncmp(str, encryptedHeaderPrefix, v1_len) == 0) {
		start = str + v1_len;
		is_v2 = false;
	}
	else {
		return false;
	}

	truncnl(str);

	if (is_v2) {
		/* v2 header: "ALGO[-bits], KDF: <spec>, Salt: <hex>" */
		char *kdf_tok = strstr(start, ", KDF: ");
		if (kdf_tok == NULL)
			return false;
		char *salt_tok = strstr(kdf_tok, ", Salt: ");
		if (salt_tok == NULL)
			return false;
		*kdf_tok = 0;
		kdf_tok += strlen(", KDF: ");
		*salt_tok = 0;
		salt_tok += strlen(", Salt: ");

		char *dash = strchr(start, '-');
		if (dash) {
			*dash = 0;
			char *endp = NULL;
			long ll = strtol(dash + 1, &endp, 10);
			if (ll <= 0 || ll == LONG_MAX)
				return false;
			*keySize_out = (size_t)ll;
		}
		else {
			*keySize_out = 0;
		}
		*algo_out = iniCryptGetAlgoFromName(start);
		if (*algo_out == INI_CRYPT_ALGO_NONE)
			return false;

		if (!parse_kdf_spec(kdf_tok, kdf_out))
			return false;

		size_t hexlen = strlen(salt_tok);
		while (hexlen > 0 && (salt_tok[hexlen - 1] == ' ' ||
		                      salt_tok[hexlen - 1] == '\r'))
			hexlen--;
		if ((hexlen & 1) != 0)
			return false;
		size_t sbytes = hexlen / 2;
		if (sbytes == 0 || sbytes > salt_cap)
			return false;
		if (!hex_decode(salt_tok, sbytes, salt))
			return false;
		*salt_len_out = sbytes;
		return true;
	}

	/* v1 header: "ALGO[-bits] <ascii-salt>" */
	char *space = strchr(start, ' ');
	char *dash  = strchr(start, '-');
	if (space == NULL)
		return false;
	if (dash > space)
		dash = NULL;
	char *end = dash ? dash : space;
	*end = 0;
	*algo_out = iniCryptGetAlgoFromName(start);
	if (*algo_out == INI_CRYPT_ALGO_NONE)
		return false;
	if (dash) {
		char *endp = NULL;
		long ll = strtol(dash + 1, &endp, 10);
		if (ll <= 0 || ll == LONG_MAX) {
			*space = 0;
			return false;
		}
		*keySize_out = (size_t)ll;
	}
	else {
		*keySize_out = 0;
	}
	*space = 0;
	char *salt_start = space + 1;
	truncsp(salt_start);
	size_t slen = strlen(salt_start);
	if (slen == 0 || slen > salt_cap)
		return false;
	memcpy(salt, salt_start, slen);
	*salt_len_out = slen;

	if (fallback_pbkdf2_iters < 1)
		fallback_pbkdf2_iters = 50000;
	memset(kdf_out, 0, sizeof(*kdf_out));
	kdf_out->kdf = XP_CRYPT_KDF_PBKDF2_HMAC_SHA256;
	kdf_out->iterations = fallback_pbkdf2_iters;
	return true;
}

str_list_t
iniReadEncryptedFile(FILE* fp, bool(*get_key)(void *cb_data, char *keybuf, size_t *sz), int KDFiterations, enum iniCryptAlgo *algoPtr, int *ks, char *saltBuf, size_t *saltsz, void *cbdata)
{
	char keyData[1024];
	size_t keyDataSize;
	uint8_t salt[INI_MAX_SALT_SIZE];
	size_t saltLength = 0;
	char str[INI_MAX_LINE_LEN + 1];
	size_t strpos = 0;
	char *buffer = NULL;
	size_t bufferSize = 0;
	size_t keySize = 0;
	enum iniCryptAlgo algo = INI_CRYPT_ALGO_NONE;
	struct xp_crypt_kdf_params kdf_params = {0};
	str_list_t ret = NULL;
	xp_crypt_t ctx = NULL;
	int blocksize;
	int ivsize;
	bool streamCipher = false;

	if (fp == NULL || get_key == NULL)
		goto done;

	rewind(fp);

	if (fgets(str, sizeof(str), fp) == NULL) {
		ret = strListInit();
		goto done;
	}

	if (strncmp(str, encryptedHeaderPrefix, strlen(encryptedHeaderPrefix)) != 0 &&
	    strncmp(str, encryptedHeaderPrefixV2, strlen(encryptedHeaderPrefixV2)) != 0) {
		ret = iniReadFile(fp);
		goto done;
	}

	if (!parse_encrypted_header(str, KDFiterations, &algo, &keySize,
	                             salt, sizeof(salt), &saltLength, &kdf_params))
		goto done;

	keyDataSize = sizeof(keyData);
	if (!get_key(cbdata, keyData, &keyDataSize))
		goto done;

	ctx = xp_crypt_open((int)algo, (int)keySize, salt, saltLength,
	                    &kdf_params, keyData, keyDataSize, /*encrypt=*/false);
	if (ctx == NULL)
		goto done;

	blocksize = xp_crypt_blocksize(ctx);
	if (blocksize <= 1) {
		bufferSize = INI_MAX_LINE_LEN - 1;
		streamCipher = true;
	} else {
		bufferSize = (size_t)blocksize;
	}

	ivsize = xp_crypt_ivsize(ctx);
	if (ivsize > 0) {
		unsigned char iv[XP_CRYPT_MAX_IVSIZE];
		uint16_t ivs;
		int iv_file_size;
		if (fread(&ivs, 1, sizeof(ivs), fp) != sizeof(ivs))
			goto done;
		iv_file_size = ntohs(ivs);
		if (iv_file_size <= 0 || iv_file_size > (int)sizeof(iv))
			goto done;
		if (fread(iv, 1, iv_file_size, fp) != (size_t)iv_file_size)
			goto done;
		if (xp_crypt_set_iv(ctx, iv, iv_file_size) != 0)
			goto done;
	}

	buffer = malloc(bufferSize);
	if (buffer == NULL)
		goto done;
	size_t lines = 0;
	while(!feof(fp)) {
		size_t rret = fread(buffer, 1, bufferSize, fp);
		if (rret > INT_MAX) {
			strListFree(&ret);
			ret = NULL;
			goto done;
		}
		if ((streamCipher && rret > 0) || rret == bufferSize) {
			size_t bufpos = 0;
			if (xp_crypt_process(ctx, buffer, rret) != 0)
				goto done;
			while (bufpos < rret) {
				if (buffer[bufpos] == '\n' || strpos == sizeof(str) - 2) {
					bufpos++;
					while (strpos && (str[strpos - 1] == '\r' || str[strpos - 1] == '\n'))
						strpos--;
					str[strpos] = 0;
					char *p = str;
					SKIP_WHITESPACE(p);
					if (*p == INI_COMMENT_CHAR) {
						strListFree(&ret);
						ret = NULL;
						goto done;
					}
					if (!strListAppend(&ret, str, lines++)) {
						strListFree(&ret);
						ret = NULL;
						goto done;
					}
					strpos = 0;
				}
				else
					str[strpos++] = buffer[bufpos++];
			}
		}
		else {
			if (!feof(fp)) {
				strListFree(&ret);
				ret = NULL;
				goto done;
			}
		}
	}
	/*
	 * Trailing bytes after the final '\n': for stream ciphers this is a
	 * genuine unterminated last line; for block ciphers it's the
	 * zero-pad the writer emitted to reach a block boundary.  Skip the
	 * zero-pad; keep a real partial line.
	 */
	if (strpos) {
		bool all_zero = true;
		for (size_t i = 0; i < strpos; i++) {
			if (str[i] != 0) { all_zero = false; break; }
		}
		if (!all_zero) {
			str[strpos] = 0;
			if (!strListAppend(&ret, str, lines++)) {
				strListFree(&ret);
				ret = NULL;
				goto done;
			}
		}
	}
	if (ret == NULL)
		ret = strListInit();

done:
	free(buffer);
	xp_crypt_close(ctx);
	if (algoPtr)
		*algoPtr = algo;
	if (ks)
		*ks = keySize;
	if (saltLength && saltBuf && saltsz && *saltsz) {
		size_t cp = *saltsz;
		if (cp > saltLength)
			cp = saltLength;
		if (cp)
			memcpy(saltBuf, salt, cp);
		if (cp < *saltsz)
			saltBuf[cp] = 0;
	}
	if (saltsz)
		*saltsz = saltLength;

	return ret;
}

/*
 * Append a single character to the encrypt buffer; when a full block
 * is accumulated, encrypt it in place and write to fp. Called only in
 * block-cipher mode — stream ciphers encrypt the partial buffer once
 * in the caller.
 */
static bool
addEncrpytedChar(xp_crypt_t ctx, const char ch, char *buffer, size_t blockSize, size_t *bufferPos, FILE *fp)
{
	buffer[(*bufferPos)++] = ch;
	if (*bufferPos == blockSize) {
		if (xp_crypt_process(ctx, buffer, blockSize) != 0)
			return false;
		if (fwrite(buffer, 1, blockSize, fp) != blockSize)
			return false;
		*bufferPos = 0;
	}
	return true;
}

/*
 * Writes the INI file in list to fp encrypted with key.
 *
 * If salt is specified, it is ignored (legacy parameter; writer always
 * generates a fresh binary salt).
 *
 * KDFiterations is ignored: the writer always emits v2 + scrypt with
 * interactive-login defaults (N=2^14, r=8, p=1).  Readers still honour
 * KDFiterations when decrypting v1 Cryptlib-era files.
 */
bool iniWriteEncryptedFile(FILE* fp, const str_list_t list, enum iniCryptAlgo algo, int keySize, int KDFiterations, const char *key, char *salt)
{
	uint8_t salt_bin[INI_V2_SALT_BYTES];
	char    salt_hex[2 * INI_V2_SALT_BYTES + 1];
	struct xp_crypt_kdf_params kdf = {
		.kdf         = XP_CRYPT_KDF_SCRYPT,
		.cost_log2   = INI_SCRYPT_COST_LOG2,
		.block_size  = INI_SCRYPT_R,
		.parallelism = INI_SCRYPT_P,
	};
	xp_crypt_t ctx = NULL;
	char *buffer = NULL;
	size_t bufferSize = 0;
	size_t bufferPos = 0;
	bool streamCipher = false;
	bool ok = false;
	size_t line = 0;
	int blocksize;
	int ivsize;
	(void)KDFiterations;
	(void)salt;

	if (fp == NULL)
		return false;
	if (algo == INI_CRYPT_ALGO_NONE)
		return iniWriteFile(fp, list);
	if (key == NULL)
		return false;

	for (size_t i = 0; i < sizeof(salt_bin); i++)
		salt_bin[i] = (uint8_t)xp_random(256);
	hex_encode(salt_bin, sizeof(salt_bin), salt_hex);

	ctx = xp_crypt_open((int)algo, keySize, salt_bin, sizeof(salt_bin),
	                    &kdf, key, strlen(key), /*encrypt=*/true);
	if (ctx == NULL)
		return false;
	/* Resolve default key size when caller passed zero. */
	if (keySize <= 0) {
		switch (algo) {
			case INI_CRYPT_ALGO_AES:
			case INI_CRYPT_ALGO_CHACHA20: keySize = 256; break;
			case INI_CRYPT_ALGO_3DES:     keySize = 192; break;
			default:                      keySize = 128; break;
		}
	}

	blocksize = xp_crypt_blocksize(ctx);
	if (blocksize <= 1) {
		bufferSize = INI_MAX_LINE_LEN - 1;
		streamCipher = true;
	} else {
		bufferSize = (size_t)blocksize;
	}
	buffer = malloc(bufferSize);
	if (buffer == NULL)
		goto done;

	char kdf_spec[64];
	if (!format_kdf_spec(&kdf, kdf_spec, sizeof(kdf_spec)))
		goto done;
	rewind(fp);
	fprintf(fp, "%s%s-%d, KDF: %s, Salt: %s\n",
	        encryptedHeaderPrefixV2, iniCryptGetAlgoName(algo), keySize,
	        kdf_spec, salt_hex);

	/* Emit the IV header if the cipher carries one, before any
	   ciphertext. Matches the on-disk layout produced by the old
	   Cryptlib-backed writer: uint16_t be_ivsize + iv bytes. */
	ivsize = xp_crypt_ivsize(ctx);
	if (ivsize > 0) {
		unsigned char iv[XP_CRYPT_MAX_IVSIZE];
		uint16_t ivs;
		if ((size_t)ivsize > sizeof(iv))
			goto done;
		if (xp_crypt_gen_iv(ctx, iv, ivsize) != 0)
			goto done;
		ivs = htons((uint16_t)ivsize);
		if (fwrite(&ivs, 1, sizeof(ivs), fp) != sizeof(ivs))
			goto done;
		if (fwrite(iv, 1, ivsize, fp) != (size_t)ivsize)
			goto done;
	}

	if (list) {
		for (; list[line]; line++) {
			size_t strPos;
			for (strPos = 0; list[line][strPos]; strPos++) {
				if (!addEncrpytedChar(ctx, list[line][strPos], buffer, bufferSize, &bufferPos, fp))
					goto done;
			}
			if (!addEncrpytedChar(ctx, '\n', buffer, bufferSize, &bufferPos, fp))
				goto done;
		}
	}
	if (bufferPos) {
		if (streamCipher) {
			if (xp_crypt_process(ctx, buffer, bufferPos) != 0)
				goto done;
			if (fwrite(buffer, 1, bufferPos, fp) != bufferPos)
				goto done;
			bufferPos = 0;
		}
		else {
			/* Zero-pad the final block. */
			while (bufferPos) {
				if (!addEncrpytedChar(ctx, 0, buffer, bufferSize, &bufferPos, fp)) {
					line--;
					goto done;
				}
			}
		}
	}
	ok = true;

done:
	free(buffer);
	xp_crypt_close(ctx);
	if (!ok)
		return false;
	return line == strListCount(list);
}
