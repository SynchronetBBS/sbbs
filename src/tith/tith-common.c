#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tith-common.h"
#include "tith-config.h"
#include "tith-interface.h"

thread_local hydro_kx_session_keypair tith_sessionKeyPair;
thread_local bool tith_encrypting;
thread_local static uint64_t rxMsgId;
thread_local static uint64_t txMsgId;
thread_local static void **allocStack;
thread_local static size_t allocStackSize;
thread_local static size_t allocStackUsed;
thread_local struct TITH_TLV *tith_cmd;
thread_local void *tith_handle;
thread_local jmp_buf tith_exitJmpBuf;

static void freeTLV(struct TITH_TLV *tlv);

void
tith_pushAlloc(void *ptr)
{
	if (ptr == NULL)
		tith_logError("Allocation failure");
	if (allocStackUsed + 1 > allocStackSize) {
		size_t newSz = allocStackSize ? allocStackSize * 2 : 8;
		void **newStack = realloc(allocStack, sizeof(void *) * newSz);
		if (newStack == NULL)
			tith_logError("Unable to realloc() allocStack");
		allocStackSize = newSz;
		allocStack = newStack;
	}
	allocStack[allocStackUsed++] = ptr;
}

void *
tith_popAlloc(void)
{
	if (allocStackUsed == 0)
		tith_logError("Popping of empty allocStack");
	return allocStack[--allocStackUsed];
}

char *tith_strDup(const char *str)
{
	size_t sz = strlen(str);
	char *ret = malloc(sz + 1);
	if (ret == NULL)
		return ret;
	strcpy(ret, str);
	return ret;
}

void
tith_cleanup(void)
{
	closeConnection(tith_handle);
	tith_handle = NULL;
	tith_freeConfig();
	cfg = NULL;
	while (allocStackUsed)
		free(tith_popAlloc());
	free(allocStack);
	if (tith_cmd)
		tith_freeCmd();
}

noreturn void
tith_logError(const char *str)
{
	logString(str);
	tith_cleanup();
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
	if (tith_cmd)
		tith_logError("Attempting to get a second command");
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

static void
freeTLV(struct TITH_TLV *tlv)
{
	while (tlv) {
		struct TITH_TLV *next = tlv->next;
		free(tlv);
		tlv = next;
	}
}

void
tith_freeCmd(void)
{
	if (!tith_cmd)
		tith_logError("Attempt to free unallocated tith_cmd");
	freeTLV(tith_cmd);
	tith_cmd = NULL;
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

void
tith_getCmd(void)
{
	enum TITH_Type type = getCmd(NULL);
	if (tith_encrypting && type != TITH_Encrypted)
		tith_logError("Non-encrypted command on encrypted channel");
	uint64_t len = getNumber(NULL);
	tith_cmd = allocTLV(type, len);
	if (!getBytes(tith_handle, tith_cmd->value, len))
		tith_logError("Error reading command");
	if (tith_encrypting) {
		uint64_t plainTextLen = len - hydro_secretbox_HEADERBYTES;
		uint8_t *plainText = malloc(plainTextLen);
		tith_pushAlloc(plainText);
		if (hydro_secretbox_decrypt(plainText, tith_cmd->value, tith_cmd->length, rxMsgId++, "TITHctx", tith_sessionKeyPair.rx))
			tith_logError("decryption failure");
		tith_freeCmd();
		tith_cmd = NULL;
		uint64_t offset = 0;
		type = parseCmd(plainText, &offset, plainTextLen);
		if (type == TITH_Encrypted)
			tith_logError("Invalid encrypted command type");
		len = parseNumber(plainText, &offset, plainTextLen);
		if (len != plainTextLen - offset)
			tith_logError("Bad plainTextLen");
		tith_cmd = allocTLV(type, len);
		memcpy(tith_cmd->value, &plainText[offset], len);
		tith_popAlloc();
		free(plainText);
	}
	uint64_t offset = 0;
	while (offset < len) {
		struct TITH_TLV *param = parseTLV(tith_cmd->value, &offset, tith_cmd->length);
		tith_cmd->tail->next = param;
		tith_cmd->tail = param;
	}
}

void
tith_allocCmd(enum TITH_Type type)
{
	if (type < TITH_FIRST_CMD || type > TITH_LAST_CMD)
		tith_logError("Invalid type to tith_allocCmd()");
	if (tith_cmd)
		tith_logError("Attempting to alloc a second command");
	tith_cmd = allocTLV(type, 0);
	tith_cmd->type = type;
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
tith_addData(enum TITH_Type type, uint64_t len, void *data)
{
	struct TITH_TLV *newData = allocTLV(type, len);
	memcpy(newData->value, data, len);
	tith_cmd->tail->next = newData;
	tith_cmd->tail = newData;
	tith_cmd->length += (uint64_t)numLen((uint64_t)type);
	tith_cmd->length += (uint64_t)numLen(len);
	tith_cmd->length += len;
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
tith_sendCmd(void)
{
	uint64_t len = tith_cmd->length;
	len += numLen(tith_cmd->type);
	len += numLen(tith_cmd->length);
	uint8_t *buffer = malloc(len);
	tith_pushAlloc(buffer);
	uint64_t offset = 0;
	offset += sendNumber(tith_cmd->type, &buffer[offset]);
	offset += sendNumber(tith_cmd->length, &buffer[offset]);
	for (struct TITH_TLV *tlv = tith_cmd->next; tlv; tlv = tlv->next) {
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
		tith_pushAlloc(encBuffer);
		offset = 0;
		offset += sendNumber(TITH_Encrypted, &encBuffer[offset]);
		offset += sendNumber(encryptedValueLen, &encBuffer[offset]);
		if (hydro_secretbox_encrypt(&encBuffer[offset], buffer, len, txMsgId++, "TITHctx", tith_sessionKeyPair.tx))
			tith_logError("encrypt() failure");
		tith_popAlloc();
		free(buffer);
		if (!sendBytes(tith_handle, encBuffer, encryptedLen))
			tith_logError("Failed to write TLV data");
		tith_popAlloc();
		free(encBuffer);
	}
	else {
		if (!sendBytes(tith_handle, buffer, offset))
			tith_logError("Failed to write TLV data");
		tith_popAlloc();
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
	while(*addr != '#') {
		if ((unsigned char)*addr < ' ' || (unsigned char)*addr == 0x7F)
			tith_logError("Invalid domain character");
		addr++;
	};
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
tith_validateCmd(enum TITH_Type command, int numargs, ...)
{
	struct TITH_TLV *tlv = tith_cmd;
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
