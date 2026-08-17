#ifndef TITH_INTERFACE_HEADER
#define TITH_INTERFACE_HEADER

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * This header describes the interface that the TITH mailer uses
 * 
 * This can be implemented using platform-specific code.
 * See tith-stdio.c for an example implementation.
 *
 * TITH supports one active TITH_main() invocation per thread.  Separate
 * threads may run separate connections concurrently, provided libhydrogen
 * supplies thread-local state on the target platform.  A connection must
 * remain on the thread which called TITH_main() until that call returns.
 *
 * These callbacks are called synchronously on that thread.  They must not
 * re-enter TITH_main() or call other TITH routines.  Because callbacks for
 * different connections may execute concurrently, an implementation which
 * shares state between handles must provide the required synchronization.
 */

/*
 * This should wait for and return a single byte from the connection.
 * 
 * The return value should be the value of the byte cast to a uint8_t.
 * -1 should be returned if an error occurred and the connection is no
 * longer able to be read.
 */
int getByte(void *handle);

/*
 * This should wait for and return the specified number of bytes from
 * the connection.
 * 
 * It should return true if the requested number of bytes were read, and
 * false if an error occurred and the connection is no longer able to be
 * read.
 */
bool getBytes(void *handle, uint8_t *buf, size_t bufsz);

/*
 * This is called when no more bytes will be retreived from a
 * connection. This should cause getByte() to return -1 and getBytes() to
 * return false, but not impact sendByte() or sendBytes().
 */
void shutdownRead(void *handle);

/*
 * This should send or buffer a single byte to the connection and block
 * until it is sent/buffered.
 * 
 * It should return true if the byte was sent/buffered, and false if an
 * error occurred and the connection is no longer able to be written to.
 */
bool sendByte(void *handle, uint8_t ch);

/*
 * This should send/buffer the entire buffer to the connection, blocking
 * until it is sent/buffered.
 * 
 * It should return true if the requested number of bytes were
 * sent/buffered, and false if an error occurred and the connection is no
 * longer able to be written to.
 */
bool sendBytes(void *handle, uint8_t *buf, size_t bufsz);

/*
 * If bytes were buffered instead of sent by sendByte() and sendBytes(),
 * this causes them to be sent.
 * 
 * Returns false if an error occurred and the connection is no longer
 * able to be written to.
 */
bool flushWrite(void *handle);

/*
 * This is called when no more bytes will be sent on a connection. This
 * should cause sendByte() and sendBytes() to return false, but not impact
 * getByte() or getBytes().
 */
void shutdownWrite(void *handle);

/*
 * This indicates that all resources associated with the connection
 * should be freed, and the handle will not be used again.
 *
 * This callback is also used during error cleanup.  It must return without
 * calling TITH routines.  handle is the value originally passed to
 * TITH_main(), including NULL when NULL is meaningful to the implementation.
 */
void closeConnection(void *handle);

/*
 * This logs a string.
 */
void logString(const char *str);

/*
 * This is the entry point to the TITH code.
 * 
 * handle is a connection to the remote.  It is passed unchanged to the
 * callbacks above, including closeConnection() when argument or configuration
 * processing fails.
 */
int TITH_main(int argc, char **argv, void *handle);

/*
 * Generates a context for reading directory contents of the passed
 * path.  The passed path must be to a directory.
 * 
 * Returns NULL on failure.
 */
void *openDirectory(const char *path);

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
const char *readDirectory(void *dhandle);

/*
 * Frees all resources associated with the directory handle and makes
 * future calls to readDirectory() with the specified handle return NULL
 * (unless the handle is returned from a later call to openDirectory).
 */
void closeDirectory(void *dhandle);

/*
 * Returns true if path is a directory
 */
bool isDir(const char *path);

#define TITH_ITEM_IDENTITY_BYTES 32

enum TITH_StoreResult {
	TITH_STORE_FAILED,
	TITH_STORE_NEW,
	TITH_STORE_DUPLICATE
};

/*
 * Atomically stores a validated Message or standalone File and its
 * duplicate-acceptance identity.
 *
 * Before returning TITH_STORE_NEW, both the item and the identity MUST be
 * durable.  TITH_STORE_DUPLICATE means that this identity was made durable
 * by an earlier successful call; the item supplied by this call MUST NOT be
 * stored again.  Implementations MUST NOT expire identities automatically.
 * Calls may occur concurrently for the same identity and must be serialized.
 *
 * inbound is the configured inbound directory.  itemValue is the exact Value
 * of the item TLV; itemType and itemLength allow the callback to preserve the
 * complete item.  All pointers remain valid only for the duration of the
 * synchronous call.
 */
enum TITH_StoreResult storeSignedItem(void *handle, const char *inbound,
    const uint8_t identity[TITH_ITEM_IDENTITY_BYTES], int itemType,
    const uint8_t *itemValue,
    uint64_t itemLength);

#endif
