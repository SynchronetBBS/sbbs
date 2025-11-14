#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tith-common.h"
#include "tith-interface.h"

hydro_kx_session_keypair tith_sessionKeyPair;
bool tith_encrypting;

static uint64_t rxMsgId;
static uint64_t txMsgId;
void *tith_handle;
jmp_buf tith_exitJmpBuf;

char *tith_strDup(const char *str)
{
	size_t sz = strlen(str);
	char *ret = malloc(sz + 1);
	if (ret == NULL)
		return ret;
	strcpy(ret, str);
	return ret;
}

noreturn void
tith_logError(const char *str)
{
	logString(str);
	closeConnection(tith_handle);
	longjmp(tith_exitJmpBuf, EXIT_FAILURE);
}

static uint64_t
parseNumber(uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	uint64_t ret = 0;
	size_t gotbits = 0;

	for (;;) {
		if (*offset >= sz)
			tith_logError("Reading number past end");
		uint8_t ch = buf[(*offset)++];
		gotbits += 7;
		if (gotbits > 63)
			tith_logError("Number with more than 63 bits received in parseNumber()");
		ret <<= 7;
		ret |= (ch & 0x7f);
		if (!(ch & 0x80))
			break;
	}
	return ret;
}

static enum TITH_Type
parseType(uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	uint64_t num = parseNumber(buf, offset, sz);
	if (num < TITH_FIRST_TYPE || num > TITH_LAST_TYPE)
		tith_logError("Type out of range");
	return (enum TITH_Type)num;
}

static enum TITH_Type
parseCmd(uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	uint64_t num = parseNumber(buf, offset, sz);
	if (num < TITH_FIRST_CMD || num > TITH_LAST_CMD)
		tith_logError("Command out of range");
	return (enum TITH_Type)num;
}

static uint64_t
getNumber(uint64_t *remain)
{
	uint64_t ret = 0;
	size_t gotbits = 0;

	for (;;) {
		if (remain && *remain == 0)
			tith_logError("Reading number past end");
		int ch = getByte(tith_handle);
		if (remain)
			*remain -= 1;
		if (ch == -1)
			tith_logError("EOF on getchar() in getNumber()");
		gotbits += 7;
		if (gotbits > 63)
			tith_logError("Number with more than 63 bits received in getNumber()");
		ret <<= 7;
		ret |= (ch & 0x7f);
		if (!(ch & 0x80))
			break;
	}
	return ret;
}

static enum TITH_Type
getCmd(uint64_t *remain)
{
	uint64_t num = getNumber(remain);
	if (num < TITH_FIRST_CMD || num > TITH_LAST_CMD)
		tith_logError("Invalid command type");
	return (enum TITH_Type)num;
}

static struct TITH_TLV *
allocTLV(enum TITH_Type type, uint64_t len)
{
	struct TITH_TLV *ret = malloc(sizeof(struct TITH_TLV) + len);
	if (ret == NULL)
		tith_logError("malloc() failure in allocTLV()");
	ret->next = NULL;
	ret->tail = ret;
	ret->type = type;
	ret->length = len;
	ret->value = (uint8_t *)&ret->value;
	ret->value += sizeof(ret->value);
	return ret;
}

void
tith_freeTLV(struct TITH_TLV *tlv)
{
	while (tlv) {
		struct TITH_TLV *next = tlv->next;
		free(tlv);
		tlv = next;
	}
}

static struct TITH_TLV *
parseTLV(uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	enum TITH_Type type = parseType(buf, offset, sz);
	uint64_t length = parseNumber(buf, offset, sz);
	if (length > sz - *offset)
		tith_logError("Value extends past length");
	struct TITH_TLV *ret = allocTLV(type, length);
	ret->value = &buf[*offset];
	*offset += length;
	return ret;
}

struct TITH_TLV *
tith_getCmd(void)
{
	enum TITH_Type type = getCmd(NULL);
	if (tith_encrypting && type != TITH_Encrypted)
		tith_logError("Non-encrypted command on encrypted channel");
	uint64_t len = getNumber(NULL);
	struct TITH_TLV *ret = allocTLV(type, len);
	if (!getBytes(tith_handle, ret->value, len))
		tith_logError("Error reading command");
	if (tith_encrypting) {
		uint64_t plainTextLen = len - hydro_secretbox_HEADERBYTES;
		uint8_t *plainText = malloc(plainTextLen);
		if (plainText == NULL)
			tith_logError("malloc() error in getCmd()");
		if (hydro_secretbox_decrypt(plainText, ret->value, ret->length, rxMsgId++, "TITHctx", tith_sessionKeyPair.rx))
			tith_logError("decryption failure");
		tith_freeTLV(ret);
		uint64_t offset = 0;
		type = parseCmd(plainText, &offset, plainTextLen);
		if (type == TITH_Encrypted)
			tith_logError("Invalid encrypted command type");
		len = parseNumber(plainText, &offset, plainTextLen);
		if (len != plainTextLen - offset)
			tith_logError("Bad plainTextLen");
		ret = allocTLV(type, len);
		memcpy(ret->value, &plainText[offset], len);
		free(plainText);
	}
	uint64_t offset = 0;
	while (offset < len) {
		struct TITH_TLV *param = parseTLV(ret->value, &offset, ret->length);
		ret->tail->next = param;
		ret->tail = param;
	}
	return ret;
}

struct TITH_TLV *
tith_allocCmd(enum TITH_Type type)
{
	if (type < TITH_FIRST_CMD || type > TITH_LAST_CMD)
		tith_logError("Invalid type to tith_allocCmd()");
	struct TITH_TLV *ret = allocTLV(type, 0);
	ret->type = type;
	return ret;
}

static unsigned
numLen(uint64_t num)
{
	unsigned ret = 1;
	while (num > 127) {
		ret++;
		num >>= 7;
	}
	return ret;
}

void
tith_addData(struct TITH_TLV *cmd, enum TITH_Type type, uint64_t len, void *data)
{
	struct TITH_TLV *newData = allocTLV(type, len);
	memcpy(newData->value, data, len);
	cmd->tail->next = newData;
	cmd->tail = newData;
	cmd->length += (uint64_t)numLen((uint64_t)type);
	cmd->length += (uint64_t)numLen(len);
	cmd->length += len;
}

static unsigned
sendNumber(uint64_t num, uint8_t *buf)
{
	unsigned used = numLen(num);
	if (used > INT_MAX)
		tith_logError("Oversized number encountered");
	for (int len = (int)used - 1; len >= 0; len--) {
		uint8_t b = (num >> (7 * len)) & 0x7F;
		if (len)
			b |= 0x80;
		*(buf++) = b;
	}
	return used;
}

void
tith_sendCmd(struct TITH_TLV *cmd)
{
	uint64_t len = cmd->length;
	len += numLen(cmd->type);
	len += numLen(cmd->length);
	uint8_t *buffer = malloc(len);
	if (buffer == NULL)
		tith_logError("maloc() failure in tith_sendCmd()");
	uint64_t offset = 0;
	offset += sendNumber(cmd->type, &buffer[offset]);
	offset += sendNumber(cmd->length, &buffer[offset]);
	for (struct TITH_TLV *tlv = cmd->next; tlv; tlv = tlv->next) {
		offset += sendNumber(tlv->type, &buffer[offset]);
		offset += sendNumber(tlv->length, &buffer[offset]);
		memcpy(&buffer[offset], tlv->value, tlv->length);
		offset += tlv->length;
	}
	if (offset != len)
		tith_logError("offset/len mismatch!");
	if (tith_encrypting) {
		uint64_t encryptedValueLen = len + hydro_secretbox_HEADERBYTES;
		uint64_t encryptedLen = encryptedValueLen;
		encryptedLen += numLen(TITH_Encrypted);
		encryptedLen += numLen(encryptedValueLen);
		uint8_t *encBuffer = malloc(encryptedLen);
		if (encBuffer == NULL)
			tith_logError("malloc() failure creating crypt buffer");
		offset = 0;
		offset += sendNumber(TITH_Encrypted, &encBuffer[offset]);
		offset += sendNumber(encryptedValueLen, &encBuffer[offset]);
		if (hydro_secretbox_encrypt(&encBuffer[offset], buffer, len, txMsgId++, "TITHctx", tith_sessionKeyPair.tx))
			tith_logError("encrypt() failure");
		free(buffer);
		if (!sendBytes(tith_handle, encBuffer, encryptedLen))
			tith_logError("Failed to write TLV data");
	}
	else {
		if (!sendBytes(tith_handle, buffer, offset))
			tith_logError("Failed to write TLV data");
		free(buffer);
	}
	if (!flushWrite(tith_handle))
		tith_logError("fflush(stdout) failed");
}

static void
validateAddrPart(const char **addr)
{
	errno = 0;
	char *endptr;
	long l = strtol(*addr, &endptr, 10);
	if (endptr == *addr)
		tith_logError("No number found");
	if (l > INT16_MAX)
		tith_logError("Number too lage");
	if (l < INT16_MIN)
		tith_logError("Number too small");
	if (l && **addr == '0')
		tith_logError("Leading zero");
	*addr = endptr;
}

void
tith_validateAddress(const char *addr)
{
	/*
	 * A valid address is the following sequence
	 * ANY NUMBER of alpha-numeric characters
	 * TODO: Allow UTF-8
	 * TODO: Case? Tough with Unicode, mostly pointless
	 * A hash (#)
	 * A 16-bit signed integer in decimal format without leading zeros
	 * A colon (:)
	 * A 16-bit signed integer in decimal format without leading zeros
	 * A slash (/)
	 * A 16-bit signed integer in decimal format without leading zeros
	 * The following two may be left off
	 * A period (.)
	 * A 16-bit signed integer in decimal format without leading zeros
	 */
	do {
		if (!(isdigit(*addr) || isalpha(*addr)))
			tith_logError("Invalid domain character");
		addr++;
	} while(*addr != '#');
	addr++;
	errno = 0;
	validateAddrPart(&addr);
	if (*addr != ':')
		tith_logError("Garbage after zone");
	addr++;
	validateAddrPart(&addr);
	if (*addr != '/')
		tith_logError("Garbage after net/region");
	addr++;
	validateAddrPart(&addr);
	if (*addr == 0)
		return;
	if (*addr != '.')
		tith_logError("Garbage after node");
	addr++;
	validateAddrPart(&addr);
	if (*addr)
		tith_logError("Garbage after point");
}

void
tith_validateCmd(struct TITH_TLV *tlv, enum TITH_Type command, int numargs, ...)
{
	va_list ap;
	va_start(ap, numargs);
	if (tlv->type != command)
		tith_logError("Invalid command");
	for (int i = 0; i < numargs; i++) {
		int required = va_arg(ap, int);
		enum TITH_Type type = va_arg(ap, enum TITH_Type);
		if (tlv->next == NULL || tlv->next->type != type) {
			if (required)
				tith_logError("Missing required type");
		}
		else {
			tlv = tlv->next;
		}
	}
}
