#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "tith-config.h"
#include "tith-common.h"
#include "tith-file.h"
#include "tith-interface.h"
#include "tith-strings.h"

thread_local struct TITH_TLV *tith_TLV;
thread_local struct TITH_TLV *tail;
thread_local jmp_buf tith_exitJmpBuf;
thread_local void *tith_handle;

thread_local static void **allocStack;
thread_local static size_t allocStackSize;
thread_local static size_t allocStackUsed;
#define NO_NEXT_TYPE -1
thread_local static int nextType = NO_NEXT_TYPE;

struct ActiveSignature {
	uint8_t *signatureStorage;
	struct hydro_sign_keypair *kp;
	hydro_sign_state st;
};

static thread_local struct ActiveSignature *activeSignatures;
static thread_local size_t activeSignaturesSize;
static thread_local size_t activeSignaturesCount;

static void freeTLV(struct TITH_TLV *tlv);

/*
 * The TLV API here works as follows:
 * tith_getTLV() will allocate a root TLV, there may only be one root
 * TLV at a time, which is available as tith_TLV.
 * 
 * tith_getNextTLV(struct TITH_TLV *cur, enum TITH_Type, bool required)
 * will attempt to fetch a TLV into the next pointer of cur.  If required
 * is true, it is an error if a TLV of that type is not next.  Returns
 * the appropriate value for cur for the next call to tith_getNextTLV()
 * 
 * tith_parseTLV(struct TITH_TLV *tlv)
 * Will parse all the data currently in tlv as a TLV sequence
 * 
 * tith_allocTLV() will allocate a root TLV.  There may only be one root
 * TLV at a time, which is available as tith_TLV.
 * 
 * tith_addData(struct TITH_TLV *tlv, enum TITH_Type type, uint64_t len,
 * void *data, bool child)
 * Copies len bytes from data into a newly allocated tith_TLV * of type
 * type which becomes either the last TLV in the next chain of tlv or
 * the child depending on the value of child.
 * 
 * tith_addFile(struct TITH_TLV *tlv, enum TITH_Type type, const char *
 * filename, bool child)
 * As above, but adds the contents of a file, memory is not allocated.
 * 
 * If ever needed, the get*() interface could be extended to write values
 * over a certain length to files instead of allocating memory.
 */

/*
 * Allocation stack.  When a function returns an allocated pointer,
 * you must call tith_pushAlloc(), which will ensure the pointer is not
 * NULL and push it onto the stack.
 * 
 * Before you free() or return the value, you must call tith_popAlloc()
 */
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

/*
 * Closes the current TITH handle and frees all allocations
 */
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
	allocStack = NULL;
	if (tith_TLV)
		tith_freeTLV();
	free(activeSignatures);
	activeSignatures = NULL;
}

/*
 * Logs the specified error and terminates TITH
 */
noreturn void
tith_logError(const char *str)
{
	logString(str);
	tith_cleanup();
	longjmp(tith_exitJmpBuf, EXIT_FAILURE);
}

/*
 * Parses a TTS-0002 number from a buffer starting at *offset.
 * Updates *offset to point to the first byte after the decoded number.
 */
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

static int
parseType(uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	uint64_t type = parseNumber(buf, offset, sz);
	if (type > INT_MAX)
		tith_logError("Type too large");
	return (int)type;
}

/*
 * Reads a TTS-0003 number from the current handle.
 */
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

static int
getType(uint64_t *remain)
{
	if (nextType != NO_NEXT_TYPE) {
		int ret = nextType;
		nextType = NO_NEXT_TYPE;
		return ret;
	}
	uint64_t type = getNumber(remain);
	if (type > INT_MAX)
		tith_logError("Type too large");
	return (int)type;
}

/*
 * Allocates a TLV with enough storage for len bytes
 */
static struct TITH_TLV *
allocTLVBuffer(int type, uint64_t len, struct TITH_TLV *first, struct TITH_TLV *parent)
{
	struct TITH_TLV *ret = malloc(sizeof(struct TITH_TLV) + len);
	if (ret == NULL)
		tith_logError("malloc() failure in allocTLV()");
	if (first == NULL)
		first = ret;
	ret->first = first;
	ret->next = NULL;
	ret->child = NULL;
	ret->parent = parent;
	ret->fileName = NULL;
	ret->length = len;
	ret->type = type;
	ret->parsed = false;
	ret->added = false;
	ret->value = (uint8_t *)&ret->value;
	ret->value += sizeof(ret->value);
	return ret;
}

/*
 * Allocates a TLV and sets the value to data
 */
static struct TITH_TLV *
allocTLVData(int type, uint64_t len, uint8_t *data, struct TITH_TLV *first, struct TITH_TLV *parent)
{
	struct TITH_TLV *ret = malloc(sizeof(struct TITH_TLV) + len);
	if (ret == NULL)
		tith_logError("malloc() failure in allocTLV()");
	if (first == NULL)
		first = ret;
	ret->first = first;
	ret->next = NULL;
	ret->child = NULL;
	ret->parent = parent;
	ret->fileName = NULL;
	ret->length = len;
	ret->type = type;
	ret->parsed = false;
	ret->value = data;
	return ret;
}

static struct TITH_TLV *
findOrigin(struct TITH_TLV *tlv)
{
	struct TITH_TLV *origin = NULL;
	struct TITH_TLV *end = tlv;
	struct TITH_TLV *cur = tlv->first;

	/*
	 * o is a potential origin, we need the most recent one
	 * though, so in the case of Origin, Origin, we need the
	 * second one.
	 */
	struct TITH_TLV *o = NULL;
	while (origin == NULL) {
		if (cur == end) {
			if (o)
				origin = o;
			else {
				if (cur->parent == NULL)
					tith_logError("Unable to find Origin for SignedTLV");
				end = cur->parent;
				cur = cur->parent->first;
			}
		}
		else if (cur == NULL)
			tith_logError("Unable to find Origin for SignedTLV");
		else if (cur->type == TITH_Origin)
			o = cur;
	}
	return origin;
}

static void
checkForSignedTLV(struct TITH_TLV *tlv)
{
	if (tlv->type != TITH_SignedTLV)
		return;
	// Since we know this is a sequence, go ahead and parse it...
	tith_parseTLV(tlv);
	// Now find the origin...
	struct TITH_TLV *origin = NULL;
	if (tlv->child->type == TITH_Origin)
		origin = tlv->child;
	else
		origin = findOrigin(tlv);
	// Now that we have an origin, look up the public key for it
	/*
	 * TODO: We need this interface in tith-config
	 *       Basically, we need a nodelist per zone to be parsed
	 *       so we have the nodelist deets available.
	 *       WE only need the public key, but SRIF will need
	 *       Sysop Name, and possibly other fields if we want to lie
	 *       about where we got them.
	 */
}

void
tith_parseTLV(struct TITH_TLV *tlv)
{
	if (tlv->parsed)
		return;
	uint64_t offset = 0;
	struct TITH_TLV **tail = &tlv->child;
	struct TITH_TLV *first = NULL;
	while (offset < tlv->length) {
		int type = parseType(tlv->value, &offset, tlv->length);
		uint64_t length = parseNumber(tlv->value, &offset, tlv->length);
		struct TITH_TLV *newTail = allocTLVData(type, length, &tlv->value[offset], first, tlv);
		offset += length;
		*tail = newTail;
		tail = &newTail->next;
		first = newTail->first;
	}
	tlv->parsed = true;
}

void
tith_getTLV(void)
{
	if (tith_TLV)
		tith_logError("Root TLV already present");
	int type = getType(NULL);
	uint64_t length = getNumber(NULL);
	tith_TLV = allocTLVBuffer(type, length, NULL, NULL);
	getBytes(tith_handle, tith_TLV->value, length);
	checkForSignedTLV(tith_TLV);
}

struct TITH_TLV *
tith_getNextTLV(struct TITH_TLV *tlv, int type, bool required)
{
	if (!tlv)
		tith_logError("NULL tlv passed to tith_getNextTLV()");
	if (tlv->next)
		tith_logError("tlv already has next TLV");
	int tlvType = getType(NULL);
	if (tlvType != type) {
		if (required)
			tith_logError("Required next type not present");
		nextType = tlvType;
		return tlv;
	}
	uint64_t length = getNumber(NULL);
	tlv->next = allocTLVBuffer(type, length, NULL, NULL);
	getBytes(tith_handle, tlv->next->value, length);
	checkForSignedTLV(tlv->next);
	return tlv->next;
}

/*
 * Recursive, depth-first free
 */
static void
freeTLV(struct TITH_TLV *tlv)
{
	if (tlv->child)
		freeTLV(tlv->child);
	if (tlv->next)
		freeTLV(tlv->next);
	free(tlv->fileName);
	free(tlv);
}

void
tith_freeTLV(void)
{
	if (!tith_TLV)
		tith_logError("Attempt to free unallocated root TLV");
	freeTLV(tith_TLV);
	tith_TLV = NULL;
}

static int16_t
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
	return (int16_t)l;
}

static void
checkComponent(const char **addr, int16_t zone, char prefix)
{
	if (**addr == prefix) {
		if (zone == -1)
			tith_logError("Additional component specified in zone -1");
		(*addr)++;
		int16_t component = validateAddrPart(addr);
		if (component < 1)
			tith_logError("Invalid component");
	}
}

void
tith_validateAddress(const char *addr)
{
	/*
	 * See TTS-0004
	 */
	while(*addr != '#') {
		if ((unsigned char)*addr < ' ' || (unsigned char)*addr == 0x7F || (unsigned char)*addr == ',')
			tith_logError("Invalid domain character");
		addr++;
	};
	addr++;
	int16_t zone = validateAddrPart(&addr);
	if (zone < -1 || zone == 0)
		tith_logError("Invalid zone");
	checkComponent(&addr, zone, ':');
	checkComponent(&addr, zone, '/');
	checkComponent(&addr, zone, '.');
	if (*addr)
		tith_logError("Garbage at end");
}

void
tith_allocTLV(int type)
{
	if (tith_TLV)
		tith_logError("Attempting to alloc a second root TLV");
	tith_TLV = allocTLVBuffer(type, 0, NULL, NULL);
	tith_TLV->value = NULL;
	tith_TLV->added = true;
}

static unsigned
lengthLen(uint64_t num)
{
	unsigned ret = 1;
	while (num > 127) {
		ret++;
		num >>= 7;
	}
	return ret;
}

static unsigned
typeLen(int num)
{
	if (num < 0)
		tith_logError("Negative type value");
	return lengthLen((uint64_t)num);
}

struct TITH_TLV *
tith_addData(struct TITH_TLV *tlv, int type, uint64_t len, void *data, bool child)
{
	if (child && tlv->child)
		tith_logError("TLV already has a child");
	if (!child && tlv->next)
		tith_logError("TLV already has a next item");
	struct TITH_TLV *newTlv = allocTLVBuffer(type, len, child ? NULL : tlv->first, child ? tlv : tlv->parent);
	memcpy(newTlv->value, data, len);
	newTlv->added = true;
	if (child)
		tlv->child = newTlv;
	else
		tlv->next = newTlv;
	uint64_t addLen = (uint64_t)len + typeLen(type) + lengthLen(len);
	for (struct TITH_TLV *parent = newTlv->parent; parent; parent = parent->parent)
		parent->length += addLen;
	return newTlv;
}

struct TITH_TLV *
tith_addFile(struct TITH_TLV *tlv, int type, const char *filename, bool child)
{
	if (child && tlv->child)
		tith_logError("TLV already has a child");
	if (!child && tlv->next)
		tith_logError("TLV already has a next item");
	struct TITH_TLV *newTlv = allocTLVBuffer(type, 0, child ? NULL : tlv->first, child ? tlv : tlv->parent);
	newTlv->value = NULL;
	newTlv->added = true;
	long len = tith_flen(filename);
	if (len < 0)
		tith_logError("Unable to get file length");
	newTlv->fileName = tith_strDup(filename);
	if (child)
		tlv->child = newTlv;
	else
		tlv->next = newTlv;
	uint64_t addLen = (uint64_t)len + typeLen(type) + lengthLen((uint64_t)len);
	for (struct TITH_TLV *parent = tlv->parent; parent; parent = parent->parent)
		parent->length += addLen;
	return newTlv;
}

static unsigned
sendNumber(uint64_t num)
{
	unsigned used = lengthLen(num);
	if (used > INT_MAX)
		tith_logError("Oversized number encountered");
	for (int len = (int)used - 1; len >= 0; len--) {
		uint8_t b = (num >> (7 * len)) & 0x7F;
		if (len)
			b |= 0x80;
		sendByte(tith_handle, b);
		for (size_t i = 0; i < activeSignaturesCount; i++)
			hydro_sign_update(&activeSignatures[i].st, &b, 1);
	}
	return used;
}

static uint64_t
sendBuffer(uint8_t *buf, uint64_t len)
{
	sendBytes(tith_handle, buf, len);
	for (size_t i = 0; i < activeSignaturesCount; i++)
		hydro_sign_update(&activeSignatures[i].st, buf, len);
	return len;
}

static uint64_t
sendFile(const char *fname, uint64_t len)
{
	FILE *fp = fopen(fname, "rb");
	if (!fp)
		tith_logError("Unable to open file");
	size_t remain = len;
	for (;;) {
		uint8_t buf[1024];
		size_t bytes = remain > sizeof(buf) ? sizeof(buf) : remain;
		if (bytes == 0)
			break;
		size_t ret = fread(buf, 1, bytes, fp);
		if (ret != bytes) {
			fclose(fp);
			tith_logError("Failed to read file");
		}
		sendBuffer(buf, ret);
		remain -= ret;
	}
	return len;
}

static uint64_t
sendTLV(struct TITH_TLV *tlv)
{
	if (tlv == NULL)
		tith_logError("Attempting to send NULL TLV");
	uint64_t used = sendNumber((uint64_t)tlv->type);
	used += sendNumber(tlv->length);
	struct ActiveSignature *as = NULL;
	struct TITH_TLV *origin = NULL;
	
	if (tlv->type == TITH_SignedData && tlv->added) {
		origin = findOrigin(tlv);
		// TODO: Ensure origin has a secret key
 		if (tlv->next == NULL)
			tith_logError("Sending SignedData at end of sequence");
		if (tlv->next->type != TITH_Signature)
			tith_logError("Sending SignedData that's not followed by Signature");
		if (tlv->next->length != hydro_sign_BYTES)
			tith_logError("Sending SignedData followed by Signature with wrong size");
		if (!tlv->next->value)
			tith_logError("Sending SignedData followed by Signature without storage");
		if (activeSignaturesSize <= activeSignaturesCount) {
			struct ActiveSignature *newSigs = realloc(activeSignatures, sizeof(struct ActiveSignature) * (activeSignaturesCount + 1));
			if (newSigs == NULL)
				tith_logError("Unable to allocated new ActiveSignature");
			activeSignatures = newSigs;
		}
		as = &activeSignatures[activeSignaturesCount++];
		as->signatureStorage = tlv->next->value;
		hydro_sign_init(&as->st, "SignTLV");
		// TODO: Set the keypair here...
	}
	// If we have the value in memory, send it...
	if (tlv->value)
		used += sendBuffer(tlv->value, tlv->length);
	// If it's in a file, send that...
	else if (tlv->fileName)
		used += sendFile(tlv->fileName, tlv->length);
	// Otherwise, it must have children... send those
	else
		used += sendTLV(tlv->child);
	if (as) {
		// Finish up the signature we started earlier
		hydro_sign_final_create(&as->st, as->signatureStorage, as->kp->sk);
	}
	return used;
}

/*
 * Sends the root TLV
 */
void
tith_sendTLV(void)
{
	// TODO: Ensure lengths all match up!
	if (sendTLV(tith_TLV) != tith_TLV->length + typeLen(tith_TLV->type) + lengthLen(tith_TLV->length))
		tith_logError("Length mismatch!");
	flushWrite(tith_handle);
}

void
tith_validateTLV(int command, int numargs, ...)
{
	struct TITH_TLV *tlv = tith_TLV;
	va_list ap;
	va_start(ap, numargs);
	if (tlv->type != command)
		tith_logError("Incorrect top-level type");
	tlv = tlv->child;
	for (int i = 0; i < numargs; i++) {
		int required = va_arg(ap, int);
		int type = va_arg(ap, int);
		if (tlv == NULL || tlv->type != type) {
			if (required)
				tith_logError("Missing required type");
		}
		else {
			tlv = tlv->next;
		}
	}
}
