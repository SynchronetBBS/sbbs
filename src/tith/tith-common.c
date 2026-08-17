#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#include "tith-config.h"
#include "tith-common.h"
#include "tith-file.h"
#include "tith-interface.h"
#include "tith-strings.h"

_Static_assert(hydro_hash_BYTES == TITH_ITEM_IDENTITY_BYTES,
    "Item identity size does not match libhydrogen hash size");

thread_local struct TITH_TLV *tith_TLV;
thread_local jmp_buf tith_exitJmpBuf;
thread_local void *tith_handle;

thread_local static void **allocStack;
thread_local static size_t allocStackSize;
thread_local static size_t allocStackUsed;
thread_local static FILE **fileStack;
thread_local static size_t fileStackSize;
thread_local static size_t fileStackUsed;

#define NO_NEXT_TYPE -1
thread_local static int nextType = NO_NEXT_TYPE;

static void freeTLV(struct TITH_TLV *tlv);
static void getOriginPublicKey(const struct TITH_TLV *origin,
    uint8_t pk[hydro_sign_PUBLICKEYBYTES]);
static struct TITH_Node *getSigningNode(const struct TITH_TLV *origin);
static void validateUTF8(const uint8_t *value, size_t length);

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

void
tith_pushFile(FILE *file)
{
	if (file == NULL)
		tith_logError("Open failure");
	if (fileStackUsed + 1 > fileStackSize) {
		size_t newSz = fileStackSize ? fileStackSize * 2 : 8;
		FILE **newStack = realloc(fileStack, sizeof(void *) * newSz);
		if (newStack == NULL)
			tith_logError("Unable to realloc() fileStack");
		fileStackSize = newSz;
		fileStack = newStack;
	}
	fileStack[fileStackUsed++] = file;
}

FILE *
tith_popFile(void)
{
	if (fileStackUsed == 0)
		tith_logError("Popping of empty fileStack");
	return fileStack[--fileStackUsed];
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
	allocStackSize = 0;
	while (fileStackUsed)
		fclose(tith_popFile());
	free(fileStack);
	fileStack = NULL;
	fileStackSize = 0;
	if (tith_TLV)
		tith_freeTLV();
	nextType = NO_NEXT_TYPE;
}

/*
 * Logs the specified error and terminates TITH
 */
noreturn void
tith_logError(const char *str)
{
	logString(str);
	longjmp(tith_exitJmpBuf, EXIT_FAILURE);
}

/*
 * Parses a TTS-0002 number from a buffer starting at *offset.
 * Updates *offset to point to the first byte after the decoded number.
 */
static uint64_t
parseNumber(const uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	uint64_t ret = 0;
	bool first = true;

	for (;;) {
		if (*offset >= sz)
			tith_logError("Reading number past end");
		uint8_t ch = buf[(*offset)++];
		if (first && (ch & 0x80) && !(ch & 0x7f))
			tith_logError("Number is not in its shortest form");
		first = false;
		if (ret > (UINT64_MAX >> 7))
			tith_logError("Number is too large");
		ret <<= 7;
		ret |= (ch & 0x7f);
		if (!(ch & 0x80))
			break;
	}
	return ret;
}

static int
parseType(const uint8_t *buf, uint64_t *offset, uint64_t sz)
{
	uint64_t type = parseNumber(buf, offset, sz);
	if (type == 0)
		tith_logError("Invalid TLV type zero");
	if (type > INT_MAX)
		tith_logError("Type too large");
	return (int)type;
}

/*
 * Reads a TTS-0002 number from the current handle.
 */
static uint64_t
getNumber(uint64_t *remain)
{
	uint64_t ret = 0;
	bool first = true;

	for (;;) {
		if (remain && *remain == 0)
			tith_logError("Reading number past end");
		int ch = getByte(tith_handle);
		if (remain)
			*remain -= 1;
		if (ch == -1)
			tith_logError("EOF on getchar() in getNumber()");
		if (first && (ch & 0x80) && !(ch & 0x7f))
			tith_logError("Number is not in its shortest form");
		first = false;
		if (ret > (UINT64_MAX >> 7))
			tith_logError("Number is too large");
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
	if (type == 0)
		tith_logError("Invalid TLV type zero");
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
	if (len > SIZE_MAX - sizeof(struct TITH_TLV))
		tith_logError("TLV too large");
	struct TITH_TLV *ret = malloc(sizeof(struct TITH_TLV) + (size_t)len);
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
	ret->signaturePrepared = false;
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
	struct TITH_TLV *ret = malloc(sizeof(struct TITH_TLV));
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
	ret->signaturePrepared = false;
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
		else if (cur->type == TITH_Origin) {
			o = cur;
			cur = cur->next;
		}
		else
			cur = cur->next;
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
	tith_validateTLV(tlv, TITH_SignedTLV, 3, TITH_OPTIONAL, TITH_Origin, TITH_REQUIRED, TITH_SignedData, TITH_REQUIRED, TITH_Signature);
	// Now find the origin...
	struct TITH_TLV *origin = NULL;
	struct TITH_TLV *sdata = NULL;
	if (tlv->child->type == TITH_Origin) {
		origin = tlv->child;
		sdata = tlv->child->next;
	}
	else {
		origin = findOrigin(tlv);
		sdata = tlv->child;
	}
	if (sdata->next->length != hydro_sign_BYTES)
		tith_logError("Signature has wrong size");
	// Now that we have an origin, find the public key applicable to it
	uint8_t pk[hydro_sign_PUBLICKEYBYTES];
	getOriginPublicKey(origin, pk);
	// And validate...
	if (hydro_sign_verify(sdata->next->value, sdata->value, sdata->length, "SignTLV", pk))
		tith_logError("Signature failed to validate!");
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
		if (length > tlv->length - offset)
			tith_logError("TLV length exceeds container");
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
	if (length && !getBytes(tith_handle, tith_TLV->value, (size_t)length))
		tith_logError("Failed to read TLV");
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
	tlv->next = allocTLVBuffer(type, length, tlv->first, tlv->parent);
	if (length && !getBytes(tith_handle, tlv->next->value, (size_t)length))
		tith_logError("Failed to read TLV");
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

static long
validateAddrPart(const char **addr)
{
	const char *start = *addr;
	errno = 0;
	char *endptr;
	long l = strtol(*addr, &endptr, 10);
	if (endptr == *addr)
		tith_logError("No number found");
	if (errno == ERANGE || l > UINT16_MAX)
		tith_logError("Number too large");
	if (l < -1)
		tith_logError("Number too small");
	if (*start == '+')
		tith_logError("Leading plus sign");
	if (*start == '-' && (l != -1 || endptr != start + 2))
		tith_logError("Invalid negative number");
	if (*start == '0' && endptr != start + 1)
		tith_logError("Leading zero");
	*addr = endptr;
	return l;
}

static void
checkComponent(const char **addr, long zone, char prefix, long maximum)
{
	if (**addr == prefix) {
		if (zone == -1)
			tith_logError("Additional component specified in zone -1");
		(*addr)++;
		long component = validateAddrPart(addr);
		if (component < 1 || component > maximum)
			tith_logError("Invalid component");
	}
}

void
tith_validateAddress(const char *addr)
{
	/*
	 * See TTS-0004
	 */
	if (addr == NULL || *addr == 0 || *addr == '#' || isspace((unsigned char)*addr))
		tith_logError("Invalid domain");
	const char *domain = addr;
	while(*addr && *addr != '#') {
		unsigned char ch = (unsigned char)*addr;
		if (ch < ' ' || ch == 0x7F || ch == '*' || ch == ',' || ch == '<' || ch == '>')
			tith_logError("Invalid domain character");
		addr++;
	};
	if (*addr != '#' || addr == domain || isspace((unsigned char)addr[-1]))
		tith_logError("Invalid domain");
	addr++;
	long zone = validateAddrPart(&addr);
	if (zone < -1 || zone == 0)
		tith_logError("Invalid zone");
	if (zone > INT16_MAX)
		tith_logError("Invalid zone");
	checkComponent(&addr, zone, ':', INT16_MAX);
	checkComponent(&addr, zone, '/', INT16_MAX);
	checkComponent(&addr, zone, '.', UINT16_MAX);
	if (*addr)
		tith_logError("Garbage at end");
}

void
tith_validateAddressValue(const struct TITH_TLV *address)
{
	if (address == NULL || address->length > SIZE_MAX)
		tith_logError("Address is too large");
	char *value = tith_memDup(address->value, (size_t)address->length);
	tith_pushAlloc(value);
	tith_validateAddress(value);
	tith_popAlloc();
	free(value);
}

bool
tith_isUnlistedAddressString(const char *address)
{
	size_t len = strlen(address);
	return len >= 4 && strcmp(&address[len - 3], "#-1") == 0;
}

bool
tith_isUnlistedAddress(const struct TITH_TLV *address)
{
	if (address == NULL || address->length < 4 || address->length > SIZE_MAX)
		return false;
	size_t len = (size_t)address->length;
	return address->value[len - 3] == '#' &&
	    address->value[len - 2] == '-' && address->value[len - 1] == '1';
}

const struct TITH_TLV *
tith_getAddressPublicKey(const struct TITH_TLV *address)
{
	tith_validateAddressValue(address);
	if (!tith_isUnlistedAddress(address)) {
		if (address->next && address->next->type == TITH_PublicKey)
			tith_logError("PublicKey supplied for listed address");
		return NULL;
	}
	const struct TITH_TLV *publicKey = address->next;
	if (publicKey == NULL || publicKey->type != TITH_PublicKey)
		tith_logError("Unlisted address is not followed by PublicKey");
	if (publicKey->length != hydro_sign_PUBLICKEYBYTES)
		tith_logError("PublicKey has wrong size");
	return publicKey;
}

static void
getOriginPublicKey(const struct TITH_TLV *origin,
    uint8_t pk[hydro_sign_PUBLICKEYBYTES])
{
	const struct TITH_TLV *publicKey = tith_getAddressPublicKey(origin);
	if (publicKey)
		memcpy(pk, publicKey->value, hydro_sign_PUBLICKEYBYTES);
	else
		tith_configGetPublicKey(origin, pk);
}

static struct TITH_Node *
getSigningNode(const struct TITH_TLV *origin)
{
	const struct TITH_TLV *publicKey = tith_getAddressPublicKey(origin);
	struct TITH_Node *node = tith_getNode(cfg, origin);
	if (node == NULL)
		tith_logError("Origin not configured");
	if (!node->hasSecretKey)
		tith_logError("No secret key for Origin");
	if (publicKey && memcmp(publicKey->value, node->kp.pk,
	    hydro_sign_PUBLICKEYBYTES) != 0)
		tith_logError("Origin PublicKey does not match configured key");
	return node;
}

void
tith_readBundleHeader(struct TITH_BundleHeader *bundle)
{
	memset(bundle, 0, sizeof(*bundle));
	tith_getTLV();
	if (tith_TLV->type != TITH_Origin)
		tith_logError("Bundle does not begin with Origin");
	bundle->origin = tith_TLV;
	tith_validateAddressValue(bundle->origin);

	struct TITH_TLV *tail = bundle->origin;
	if (tith_isUnlistedAddress(bundle->origin)) {
		tail = tith_getNextTLV(tail, TITH_PublicKey, true);
		bundle->originPublicKey = tail;
		(void)tith_getAddressPublicKey(bundle->origin);
	}
	else {
		struct TITH_TLV *next = tith_getNextTLV(tail,
		    TITH_PublicKey, false);
		if (next != tail)
			tith_logError("PublicKey supplied for listed Bundle Origin");
	}

	bundle->header = tith_getNextTLV(tail, TITH_SignedTLV, true);
	tith_parseTLV(bundle->header);
	struct TITH_TLV *signedData = bundle->header->child;
	if (signedData == NULL || signedData->type != TITH_SignedData ||
	    signedData->next == NULL ||
	    signedData->next->type != TITH_Signature ||
	    signedData->next->next != NULL)
		tith_logError("Invalid Header SignedTLV");

	tith_parseTLV(signedData);
	bundle->destination = signedData->child;
	if (bundle->destination == NULL ||
	    bundle->destination->type != TITH_Destination)
		tith_logError("Bundle Header does not begin with Destination");
	bundle->destinationPublicKey =
	    tith_getAddressPublicKey(bundle->destination);
	bundle->timestamp = bundle->destinationPublicKey ?
	    bundle->destinationPublicKey->next : bundle->destination->next;
	if (bundle->timestamp == NULL ||
	    bundle->timestamp->type != TITH_Timestamp ||
	    bundle->timestamp->next != NULL)
		tith_logError("Invalid Bundle Header");
	(void)tith_getNumberValue(bundle->timestamp);
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

void
tith_allocDataTLV(int type, uint64_t len, const void *data)
{
	if (tith_TLV)
		tith_logError("Attempting to alloc a second root TLV");
	tith_TLV = allocTLVBuffer(type, len, NULL, NULL);
	if (len)
		memcpy(tith_TLV->value, data, (size_t)len);
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
encodeNumber(uint64_t num, uint8_t buf[10])
{
	unsigned used = lengthLen(num);
	for (unsigned pos = 0; pos < used; pos++) {
		unsigned shift = 7 * (used - pos - 1);
		buf[pos] = (uint8_t)((num >> shift) & 0x7f);
		if (pos + 1 < used)
			buf[pos] |= 0x80;
	}
	return used;
}

static unsigned
typeLen(int num)
{
	if (num < 0)
		tith_logError("Negative type value");
	return lengthLen((uint64_t)num);
}

static struct TITH_TLV *
addRaw(struct TITH_TLV *tlv, int type, uint64_t len, bool child)
{
	if (child && tlv->child)
		tith_logError("TLV already has a child");
	if (!child && tlv->next)
		tith_logError("TLV already has a next item");
	struct TITH_TLV *newTlv = allocTLVBuffer(type, len, child ? NULL : tlv->first, child ? tlv : tlv->parent);
	newTlv->added = true;
	if (child)
		tlv->child = newTlv;
	else
		tlv->next = newTlv;
	return newTlv;
}

static void
growParents(struct TITH_TLV *tlv, uint64_t len)
{
	for (struct TITH_TLV *parent = tlv->parent; parent; parent = parent->parent) {
		uint64_t oldplen = lengthLen(parent->length);
		parent->length += len;
		uint64_t newplen = lengthLen(parent->length);
		len += newplen - oldplen;
	}
}

struct TITH_TLV *
tith_addData(struct TITH_TLV *tlv, int type, uint64_t len, void *data, bool child)
{
	struct TITH_TLV *newTlv = addRaw(tlv, type, len, child);
	memcpy(newTlv->value, data, len);
	uint64_t addLen = (uint64_t)len + typeLen(type) + lengthLen(len);
	growParents(newTlv, addLen);
	return newTlv;
}

struct TITH_TLV *
tith_addNullData(struct TITH_TLV *tlv, int type, uint64_t len, bool child)
{
	struct TITH_TLV *newTlv = addRaw(tlv, type, len, child);
	memset(newTlv->value, 0, len);
	uint64_t addLen = (uint64_t)len + typeLen(type) + lengthLen(len);
	growParents(newTlv, addLen);
	return newTlv;
}

struct TITH_TLV *
tith_addContainer(struct TITH_TLV *tlv, int type, bool child)
{
	struct TITH_TLV *newTlv = addRaw(tlv, type, 0, child);
	newTlv->value = NULL;
	uint64_t addLen = typeLen(type) + lengthLen(0);
	growParents(newTlv, addLen);
	return newTlv;
}

struct TITH_TLV *
tith_addNumber(struct TITH_TLV *tlv, int type, uint64_t value, bool child)
{
	uint8_t buf[10];
	unsigned len = encodeNumber(value, buf);
	return tith_addData(tlv, type, len, buf, child);
}

struct TITH_TLV *
tith_addSignedNumber(struct TITH_TLV *tlv, int type, int64_t value, bool child)
{
	uint64_t mapped;
	if (value >= 0)
		mapped = (uint64_t)value << 1;
	else
		mapped = ((uint64_t)(-(value + 1)) << 1) | 1;
	return tith_addNumber(tlv, type, mapped, child);
}

static size_t
putNumber(uint8_t *buffer, size_t offset, uint64_t value)
{
	uint8_t encoded[10];
	unsigned length = encodeNumber(value, encoded);
	memcpy(&buffer[offset], encoded, length);
	return offset + length;
}

static size_t
putContainedTLV(uint8_t *buffer, size_t offset, int type, uint64_t length,
    const void *value)
{
	offset = putNumber(buffer, offset, (uint64_t)type);
	offset = putNumber(buffer, offset, length);
	if (length > SIZE_MAX)
		tith_logError("Contained TLV is too large");
	memcpy(&buffer[offset], value, (size_t)length);
	return offset + (size_t)length;
}

struct TITH_TLV *
tith_addVia(struct TITH_TLV *tlv, const char *address,
    const uint8_t publicKey[hydro_sign_PUBLICKEYBYTES], uint64_t timestamp,
    const char *program, bool child)
{
	if (address == NULL || program == NULL)
		tith_logError("NULL value supplied for Via");
	tith_validateAddress(address);
	bool unlisted = tith_isUnlistedAddressString(address);
	if (unlisted && publicKey == NULL)
		tith_logError("No PublicKey for unlisted Via");
	if (!unlisted && publicKey != NULL)
		tith_logError("PublicKey supplied for listed Via");

	size_t addressLength = strlen(address);
	size_t programLength = strlen(program);
	validateUTF8((const uint8_t *)program, programLength);
	uint8_t encodedTimestamp[10];
	unsigned timestampLength = encodeNumber(timestamp, encodedTimestamp);

	uint64_t length = (uint64_t)typeLen(TITH_Address) +
	    lengthLen((uint64_t)addressLength) + (uint64_t)addressLength;
	if (unlisted) {
		uint64_t keyLength = (uint64_t)typeLen(TITH_PublicKey) +
		    lengthLen(hydro_sign_PUBLICKEYBYTES) +
		    hydro_sign_PUBLICKEYBYTES;
		if (length > UINT64_MAX - keyLength)
			tith_logError("Via is too large");
		length += keyLength;
	}
	uint64_t timestampTLVLength = (uint64_t)typeLen(TITH_Timestamp) +
	    lengthLen(timestampLength) + timestampLength;
	if (length > UINT64_MAX - timestampTLVLength ||
	    length + timestampTLVLength > UINT64_MAX - programLength)
		tith_logError("Via is too large");
	length += timestampTLVLength + programLength;
	if (length > SIZE_MAX)
		tith_logError("Via is too large");

	uint8_t *value = malloc((size_t)length);
	tith_pushAlloc(value);
	size_t offset = putContainedTLV(value, 0, TITH_Address,
	    (uint64_t)addressLength, address);
	if (unlisted)
		offset = putContainedTLV(value, offset, TITH_PublicKey,
		    hydro_sign_PUBLICKEYBYTES, publicKey);
	offset = putContainedTLV(value, offset, TITH_Timestamp,
	    timestampLength, encodedTimestamp);
	memcpy(&value[offset], program, programLength);
	offset += programLength;
	if (offset != (size_t)length)
		tith_logError("Via length mismatch");
	struct TITH_TLV *ret = tith_addData(tlv, TITH_Via, length, value,
	    child);
	tith_popAlloc();
	free(value);
	return ret;
}

struct TITH_TLV *
tith_allocBundleOrigin(const char *address)
{
	tith_validateAddress(address);
	tith_allocDataTLV(TITH_Origin, strlen(address), address);
	if (!tith_isUnlistedAddress(tith_TLV))
		return tith_TLV;
	struct TITH_Node *node = tith_getNode(cfg, tith_TLV);
	if (node == NULL)
		tith_logError("Origin not configured");
	if (!node->hasSecretKey)
		tith_logError("No secret key for Origin");
	return tith_addData(tith_TLV, TITH_PublicKey,
	    hydro_sign_PUBLICKEYBYTES, node->kp.pk, false);
}

struct TITH_TLV *
tith_addBundleHeader(struct TITH_TLV *tail, const char *destination,
    const uint8_t destinationPublicKey[hydro_sign_PUBLICKEYBYTES])
{
	tith_validateAddress(destination);
	bool unlisted = tith_isUnlistedAddressString(destination);
	if (unlisted && destinationPublicKey == NULL)
		tith_logError("No PublicKey for unlisted Bundle Destination");
	if (!unlisted && destinationPublicKey != NULL)
		tith_logError("PublicKey supplied for listed Bundle Destination");

	time_t now = time(NULL);
	if (now < 0)
		tith_logError("Unable to create Bundle Timestamp");
	struct TITH_TLV *header = tith_addContainer(tail,
	    TITH_SignedTLV, false);
	struct TITH_TLV *signedData = tith_addContainer(header,
	    TITH_SignedData, true);
	struct TITH_TLV *headerTail = tith_addData(signedData,
	    TITH_Destination, strlen(destination), (void *)destination, true);
	if (unlisted)
		headerTail = tith_addData(headerTail, TITH_PublicKey,
		    hydro_sign_PUBLICKEYBYTES, (void *)destinationPublicKey, false);
	tith_addNumber(headerTail, TITH_Timestamp, (uint64_t)now, false);
	tith_addNullData(signedData, TITH_Signature, hydro_sign_BYTES, false);
	tith_prepareSignedTLV(header);
	return header;
}

uint64_t
tith_getNumberValue(const struct TITH_TLV *tlv)
{
	uint64_t offset = 0;
	uint64_t ret = parseNumber(tlv->value, &offset, tlv->length);
	if (offset != tlv->length)
		tith_logError("Extra data after encoded number");
	return ret;
}

int64_t
tith_getSignedNumberValue(const struct TITH_TLV *tlv)
{
	uint64_t mapped = tith_getNumberValue(tlv);
	if (!(mapped & 1))
		return (int64_t)(mapped >> 1);
	uint64_t magnitude = (mapped >> 1) + 1;
	if (magnitude == (UINT64_C(1) << 63))
		return INT64_MIN;
	return -(int64_t)magnitude;
}

struct TITH_TLV *
tith_addFile(struct TITH_TLV *tlv, int type, const char *filename, bool child)
{
	struct TITH_TLV *newTlv = addRaw(tlv, type, 0, child);
	newTlv->value = NULL;
	newTlv->fileName = tith_strDup(filename);
	long len = tith_flen(filename);
	if (len < 0)
		tith_logError("Unable to get file length");
	newTlv->length = (uint64_t)len;
	uint64_t addLen = (uint64_t)len + typeLen(type) + lengthLen((uint64_t)len);
	growParents(newTlv, addLen);
	return newTlv;
}

struct ByteSink {
	void *state;
	void (*update)(void *state, const uint8_t *buf, size_t len);
};

static void
feedNumber(struct ByteSink *sink, uint64_t num)
{
	uint8_t buf[10];
	unsigned len = encodeNumber(num, buf);
	sink->update(sink->state, buf, len);
}

static void feedTLVSequence(struct ByteSink *sink, const struct TITH_TLV *tlv);

static void
feedFile(struct ByteSink *sink, const char *fname, uint64_t len)
{
	FILE *fp = fopen(fname, "rb");
	tith_pushFile(fp);
	uint64_t remain = len;
	while (remain) {
		uint8_t buf[1024];
		size_t bytes = remain > sizeof(buf) ? sizeof(buf) : (size_t)remain;
		if (fread(buf, 1, bytes, fp) != bytes)
			tith_logError("Failed to read file");
		sink->update(sink->state, buf, bytes);
		remain -= bytes;
	}
	tith_popFile();
	fclose(fp);
}

static void
feedTLVValue(struct ByteSink *sink, const struct TITH_TLV *tlv)
{
	if (tlv->length > SIZE_MAX)
		tith_logError("TLV too large");
	if (tlv->value)
		sink->update(sink->state, tlv->value, (size_t)tlv->length);
	else if (tlv->fileName)
		feedFile(sink, tlv->fileName, tlv->length);
	else
		feedTLVSequence(sink, tlv->child);
}

static void
feedTLVSequence(struct ByteSink *sink, const struct TITH_TLV *tlv)
{
	for (; tlv; tlv = tlv->next) {
		feedNumber(sink, (uint64_t)tlv->type);
		feedNumber(sink, tlv->length);
		feedTLVValue(sink, tlv);
	}
}

static void
feedTLVRange(struct ByteSink *sink, const struct TITH_TLV *tlv,
    const struct TITH_TLV *end)
{
	for (; tlv && tlv != end; tlv = tlv->next) {
		feedNumber(sink, (uint64_t)tlv->type);
		feedNumber(sink, tlv->length);
		feedTLVValue(sink, tlv);
	}
	if (tlv != end)
		tith_logError("Signature is not in its enclosing item");
}

static void
signUpdate(void *state, const uint8_t *buf, size_t len)
{
	if (hydro_sign_update(state, buf, len) != 0)
		tith_logError("Unable to update Signature");
}

static void
hashUpdate(void *state, const uint8_t *buf, size_t len)
{
	if (hydro_hash_update(state, buf, len) != 0)
		tith_logError("Unable to update TLVHash");
}

void
tith_prepareSignedTLV(struct TITH_TLV *tlv)
{
	if (tlv->type != TITH_SignedTLV || !tlv->added)
		tith_logError("Attempt to prepare something other than an added SignedTLV");
	struct TITH_TLV *sdata = tlv->child;
	if (sdata && sdata->type == TITH_Origin)
		sdata = sdata->next;
	if (sdata == NULL || sdata->type != TITH_SignedData)
		tith_logError("SignedTLV does not contain SignedData");
	struct TITH_TLV *signature = sdata->next;
	if (signature == NULL || signature->type != TITH_Signature || signature->next)
		tith_logError("SignedTLV does not end with one Signature");
	if (signature->length != hydro_sign_BYTES || signature->value == NULL)
		tith_logError("Signature has wrong size");
	if (sdata->signaturePrepared)
		return;

	struct TITH_TLV *origin = findOrigin(sdata);
	struct TITH_Node *node = getSigningNode(origin);

	hydro_sign_state state;
	if (hydro_sign_init(&state, "SignTLV") != 0)
		tith_logError("Unable to initialize Signature");
	struct ByteSink sink = {&state, signUpdate};
	feedTLVSequence(&sink, sdata->child);
	if (hydro_sign_final_create(&state, signature->value, node->kp.sk) != 0)
		tith_logError("Unable to create Signature");
	sdata->signaturePrepared = true;
}

void
tith_hashTLV(const struct TITH_TLV *tlv, uint8_t hash[hydro_hash_BYTES])
{
	if (tlv == NULL)
		tith_logError("Attempt to hash NULL TLV");
	hydro_hash_state state;
	if (hydro_hash_init(&state, "HashTLV", NULL) != 0)
		tith_logError("Unable to initialize TLVHash");
	struct ByteSink sink = {&state, hashUpdate};
	feedNumber(&sink, (uint64_t)tlv->type);
	feedNumber(&sink, tlv->length);
	feedTLVValue(&sink, tlv);
	if (hydro_hash_final(&state, hash, hydro_hash_BYTES) != 0)
		tith_logError("Unable to finish TLVHash");
}

static struct TITH_TLV *
findItemSignature(struct TITH_TLV *tlv)
{
	for (struct TITH_TLV *child = tlv->child; child; child = child->next) {
		if (child->type == TITH_Signature)
			return child;
	}
	return NULL;
}

static struct TITH_TLV *
requireChildType(struct TITH_TLV *child, int type, const char *error)
{
	if (child == NULL || child->type != type)
		tith_logError(error);
	return child->next;
}

static bool
isUTF8Continuation(uint8_t ch)
{
	return (ch & 0xc0) == 0x80;
}

static void
validateUTF8(const uint8_t *value, size_t length)
{
	size_t pos = 0;
	while (pos < length) {
		uint8_t first = value[pos++];
		if (first < 0x80)
			continue;
		if (first >= 0xc2 && first <= 0xdf) {
			if (pos >= length || !isUTF8Continuation(value[pos]))
				tith_logError("Invalid UTF-8 string");
			pos++;
			continue;
		}
		if (first >= 0xe0 && first <= 0xef) {
			if (length - pos < 2 ||
			    !isUTF8Continuation(value[pos]) ||
			    !isUTF8Continuation(value[pos + 1]))
				tith_logError("Invalid UTF-8 string");
			if ((first == 0xe0 && value[pos] < 0xa0) ||
			    (first == 0xed && value[pos] >= 0xa0))
				tith_logError("Invalid UTF-8 string");
			pos += 2;
			continue;
		}
		if (first >= 0xf0 && first <= 0xf4) {
			if (length - pos < 3 ||
			    !isUTF8Continuation(value[pos]) ||
			    !isUTF8Continuation(value[pos + 1]) ||
			    !isUTF8Continuation(value[pos + 2]))
				tith_logError("Invalid UTF-8 string");
			if ((first == 0xf0 && value[pos] < 0x90) ||
			    (first == 0xf4 && value[pos] >= 0x90))
				tith_logError("Invalid UTF-8 string");
			pos += 3;
			continue;
		}
		tith_logError("Invalid UTF-8 string");
	}
}

static void
readContainedTLV(const struct TITH_TLV *container, uint64_t *offset,
    struct TITH_TLV *value)
{
	if (*offset >= container->length)
		tith_logError("Missing value in Via");
	memset(value, 0, sizeof(*value));
	value->type = parseType(container->value, offset, container->length);
	value->length = parseNumber(container->value, offset,
	    container->length);
	if (value->length > container->length - *offset)
		tith_logError("TLV length exceeds Via");
	value->value = &container->value[*offset];
	*offset += value->length;
}

static void
validateVia(struct TITH_TLV *via)
{
	uint64_t offset = 0;
	struct TITH_TLV address;
	struct TITH_TLV publicKey;
	struct TITH_TLV timestamp;
	readContainedTLV(via, &offset, &address);
	if (address.type != TITH_Address)
		tith_logError("Via does not begin with Address");

	readContainedTLV(via, &offset, &timestamp);
	if (timestamp.type == TITH_PublicKey) {
		publicKey = timestamp;
		readContainedTLV(via, &offset, &timestamp);
		address.next = &publicKey;
		publicKey.next = &timestamp;
	}
	else
		address.next = &timestamp;

	(void)tith_getAddressPublicKey(&address);
	if (timestamp.type != TITH_Timestamp)
		tith_logError("Via has no Timestamp");
	(void)tith_getNumberValue(&timestamp);
	if (via->length - offset > SIZE_MAX)
		tith_logError("Via string is too large");
	validateUTF8(&via->value[offset],
	    (size_t)(via->length - offset));
}

static void
validateArea(struct TITH_TLV *area)
{
	tith_parseTLV(area);
	struct TITH_TLV *child = area->child;
	child = requireChildType(child, TITH_AreaName, "Area has no AreaName");
	if (child && child->type == TITH_AreaDescription)
		child = child->next;
	if (child)
		tith_logError("Invalid value in Area");
}

static void
validateMessage(struct TITH_TLV *message)
{
	if (!message->parsed && !message->added)
		tith_parseTLV(message);
	struct TITH_TLV *child = message->child;
	if (child == NULL || child->type != TITH_Origin)
		tith_logError("Message does not begin with Origin");
	const struct TITH_TLV *publicKey = tith_getAddressPublicKey(child);
	child = publicKey ? publicKey->next : child->next;

	bool netmail = child && child->type == TITH_Destination;
	if (netmail) {
		publicKey = tith_getAddressPublicKey(child);
		child = publicKey ? publicKey->next : child->next;
	}
	struct TITH_TLV *timestamp = child;
	child = requireChildType(child, TITH_Timestamp,
	    "Message has no Timestamp");
	(void)tith_getNumberValue(timestamp);
	child = requireChildType(child, TITH_ToUserName,
	    "Message has no ToUserName");
	child = requireChildType(child, TITH_FromUserName,
	    "Message has no FromUserName");
	child = requireChildType(child, TITH_Subject,
	    "Message has no Subject");
	child = requireChildType(child, TITH_MessageText,
	    "Message has no MessageText");

	bool echomail = child && child->type == TITH_Area;
	if (echomail) {
		validateArea(child);
		child = child->next;
	}
	if (netmail == echomail)
		tith_logError("Message is not exactly one of NetMail or EchoMail");

	while (child && child->type == TITH_File)
		child = child->next;
	const int optional[] = {
		TITH_LegacyAttributes, TITH_TimestampOffset, TITH_TearLine,
		TITH_OriginLine, TITH_MessageID, TITH_ReplyTo,
		TITH_OriginalCharacterSet
	};
	for (size_t i = 0; i < sizeof(optional) / sizeof(optional[0]); i++) {
		if (child && child->type == optional[i]) {
			if (child->type == TITH_LegacyAttributes)
				(void)tith_getNumberValue(child);
			else if (child->type == TITH_TimestampOffset)
				(void)tith_getSignedNumberValue(child);
			child = child->next;
		}
	}
	if (child == NULL || child->type != TITH_Signature ||
	    child->length != hydro_sign_BYTES)
		tith_logError("Message has invalid Signature");
	child = child->next;
	if (child == NULL || child->type != TITH_RequestIdentifier)
		tith_logError("Message has no RequestIdentifier");
	(void)tith_getNumberValue(child);
	child = child->next;
	if (child == NULL || child->type != TITH_Via)
		tith_logError("Message has no Via");
	while (child && child->type == TITH_Via) {
		validateVia(child);
		child = child->next;
	}
	if (child && child->type == TITH_SeenBy)
		child = child->next;
	while (child && child->type == TITH_AdditionalKludgeLine)
		child = child->next;
	if (child)
		tith_logError("Invalid value in Message");
}

static void
validateFile(struct TITH_TLV *file)
{
	if (!file->parsed && !file->added)
		tith_parseTLV(file);
	struct TITH_TLV *child = file->child;
	bool standalone = file->parent && file->parent->type == TITH_SignedData;

	if (child && child->type == TITH_Filename)
		child = child->next;
	if (child && child->type == TITH_Timestamp) {
		(void)tith_getNumberValue(child);
		child = child->next;
	}
	child = requireChildType(child, TITH_Contents, "File has no Contents");

	bool distribution = child && child->type == TITH_Area;
	if (distribution) {
		validateArea(child);
		child = child->next;
	}

	if (child && child->type == TITH_Origin) {
		const struct TITH_TLV *publicKey =
		    tith_getAddressPublicKey(child);
		child = publicKey ? publicKey->next : child->next;
	}
	else if (standalone)
		tith_logError("Standalone File has no Origin");

	const int optional[] = {
		TITH_ShortDescription, TITH_LongDescriptionLine,
		TITH_TearLine, TITH_MagicWord, TITH_Replaces
	};
	for (size_t i = 0; i < sizeof(optional) / sizeof(optional[0]); i++) {
		if (optional[i] == TITH_LongDescriptionLine) {
			while (child && child->type == optional[i])
				child = child->next;
		}
		else if (child && child->type == optional[i])
			child = child->next;
	}

	if (child && child->type == TITH_Signature) {
		if (child->length != hydro_sign_BYTES)
			tith_logError("File has invalid Signature");
		child = child->next;
	}
	else if (standalone)
		tith_logError("Standalone File has no Signature");

	if (child && child->type == TITH_RequestIdentifier) {
		(void)tith_getNumberValue(child);
		child = child->next;
	}
	else if (standalone)
		tith_logError("Standalone File has no RequestIdentifier");

	if (distribution) {
		if (child == NULL || child->type != TITH_Via)
			tith_logError("Distribution File has no Via");
		while (child && child->type == TITH_Via) {
			validateVia(child);
			child = child->next;
		}
		if (child == NULL || child->type != TITH_SeenBy)
			tith_logError("Distribution File has no SeenBy");
		while (child && child->type == TITH_SeenBy)
			child = child->next;
	}
	if (child)
		tith_logError("Invalid value in File");
}

static bool
itemSignatureRequired(const struct TITH_TLV *tlv)
{
	if (tlv->type == TITH_Message)
		return true;
	return tlv->type == TITH_File && tlv->parent &&
	    tlv->parent->type == TITH_SignedData;
}

static struct TITH_TLV *
itemSignatureOrigin(struct TITH_TLV *tlv, struct TITH_TLV *signature)
{
	if (tlv->type == TITH_Message) {
		if (tlv->child == NULL || tlv->child->type != TITH_Origin)
			tith_logError("Message does not begin with Origin");
		return tlv->child;
	}
	return findOrigin(signature);
}

static void
prepareItemSignature(struct TITH_TLV *tlv, bool required)
{
	if (tlv->type != TITH_Message && tlv->type != TITH_File)
		tith_logError("Attempt to prepare Signature for invalid item type");
	if (!tlv->added)
		tith_logError("Attempt to prepare Signature for received item");
	if (tlv->type == TITH_Message)
		validateMessage(tlv);
	else
		validateFile(tlv);
	struct TITH_TLV *signature = findItemSignature(tlv);
	if (signature == NULL) {
		if (required)
			tith_logError("Item does not contain required Signature");
		return;
	}
	if (signature->length != hydro_sign_BYTES || signature->value == NULL)
		tith_logError("Item Signature has wrong size");
	if (signature->signaturePrepared)
		return;
	struct TITH_TLV *origin = itemSignatureOrigin(tlv, signature);
	struct TITH_Node *node = getSigningNode(origin);

	hydro_sign_state state;
	if (hydro_sign_init(&state, "SignTLV") != 0)
		tith_logError("Unable to initialize item Signature");
	struct ByteSink sink = {&state, signUpdate};
	feedTLVRange(&sink, tlv->child, signature);
	if (hydro_sign_final_create(&state, signature->value, node->kp.sk) != 0)
		tith_logError("Unable to create item Signature");
	signature->signaturePrepared = true;
}

void
tith_prepareItemSignature(struct TITH_TLV *tlv)
{
	prepareItemSignature(tlv, itemSignatureRequired(tlv));
}

static void
verifyItemSignature(struct TITH_TLV *tlv, bool required)
{
	if (!tlv->parsed && !tlv->added)
		tith_parseTLV(tlv);
	if (tlv->type == TITH_Message)
		validateMessage(tlv);
	else
		validateFile(tlv);
	struct TITH_TLV *signature = findItemSignature(tlv);
	if (signature == NULL) {
		if (required)
			tith_logError("Item does not contain required Signature");
		return;
	}
	if (signature->length != hydro_sign_BYTES)
		tith_logError("Item Signature has wrong size");
	struct TITH_TLV *origin = itemSignatureOrigin(tlv, signature);
	uint8_t pk[hydro_sign_PUBLICKEYBYTES];
	getOriginPublicKey(origin, pk);

	hydro_sign_state state;
	if (hydro_sign_init(&state, "SignTLV") != 0)
		tith_logError("Unable to initialize item Signature");
	struct ByteSink sink = {&state, signUpdate};
	feedTLVRange(&sink, tlv->child, signature);
	if (hydro_sign_final_verify(&state, signature->value, pk) != 0)
		tith_logError("Item Signature failed to validate");
}

void
tith_getItemIdentity(struct TITH_TLV *tlv,
    uint8_t identity[hydro_hash_BYTES])
{
	if (tlv == NULL || (tlv->type != TITH_Message &&
	    tlv->type != TITH_File))
		tith_logError("Invalid item for duplicate identity");
	if (!itemSignatureRequired(tlv))
		tith_logError("Item has no standalone signed identity");
	verifyItemSignature(tlv, true);
	struct TITH_TLV *signature = findItemSignature(tlv);
	struct TITH_TLV *origin = itemSignatureOrigin(tlv, signature);
	uint8_t publicKey[hydro_sign_PUBLICKEYBYTES];
	getOriginPublicKey(origin, publicKey);

	hydro_hash_state state;
	if (hydro_hash_init(&state, "ItemIDv2", NULL) != 0)
		tith_logError("Unable to initialize item identity");
	struct ByteSink sink = {&state, hashUpdate};
	feedNumber(&sink, (uint64_t)tlv->type);
	feedNumber(&sink, origin->length);
	feedTLVValue(&sink, origin);
	hashUpdate(&state, publicKey, sizeof(publicKey));
	hashUpdate(&state, signature->value, hydro_sign_BYTES);
	if (hydro_hash_final(&state, identity, hydro_hash_BYTES) != 0)
		tith_logError("Unable to finish item identity");
}

void
tith_verifyItemSignatures(struct TITH_TLV *tlv)
{
	for (; tlv; tlv = tlv->next) {
		if (tlv->type == TITH_Message) {
			verifyItemSignature(tlv, true);
			for (struct TITH_TLV *child = tlv->child; child; child = child->next) {
				if (child->type == TITH_File)
					verifyItemSignature(child, false);
			}
		}
		else if (tlv->type == TITH_File)
			verifyItemSignature(tlv, itemSignatureRequired(tlv));
	}
}

static void
prepareTLVTree(struct TITH_TLV *tlv)
{
	for (; tlv; tlv = tlv->next) {
		if (tlv->child)
			prepareTLVTree(tlv->child);
		if ((tlv->type == TITH_Message || tlv->type == TITH_File) && tlv->added)
			tith_prepareItemSignature(tlv);
		if (tlv->type == TITH_SignedTLV && tlv->added)
			tith_prepareSignedTLV(tlv);
	}
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
		if (!sendByte(tith_handle, b))
			tith_logError("Failed to send TLV");
	}
	return used;
}

static uint64_t
sendBuffer(uint8_t *buf, uint64_t len)
{
	if (len > (uint64_t)SIZE_MAX)
		tith_logError("TLV too large");
	if (len && !sendBytes(tith_handle, buf, (size_t)len))
		tith_logError("Failed to send TLV");
	return len;
}

static uint64_t
sendFile(const char *fname, uint64_t len)
{
	FILE *fp = fopen(fname, "rb");
	tith_pushFile(fp);
	uint64_t remain = len;
	for (;;) {
		uint8_t buf[1024];
		size_t bytes = remain > sizeof(buf) ? sizeof(buf) : (size_t)remain;
		if (bytes == 0)
			break;
		size_t ret = fread(buf, 1, bytes, fp);
		if (ret != bytes)
			tith_logError("Failed to read file");
		sendBuffer(buf, ret);
		remain -= ret;
	}
	tith_popFile();
	fclose(fp);
	return len;
}

static uint64_t
sendTLV(struct TITH_TLV *firstTLV)
{
	if (firstTLV == NULL)
		tith_logError("Attempting to send NULL TLV");
	uint64_t used = 0;
	for (struct TITH_TLV *tlv = firstTLV; tlv; tlv = tlv->next) {
		used += sendNumber((uint64_t)tlv->type);
		used += sendNumber(tlv->length);
		// If we have the value in memory, send it...
		if (tlv->value)
			used += sendBuffer(tlv->value, tlv->length);
		// If it's in a file, send that...
		else if (tlv->fileName)
			used += sendFile(tlv->fileName, tlv->length);
		// Otherwise, it must have children... send those
		else {
			uint64_t TLVlen = sendTLV(tlv->child);
			if (TLVlen != tlv->length)
				tith_logError("TLV Length mismatch");
			used += TLVlen;
		}
	}
	return used;
}

static uint64_t
encodedSequenceLength(const struct TITH_TLV *tlv)
{
	uint64_t ret = 0;
	for (; tlv; tlv = tlv->next) {
		uint64_t overhead = typeLen(tlv->type) + lengthLen(tlv->length);
		if (tlv->length > UINT64_MAX - overhead ||
		    ret > UINT64_MAX - overhead - tlv->length)
			tith_logError("Encoded TLV sequence is too large");
		ret += overhead + tlv->length;
	}
	return ret;
}

/*
 * Sends the root TLV
 */
void
tith_sendTLV(void)
{
	prepareTLVTree(tith_TLV);
	if (sendTLV(tith_TLV) != encodedSequenceLength(tith_TLV))
		tith_logError("Length mismatch!");
	if (!flushWrite(tith_handle))
		tith_logError("Failed to flush TLV");
}

void
tith_validateTLV(struct TITH_TLV *tlv, int command, int numargs, ...)
{
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
	va_end(ap);
}

void
tith_logf(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	va_list copy;
	va_copy(copy, ap);
	int len = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	if (len <= 0) {
		va_end(ap);
		return;
	}
	char *buf = malloc((size_t)len + 1);
	tith_pushAlloc(buf);
	if (vsnprintf(buf, (size_t)len + 1, format, ap) > 0)
		logString(buf);
	va_end(ap);
	tith_popAlloc();
	free(buf);
}
