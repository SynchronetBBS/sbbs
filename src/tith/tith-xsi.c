#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <unistd.h>

#include "tith-common.h"
#include "tith-interface.h"

static const uint8_t receiptValue[] = "TITHID2\n";

/*
 * Generates a context for reading directory contents of the passed
 * path.  The passed path must be to a directory.
 * 
 * Returns NULL on failure.
 */
void *
openDirectory(const char *path)
{
	return opendir(path);
}

/*
 * Reads the next entry from a context returned by openDirectory()
 * 
 * The returned entry does not include the path, and must remain valid
 * until the next call to either readDirectory() or closeDirectory() on
 * the directory handle dhandle
 * 
 * returns NULL when the end of the directory is reached or an error
 * occurs
 */
const char *
readDirectory(void *dhandle)
{
	struct dirent *de = readdir((DIR *)dhandle);
	if (de == NULL)
		return NULL;
	return de->d_name;
}

/*
 * Frees all resources associated with the directory handle and makes
 * future calls to readDirectory() with the specified handle return NULL
 * (unless the handle is returned from a later call to openDirectory).
 */
void
closeDirectory(void *dhandle)
{
	closedir((DIR *)dhandle);
}

bool isDir(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return false;
	if (S_ISDIR(st.st_mode))
		return true;
	return false;
}

static unsigned
encodedNumber(uint64_t value, uint8_t encoded[10])
{
	unsigned length = 1;
	uint64_t remaining = value;
	while (remaining > 127) {
		length++;
		remaining >>= 7;
	}
	for (unsigned pos = 0; pos < length; pos++) {
		unsigned shift = 7 * (length - pos - 1);
		encoded[pos] = (uint8_t)((value >> shift) & 0x7f);
		if (pos + 1 < length)
			encoded[pos] |= 0x80;
	}
	return length;
}

static bool
writeAll(int fd, const uint8_t *value, size_t length)
{
	while (length > 0) {
		ssize_t written = write(fd, value, length);
		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			return false;
		size_t count = (size_t)written;
		value += count;
		length -= count;
	}
	return true;
}

static bool
readAll(int fd, uint8_t *value, size_t length)
{
	while (length > 0) {
		ssize_t received = read(fd, value, length);
		if (received < 0 && errno == EINTR)
			continue;
		if (received <= 0)
			return false;
		size_t count = (size_t)received;
		value += count;
		length -= count;
	}
	return true;
}

static bool
lockFile(int fd)
{
	struct flock lock;
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	for (;;) {
		if (fcntl(fd, F_SETLKW, &lock) == 0)
			return true;
		if (errno != EINTR)
			return false;
	}
}

static void
identityName(char name[65],
    const uint8_t identity[TITH_ITEM_IDENTITY_BYTES])
{
	static const char hex[] = "0123456789abcdef";
	for (size_t i = 0; i < TITH_ITEM_IDENTITY_BYTES; i++) {
		name[i * 2] = hex[identity[i] >> 4];
		name[i * 2 + 1] = hex[identity[i] & 0x0f];
	}
	name[64] = 0;
}

static bool
readNumber(int fd, uint64_t *value, size_t *length)
{
	uint64_t result = 0;
	bool first = true;
	*length = 0;
	for (;;) {
		uint8_t ch;
		if (!readAll(fd, &ch, 1))
			return false;
		if (first && (ch & 0x80) && !(ch & 0x7f))
			return false;
		first = false;
		if (result > (UINT64_MAX >> 7))
			return false;
		result = (result << 7) | (ch & 0x7f);
		(*length)++;
		if (!(ch & 0x80))
			break;
	}
	*value = result;
	return true;
}

static int
validateStoredItem(int fd, int itemType)
{
	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < 0)
		return -1;
	if (lseek(fd, 0, SEEK_SET) < 0)
		return -1;
	uint64_t storedType;
	uint64_t storedLength;
	size_t typeLength;
	size_t lengthLength;
	if (!readNumber(fd, &storedType, &typeLength) ||
	    !readNumber(fd, &storedLength, &lengthLength) ||
	    storedType != (uint64_t)itemType)
		return -1;
	uint64_t total = (uint64_t)typeLength + lengthLength;
	if (storedLength > UINT64_MAX - total)
		return -1;
	total += storedLength;
	if ((uintmax_t)st.st_size != total)
		return -1;
	return 1;
}

static bool
validateReceipt(int fd)
{
	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < 0 ||
	    (uintmax_t)st.st_size != sizeof(receiptValue) - 1 ||
	    lseek(fd, 0, SEEK_SET) < 0)
		return false;
	uint8_t stored[sizeof(receiptValue) - 1];
	return readAll(fd, stored, sizeof(stored)) &&
	    memcmp(stored, receiptValue, sizeof(stored)) == 0;
}

static enum TITH_StoreResult
storeSignedItemLocked(void *handle, const char *inbound,
    const uint8_t identity[TITH_ITEM_IDENTITY_BYTES], int itemType,
    const uint8_t *itemValue, uint64_t itemLength)
{
	(void)handle;
	if (inbound == NULL || identity == NULL || itemValue == NULL ||
	    (itemType != TITH_Message && itemType != TITH_File) ||
	    itemLength > SIZE_MAX)
		return TITH_STORE_FAILED;

	char base[65];
	char itemName[70];
	char lockName[70];
	char temporaryName[70];
	char receiptName[70];
	char receiptTemporaryName[70];
	identityName(base, identity);
	if (snprintf(itemName, sizeof(itemName), "%s.tith", base) < 0 ||
	    snprintf(lockName, sizeof(lockName), "%s.lock", base) < 0 ||
	    snprintf(temporaryName, sizeof(temporaryName), "%s.tmp", base) < 0 ||
	    snprintf(receiptName, sizeof(receiptName), "%s.seen", base) < 0 ||
	    snprintf(receiptTemporaryName, sizeof(receiptTemporaryName),
	        "%s.stmp", base) < 0)
		return TITH_STORE_FAILED;

	uint8_t prefix[20];
	unsigned typeLength = encodedNumber((uint64_t)itemType, prefix);
	unsigned lengthLength = encodedNumber(itemLength, &prefix[typeLength]);
	size_t prefixLength = (size_t)typeLength + lengthLength;

	int directory = open(inbound, O_RDONLY);
	if (directory < 0)
		return TITH_STORE_FAILED;
	struct stat directoryStatus;
	if (fstat(directory, &directoryStatus) != 0 ||
	    !S_ISDIR(directoryStatus.st_mode)) {
		close(directory);
		return TITH_STORE_FAILED;
	}

	int lock = openat(directory, lockName,
	    O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
	if (lock < 0 || !lockFile(lock)) {
		if (lock >= 0)
			close(lock);
		close(directory);
		return TITH_STORE_FAILED;
	}

	int receipt = openat(directory, receiptName, O_RDONLY | O_NOFOLLOW);
	if (receipt >= 0) {
		bool valid = validateReceipt(receipt);
		close(receipt);
		close(lock);
		close(directory);
		return valid ? TITH_STORE_DUPLICATE : TITH_STORE_FAILED;
	}
	if (errno != ENOENT) {
		close(lock);
		close(directory);
		return TITH_STORE_FAILED;
	}

	int existing = openat(directory, itemName, O_RDONLY | O_NOFOLLOW);
	if (existing >= 0) {
		int valid = validateStoredItem(existing, itemType);
		close(existing);
		if (valid < 0) {
			close(lock);
			close(directory);
			return TITH_STORE_FAILED;
		}
	}
	else if (errno == ENOENT) {
		int temporary = openat(directory, temporaryName,
		    O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
		bool written = temporary >= 0 &&
		    writeAll(temporary, prefix, prefixLength) &&
		    writeAll(temporary, itemValue, (size_t)itemLength) &&
		    fsync(temporary) == 0;
		if (temporary >= 0 && close(temporary) != 0)
			written = false;
		if (!written || renameat(directory, temporaryName, directory,
		    itemName) != 0) {
			(void)unlinkat(directory, temporaryName, 0);
			close(lock);
			close(directory);
			return TITH_STORE_FAILED;
		}
	}
	else {
		close(lock);
		close(directory);
		return TITH_STORE_FAILED;
	}
	if (fsync(directory) != 0) {
		close(lock);
		close(directory);
		return TITH_STORE_FAILED;
	}

	int receiptTemporary = openat(directory, receiptTemporaryName,
	    O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
	bool receiptWritten = receiptTemporary >= 0 &&
	    writeAll(receiptTemporary, receiptValue,
	        sizeof(receiptValue) - 1) && fsync(receiptTemporary) == 0;
	if (receiptTemporary >= 0 && close(receiptTemporary) != 0)
		receiptWritten = false;
	if (!receiptWritten || renameat(directory, receiptTemporaryName,
	    directory, receiptName) != 0 || fsync(directory) != 0) {
		(void)unlinkat(directory, receiptTemporaryName, 0);
		close(lock);
		close(directory);
		return TITH_STORE_FAILED;
	}

	close(lock);
	close(directory);
	return TITH_STORE_NEW;
}

static once_flag storeMutexOnce = ONCE_FLAG_INIT;
static mtx_t storeMutex;
static bool storeMutexReady;

static void
initializeStoreMutex(void)
{
	storeMutexReady = mtx_init(&storeMutex, mtx_plain) == thrd_success;
}

enum TITH_StoreResult
storeSignedItem(void *handle, const char *inbound,
    const uint8_t identity[TITH_ITEM_IDENTITY_BYTES], int itemType,
    const uint8_t *itemValue, uint64_t itemLength)
{
	call_once(&storeMutexOnce, initializeStoreMutex);
	if (!storeMutexReady || mtx_lock(&storeMutex) != thrd_success)
		return TITH_STORE_FAILED;
	enum TITH_StoreResult result = storeSignedItemLocked(handle, inbound,
	    identity, itemType, itemValue, itemLength);
	if (mtx_unlock(&storeMutex) != thrd_success)
		return TITH_STORE_FAILED;
	return result;
}
